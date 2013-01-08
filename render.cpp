//
//  Render.cc
//  This implements the map renderer class, which draws the scene as defined
// by the map, contained in the WAD file, as seen from the current X,Y,Z coordinate
// and ViewAngle.
//
// Still to be done:
//
//       Implement (properly) the routines to render the floor and ceilings (Flats)
//       Implement (properly) bounding box checking.
//       Implement routines to render Things.
//       Fix scrolling texture bug.
//
//

#include <string.h>
#include <math.h>
#include "Render.h"
#include <fstream>

// #define USEBOUNDINGBOX
// #define DEBUG
const double PI = 3.141592654;


//
// This tracks the start and end of each run of the floor / ceiling.
//
struct floorRunStr
{
    short  start, end;
    short  height;
    char*  txtPointer;
}
;
struct colRunStr
{
    short start, end, Col;
    short height;
    char* txtPointer;
};

//
// Each entry in this array is a strip of texture that is to go on the
// floor or ceiling of the subsectors on the screen. Each screen row has
// an entry on the 1st subscript, and I have enough room on the 2nd for
// all of the possible runs for each scan line (row).
//
floorRunStr floorRuns[200][320];
short       maxFloorRun[200];

//
// Each entry in this array is a wall texture, sorted by depth (deepest first) by the BSP tree.
// The first index is the screen column number. The second is the order.
//
colRunStr wallRuns[320][100];
short     maxWallRuns[320];

//
// These track the maximum and minimum Y offsets (in screen coordinates)
// of the walls. This assists us in determining where the floor segments
// are to go.
//
unsigned short  maxY[200];
unsigned short  minY[200];

Renderer::Renderer( HWND hWnd, GameWad* Wad )
{
    // open the trace file.
#ifdef DEBUG
    // myTrace.open( "c:\\temp\\wadreader.out" );
#endif

    // Copy over the items we want to know about in the outside world.
    WadFile = Wad;
    ghWnd = hWnd;

    // Allocate the vertex cache.
    VerticesCache = new Vertex[WadFile->NbrVertices];
    BitMapInfo = NULL;

    // Initialize the lookup tables.
    initTables();

    // Initialize the double buffer DIB.
    init_double_buffer();
}

Renderer::~Renderer(void)
{
    // Clean up the vertex cache.
    if( NULL != VerticesCache)
    {
        delete VerticesCache;
        VerticesCache = NULL;
    }

    return;
}

short Renderer::initTables( void )
{
    double rads;
    int    i;

    // Generate the trig tables.
    for( i=0; i < 8192; ++i )
    {
        rads = (i * PI)/4096.0;
        SinTable[i] = (long) floor(::sin(rads) * 65536.0);
        CosTable[i] = (long) floor(::cos(rads) * 65536.0);
        TanTable[i] = (long) floor(::tan(rads) * 65536.0);
    }

    // Generate the column angle table (for texturing walls)
    for( i=0; i< 640; ++i )
    {
        colangle[i] = (short)((i-320) * 0x4000/960.0);
    }

    // Calculate the texture scaling table.
    for( i = 1; i <= 8193; ++i )
        DeltaTxtTbl[i-1] = 2097152l / i;

    return 1;
}

long Renderer::sin(short Ang)
{
    // This takes an angle (Binary Angle Measurement, or BAM), and returns the sine function from the table.
    //

     return SinTable[((unsigned short)Ang)>>3];
     //return (long)(65536.0 * ::sin(Ang * PI / (184.044444 * 180.0)));
}

long Renderer::cos(short Ang)
{
    // This takes a BAM angle, and returns the cosine, from the SinTable.
    //

     return CosTable[((unsigned short)Ang)>>3];
     //return (long)(65536.0 * ::cos(Ang * PI / (184.044444 * 180.0)));
}

long Renderer::tan(short Ang)
{
    // This takes a BAM angle, and returns the tangent, from the TanTable.
    //

     return TanTable[((unsigned short)Ang)>>3];
    // return (long)(65536.0 * ::tan(Ang * PI / (184.044444 * 180.0)));
}

short Renderer::invTan(long y, long x)
{
    // This will take the rise and run and calculate the angle (inverse tangent)


    // I used to use a table, but was having problems...
    // TODO: remove atan2 call and replace with table.

    return (short)((180.0 * atan2((float)y,(float)x) * 184.044444)/PI) ;
}

void Renderer::init_double_buffer(void)
{
    //
    // Initialize the double buffer and windows bitmaps that will be used to
    // render the double buffer.
    //
    HDC hDC;

    if ( NULL == BitMapInfo )
        BitMapInfo = (BITMAPINFO *)malloc(sizeof(BITMAPINFOHEADER) + (256 * sizeof(RGBQUAD)));

    hDC = GetDC(ghWnd);

    BitMapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    BitMapInfo->bmiHeader.biWidth = 640;
    BitMapInfo->bmiHeader.biHeight = -400;
    BitMapInfo->bmiHeader.biPlanes = 1;
    BitMapInfo->bmiHeader.biBitCount = 8;
    BitMapInfo->bmiHeader.biCompression = BI_RGB;
    BitMapInfo->bmiHeader.biSizeImage  = 0;
    BitMapInfo->bmiHeader.biXPelsPerMeter = 0;
    BitMapInfo->bmiHeader.biYPelsPerMeter = 0;
    BitMapInfo->bmiHeader.biClrUsed = 0;
    BitMapInfo->bmiHeader.biClrImportant = 0;

    //	Load the palette
    for (int i= 0; i < 256; ++i)
    {
        BitMapInfo->bmiColors[i].rgbRed = WadFile->GamePalettes[0][i*3];
        BitMapInfo->bmiColors[i].rgbGreen = WadFile->GamePalettes[0][i*3+1];
        BitMapInfo->bmiColors[i].rgbBlue = WadFile->GamePalettes[0][i*3+2];
    }
    if (DblBuff != NULL)
        DeleteObject(DblBuff);

    DblBuff = CreateDIBSection(hDC, (BITMAPINFO *)BitMapInfo, DIB_RGB_COLORS, (VOID **)&DoubleBuffer, 0, 0);

    ReleaseDC(ghWnd,hDC);
}

void Renderer::clearToColor(char c)
{
    //
    // This function clears te screen to the color "C", by blasting the
    // byte to the double buffer. This used to use a rep movsw, but now we're
    // in windows, and I don't know if that will work anymore.
    //
    memset(DoubleBuffer, c, 256000ul);
}

void Renderer::ChangePalette ( short PaletteNbr )
{
    //
    // This modifies the double buffers palette. It is used to select one of the 14 palettes
    // stored in the WAD file.
    //
    HDC hDC, hMemDC;
    HBITMAP DefaultBitmap;
     RGBQUAD tmpPalette[256];

    if ((PaletteNbr > 14) || (PaletteNbr < 1))
        return;

    hDC = GetDC(ghWnd);

    // Create memory device context compatible with the engines window dc
    hMemDC = CreateCompatibleDC(GetDC(ghWnd));

    // Save the default bitmap and select the engines double buffer into the memory dc
    DefaultBitmap = (HBITMAP)SelectObject(hMemDC, DblBuff);

    // Now get the palette for the selected bitmap.
    GetDIBColorTable(hMemDC, 0, 256, tmpPalette);

    // Modify the palette by the percentage.
    for (int i=0; i<256; ++i)
    {
        tmpPalette[i].rgbRed = (unsigned char )WadFile->GamePalettes[PaletteNbr][i*3];
        tmpPalette[i].rgbGreen = (unsigned char )WadFile->GamePalettes[PaletteNbr][i*3+1];
        tmpPalette[i].rgbBlue = (unsigned char )WadFile->GamePalettes[PaletteNbr][i*3+2];
    }

    // and now put it back into the bitmap.
    SetDIBColorTable(hMemDC, 0, 256, tmpPalette);

    // put back the default bitmap
    SelectObject(hMemDC, DefaultBitmap);

    // delete the memory dc
    DeleteDC(hMemDC);

    // delete the default bitmap
    DeleteObject(DefaultBitmap);

    // release the engines window dc
    ReleaseDC(ghWnd,hDC);

 }

void Renderer::copyscrn (char *Buffer)
{
    //
    // This method copies a loaded buffer (read PCX file) into the double buffer.
    //
     memcpy(DoubleBuffer, Buffer, 266000ul);
}

int Renderer::Save_Pcx(char *filename)
{
    return WadFile->Save_Pcx(filename, DoubleBuffer, 640, 400);
}

long Renderer::DistToPoint( long x, long y )
{
    long	dx;
    long	dy;
    long	dist;

    dx = abs(x);
    dy = abs(y);

    if( dy == 0 )
        dist = dx;
    else if (dx == 0)
        dist = dy;
    else
    {


    dist = (long)((double)sqrt ( (double)(dx * dx + dy * dy) ));

    //short ang = invTan(dy,dx);
    //dist2 = (dx * 65536l) / cos(ang);

    }

    return dist;
}

//
// Project the bounding box, and determine if the area is at least partially
// visible.
//
short Renderer::CheckBoundingBox ( short UpperX, short UpperY,
                                   short LowerX, short LowerY )
{
#ifdef USEBOUNDINGBOX

    // There will be 4 points associated with the bounding box:
    short P1x = UpperX - ViewerX;
    short P1y = UpperY - ViewerY;
    short P2x = P1x;
    short P2y = LowerY - ViewerY;
    short P3x = LowerX - ViewerX;
    short P3y = P2y;
    short P4x = P3x;
    short P4y = P1y;
    long  tempX;
    long  tempY;

#else

    return 1;

#endif


#ifdef USEBOUNDINGBOX

    // Step 2. Rotate the points.
    register long cosine = this->cos(-ViewerAngle);
    register long sine = this->sin(-ViewerAngle);

    //  Transform all vertices to view coordinates, and place new vertices in cache.
    // Now rotate the vertex so that the player is looking down the positive X axis.
    tempX = (long)P1x * cosine - (long)P1y * sine;
    tempY = (long)P1x * sine + (long)P1y * cosine;
    P1x = (short)(tempX >> 16);
    P1y = (short)(tempY >> 16);

    tempX = (long)P2x * cosine - (long)P2y * sine;
    tempY = (long)P2x * sine + (long)P2y * cosine;
    P2x = (short)(tempX >> 16);
    P2y = (short)(tempY >> 16);

    tempX = (long)P3x * cosine - (long)P3y * sine;
    tempY = (long)P3x * sine + (long)P3y * cosine;
    P3x = (short)(tempX >> 16);
    P3y = (short)(tempY >> 16);

    tempX = (long)P4x * cosine - (long)P4y * sine;
    tempY = (long)P4x * sine + (long)P4y * cosine;
    P4x = (short)(tempX >> 16);
    P4y = (short)(tempY >> 16);

    // If the entire box is behind us, there is no need to continue...
    if( P1x < 1 && P2x < 1 && P3x < 1 && P4x < 1 )
        return 0;

    if( P1x >= 1 )
        P1y  = 320l - ((long)P1y * 554l) / (long)P1x;
    else
        P1y = 0;

    if( P2x >= 1 )
        P2y  = 320l - ((long)P2y * 554l) / (long)P2x;
    else
        P2y = 0;

    if( P3x >= 1 )
        P3y  = 320l - ((long)P3y * 554l) / (long)P3x;
    else
        P3y = 0;

    if( P4x >= 1 )
        P4y  = 320l - ((long)P4y * 554l) / (long)P4x;
    else
        P4y = 0;

    // Step 4. Are there any points visible on the screen?
    //         If so, then we need to render this area.
    if ((P1y > 0 && P1y < 640) || (P2y > 0 && P2y < 640) ||
        (P3y > 0 && P3y < 640) || (P4y > 0 && P4y < 640))
        return 1;

    // Else, return False (0).
    return 0;

#endif
}

short Renderer::render( short x, short y )
{
    //
    // This method will use the loaded WAD file, mainly the nodes, sidedefs, and textures, and render
    // a view of the walls from position <x,v> looking at the specified angle. Height will be 64 plus
    // the sector height of the sector we're standing in. The vertices will have already been translated
    // and rotated by this point.
    //

    // Clear the double buffer.
    clearToColor(0);

    // Call TraverseBSPTree() with the root node of the BSP tree.
    return traverseBSPTree(WadFile->MaxNode);
}




short Renderer::getSideofCutPlane ( short X, short Y, short Node )
{
    //
    // Determines which side of a particular Node cutting plane a point <X,Y> is on.
    // returns: 0 - Left
    //          1 - Right
    long left;
    long right;

    if( 0 == WadFile->Nodes[Node].CutPlaneDy )
    {
        if( 0 < WadFile->Nodes[Node].CutPlaneDx )
            return ( Y < WadFile->Nodes[Node].CutPlaneStartY );
        else
            return ( Y > WadFile->Nodes[Node].CutPlaneStartY );
    }
    else if( 0 == WadFile->Nodes[Node].CutPlaneDx )
    {
        if ( 0 < WadFile->Nodes[Node].CutPlaneDy )
            return ( X > WadFile->Nodes[Node].CutPlaneStartX );
        else
            return ( X < WadFile->Nodes[Node].CutPlaneStartX );
    }
    else
    {
        left = (long)WadFile->Nodes[Node].CutPlaneDy * (long)(X - WadFile->Nodes[Node].CutPlaneStartX);
        right = (long)WadFile->Nodes[Node].CutPlaneDx * (long)(Y - WadFile->Nodes[Node].CutPlaneStartY);
        return ( left > right );
    }
}

short Renderer::getSideofLinedef( short X, short Y, short LineDef )
{
    //
    // Determines which side of a particular LineDef a point <X,Y> is on.
    // returns: 0 - Left
    //          1 - Right

    long left;
    long right;
    short StartX, StartY;
    short Dx, Dy;

    StartX = WadFile->Vertices[WadFile->Linedefs[LineDef].StartVertex].x;
    StartY = WadFile->Vertices[WadFile->Linedefs[LineDef].StartVertex].y;
    Dx = WadFile->Vertices[WadFile->Linedefs[LineDef].EndVertex].x - StartX;
    Dy = WadFile->Vertices[WadFile->Linedefs[LineDef].EndVertex].y - StartY;

    if( 0 == Dy )
    {
        if( 0 < Dx )
            return ( Y < StartY );
        else
            return ( Y > StartY );
    }
    else if( 0 == Dx )
    {
        if ( 0 < Dy )
            return ( X > StartX );
        else
            return ( X < StartX );
    }
    else
    {
        left = ((long)Dy) * (long)(X - StartX);
        right = ((long)Dx) * (long)(Y - StartY);
        return ( left > right );
    }
}

short Renderer::getSectorfromXY( short X, short Y )
{
    //
    // This method will traverse the BSP tree and determine which sector the <X,Y> point falls into.
    //
    short CurrentNode;
    short SegNbr;
    short LinedefNbr;

    // Start at the top node of the BSP tree.
    CurrentNode = WadFile->MaxNode;

    // Traverse the tree (in Forward order) until a SSector is reached.
    // a SSector is indicated with bit 16.

    while(0 == (unsigned short)(CurrentNode & 0x8000))
    {
        if ( 0 == getSideofCutPlane(X, Y, CurrentNode) )
            CurrentNode = WadFile->Nodes[CurrentNode].LeftChild;
        else
            CurrentNode = WadFile->Nodes[CurrentNode].RightChild;
    }

    // Return the sector number from the SSector
    // by way of the Seg, Linedef, and appropriate SideDef...
    SegNbr = WadFile->SSectors[(unsigned)(CurrentNode & 0x7fff)].StartSeg;
    LinedefNbr = WadFile->Segs[SegNbr].Linedef;

    if( 1 == (getSideofLinedef(X, Y, LinedefNbr)))
    {
        // If on the right side
        return WadFile->Sidedefs[WadFile->Linedefs[LinedefNbr].RightSideDef].SectorNumber;
    }

    // Else left side
    return WadFile->Sidedefs[WadFile->Linedefs[LinedefNbr].LeftSideDef].SectorNumber;
}

short Renderer::traverseBSPTree( short RootNode )
{
    short CurrentNode;
    short NextNode;

    //		Check this pointer. Is it a node or a ssector.
    //			If Node, then recursively call traversal again on it..
    //			If SSector, then call RenderSSector()
    if ( 0 != (unsigned short)(RootNode & 0x8000) )
        return renderSSector( RootNode & 0x7fff );

    // Start at the root node. Determine if we're on the left side, or the right side of it.
    if(0 == getSideofCutPlane(ViewerX, ViewerY, RootNode))
    {
        // If on the left side, traverse the right sub tree first, then the left.
        CurrentNode = WadFile->Nodes[RootNode].RightChild;
        NextNode = WadFile->Nodes[RootNode].LeftChild;
    }
    else
    {
        // Else on the right, then traverse the left sub tree first, then the right.
        CurrentNode = WadFile->Nodes[RootNode].LeftChild;
        NextNode =  WadFile->Nodes[RootNode].RightChild;
    }


    //
    //  Check bounding box to see of we even need consider this node.
    //
    if ( CheckBoundingBox( WadFile->Nodes[RootNode].LeftBBUpperX,
                           WadFile->Nodes[RootNode].LeftBBUpperY,
                           WadFile->Nodes[RootNode].LeftBBLowerX,
                           WadFile->Nodes[RootNode].LeftBBLowerY ))
        traverseBSPTree(CurrentNode);

    //
    //  Check bounding box to see of we even need consider this node.
    //
    if ( CheckBoundingBox( WadFile->Nodes[RootNode].LeftBBUpperX,
                           WadFile->Nodes[RootNode].LeftBBUpperY,
                           WadFile->Nodes[RootNode].LeftBBLowerX,
                           WadFile->Nodes[RootNode].LeftBBLowerY ))
        traverseBSPTree(NextNode);
    return 1;
}

short Renderer::renderSSector( short SSector )
{
    short SegNbr;
    short SegDir;
    short LinedefNbr;
    short ThisSide;
    short BackSide;
    short ThisCeilHeight;
    short BackCeilHeight;
    short ThisFloorHeight;
    short BackFloorHeight;
    long  Pdist;
    long  BOffset;
    long distToStart;
    short NormalAngle;
    short StartAngle;

    // For each SEG in the SSector
    for (int i = 0; i < WadFile->SSectors[SSector].NumSegs; ++i)
    {
        // Get the segment number, and the segment direction.
        SegNbr = WadFile->SSectors[SSector].StartSeg + i;
        SegDir = WadFile->Segs[SegNbr].Direction;

        // Get the Linedef for the segment
        LinedefNbr = WadFile->Segs[SegNbr].Linedef;

        // Get the Sidedef(s) for this Seg and Direction.
        if (1 == SegDir)
        {
            ThisSide = WadFile->Linedefs[LinedefNbr].LeftSideDef;
            BackSide = WadFile->Linedefs[LinedefNbr].RightSideDef;
        }
        else
        {
            ThisSide = WadFile->Linedefs[LinedefNbr].RightSideDef;
            BackSide = WadFile->Linedefs[LinedefNbr].LeftSideDef;
        }

        ThisCeilHeight = WadFile->Sectors[WadFile->Sidedefs[ThisSide].SectorNumber].CeilingHeight;
        ThisFloorHeight = WadFile->Sectors[WadFile->Sidedefs[ThisSide].SectorNumber].FloorHeight;

        // Calculate the shortest distance to the line from the view point.
        StartAngle = invTan( VerticesCache[WadFile->Segs[SegNbr].StartVertex].y,
                             VerticesCache[WadFile->Segs[SegNbr].StartVertex].x );

        NormalAngle = WadFile->Segs[SegNbr].Angle + ANGLE_90 - ViewerAngle;

        distToStart = DistToPoint(VerticesCache[WadFile->Segs[SegNbr].StartVertex].x,
                                  VerticesCache[WadFile->Segs[SegNbr].StartVertex].y);

        Pdist = (distToStart * sin(ANGLE_90 + NormalAngle - StartAngle) ) >> 16;

        // Now calculate the offset from the above point to the start of the wall.
        BOffset = (distToStart * sin(NormalAngle - StartAngle)) >> 16;

        // Calculate side we're on for backface removal.
        if( 1 == (getSideofLinedef(ViewerX,ViewerY,LinedefNbr) ^ SegDir))
            {

#ifdef DEBUG
 /*       myTrace << SegNbr << "|"												// Segment number
                << VerticesCache[WadFile->Segs[SegNbr].StartVertex].x << "|"	// Segment start X
                << VerticesCache[WadFile->Segs[SegNbr].StartVertex].y << "|"	// Segment start Y
                << VerticesCache[WadFile->Segs[SegNbr].EndVertex].x << "|"		// Segment end X
                << VerticesCache[WadFile->Segs[SegNbr].EndVertex].y << "|"		// Segment end Y
                << ( StartAngle / 184.04444444 ) << "|"							// Angle to segment start X,Y
                << ( NormalAngle / 184.04444444 ) << "|"						// Normal angle of segment
                << ( WadFile->Segs[SegNbr].Angle / 184.04444444 ) << "|"		// Unrotated segment normal angle.
                << ( ViewerAngle / 184.04444444 ) << "|"						// View angle
                << distToStart << "|"											// Distance to segment start X,Y
                << Pdist << "|"													// Pdist (distance to shortest intersect.
                << BOffset;*/
#endif
                // If 2 sided linedef, then
                if( BackSide > 0)
                {
                    //	Get the ceiling and floor height of this sector and the adjoining sector.
                    BackCeilHeight = WadFile->Sectors[WadFile->Sidedefs[BackSide].SectorNumber].CeilingHeight;
                    BackFloorHeight = WadFile->Sectors[WadFile->Sidedefs[BackSide].SectorNumber].FloorHeight;

                    //	If the ceiling heights are different,
                    if ( ThisCeilHeight != BackCeilHeight )
                    {
                        // Build upper polygon (Length of seg, from back height to front height)
                        drawWallTopPegged( VerticesCache[WadFile->Segs[SegNbr].StartVertex].x,
                                     VerticesCache[WadFile->Segs[SegNbr].StartVertex].y,
                                     VerticesCache[WadFile->Segs[SegNbr].EndVertex].x,
                                     VerticesCache[WadFile->Segs[SegNbr].EndVertex].y,
                                     ThisCeilHeight,
                                     BackCeilHeight,
                                     WadFile->Segs[SegNbr].Angle,
                                     WadFile->Sidedefs[ThisSide].UpperTxtPtr,
                                     WadFile->Sidedefs[ThisSide].TxtureXOffset +
                                     WadFile->Segs[SegNbr].StartOffset,
                                     WadFile->Sidedefs[ThisSide].TxtureYOffset,
                                     Pdist,
                                     BOffset );
                    }

                    //  If the floor heights are different,
                    if( ThisFloorHeight != BackFloorHeight)
                    {
                        // Build lower polygon (Length of seg, from back floor height to front floor height)
                        drawWallBtmPegged( VerticesCache[WadFile->Segs[SegNbr].StartVertex].x,
                                     VerticesCache[WadFile->Segs[SegNbr].StartVertex].y,
                                     VerticesCache[WadFile->Segs[SegNbr].EndVertex].x,
                                     VerticesCache[WadFile->Segs[SegNbr].EndVertex].y,
                                     BackFloorHeight,
                                     ThisFloorHeight,
                                     WadFile->Segs[SegNbr].Angle,
                                     WadFile->Sidedefs[ThisSide].LowerTxtPtr,
                                     WadFile->Sidedefs[ThisSide].TxtureXOffset +
                                     WadFile->Segs[SegNbr].StartOffset,
                                     WadFile->Sidedefs[ThisSide].TxtureYOffset,
                                     Pdist,
                                     BOffset );
                    }
                }

                // Now draw the center texture.
                drawWallTopPegged( VerticesCache[WadFile->Segs[SegNbr].StartVertex].x,
                                 VerticesCache[WadFile->Segs[SegNbr].StartVertex].y,
                                 VerticesCache[WadFile->Segs[SegNbr].EndVertex].x,
                                 VerticesCache[WadFile->Segs[SegNbr].EndVertex].y,
                                 ThisCeilHeight,
                                 ThisFloorHeight,
                                 WadFile->Segs[SegNbr].Angle,
                                 WadFile->Sidedefs[ThisSide].NormalTxtPtr,
                                 WadFile->Sidedefs[ThisSide].TxtureXOffset +
                                 WadFile->Segs[SegNbr].StartOffset,
                                 WadFile->Sidedefs[ThisSide].TxtureYOffset,
                                 Pdist,
                                 BOffset );


#ifdef DEBUG
//            myTrace << endl;
#endif

        }
    }

    //  *** Implement Later ***

    //	drawFlat(WadFile->SSectors[SSector].StartSeg, WadFile->SSectors[SSector].NumSegs, ThisCeilHeight);
    //  drawFlat(WadFile->SSectors[SSector].StartSeg, WadFile->SSectors[SSector].NumSegs, ThisFloorHeight);

    //		Z-sort the list of Things in this SSector
    //		For each Thing, render it.

    return 1;
}

struct Edge {
    long StartR, StartC;
    long EndR, EndC;
    Edge* nextEdge;

    Edge( long Sr, long Sc, long Er, long Ec )
    { StartR = Sr; StartC = Sc; EndR = Er; EndC = Ec;};
    ~Edge( void)
    { if( NULL != nextEdge ) delete nextEdge; };
};

short Renderer::drawFlat(unsigned short StartSeg, unsigned short numSegs, short Height)
{

    // A flat is a convex polygon bounded by the segments in a SSector. It has a height, and a texture.
    // The texture is a 64x64 bitmap, and it's origin lies on a 64x64 boundary on the x/y plane. We will
    // probably use a general polygon filling routine here, noting that X (depth) is constant across the rows.
    //
    // Note: This version does not do the texturing, and is still a work-in-progress...
    //
    short cntr;
    short currSeg;
    short StartX;
    short StartY;
    short EndX;
    short EndY;
    long  ColStart;
    long  ColEnd;
    long  RowStart;
    long  RowEnd;
    long  DeltaL;
    long  DeltaR;
    long  StartCol = 0;
    long  EndCol = 0;
    long EndRow = 0;
    Edge  *tmpEdgeList = NULL;
    Edge  *newEdge = NULL;

#ifdef DEBUG
    // DEBUG: ********************************************************************
    FILE* OutFile;
    OutFile = fopen("c:\\temp\\edgelist.out","a+");
    fprintf(OutFile, " ----------------------NEW POLYGON----------------------- \n");
    // ***************************************************************************
#endif

    // Step 1. Create a sorted list of SEGs, sorted on startX (view space)... Insertion Sort.
    //         * Remove any horizontal edges.
    //         * Remove any edges with startX < 1 && endX < 1
    //         * Clip any partially visible edges against X=1 plane.
    for( cntr = 0; cntr < numSegs; ++cntr )
    {
        currSeg = StartSeg + cntr;

        // Get the points of interest for this next edge.

        StartX = VerticesCache[WadFile->Segs[currSeg].StartVertex].x;
        StartY = VerticesCache[WadFile->Segs[currSeg].StartVertex].y;
        EndX = VerticesCache[WadFile->Segs[currSeg].EndVertex].x;
        EndY = VerticesCache[WadFile->Segs[currSeg].EndVertex].y;

#ifdef DEBUG
        // DEBUG: ******************************************************************************
        fprintf(OutFile, "Seg: <%d,%d,%d> -> <%d,%d,%d> \n", StartY, Height, StartX, EndY, Height, EndX);
        // **************************************************************************************
#endif

        if (( StartX < 1) && (EndX < 1 ))
            continue;

        // We need to clip the incoming segment at the X=1 plane...
        if( StartX < 1 && EndX >= 1 )
        {
            if( StartY != EndY )
                StartY = StartY + ((1-StartX) * ( EndY - StartY )) / (EndX - StartX);
            StartX = 1;
        }
        if( StartX >= 1 && EndX < 1)
        {
            if( StartY != EndY )
                EndY = StartY + ((1-StartX) * ( EndY - StartY )) / (EndX - StartX);
            EndX = 1;
        }
#ifdef DEBUG
        // DEBUG: ******************************************************************************
        fprintf(OutFile, "Clipped: <%d,%d,%d> -> <%d,%d,%d> \n", StartY, (Height - ViewerHeight), StartX, EndY, (Height - ViewerHeight), EndX);
        // **************************************************************************************
#endif

        // Transform the points.
        ColStart  =  320l - (((long)StartY * 554l) / (long)StartX);
        RowStart  =  240l - (((long)(Height - ViewerHeight) * 554l) / (long)StartX);
        ColEnd    =  320l - (((long)EndY * 554l) / (long)EndX);
        RowEnd    =  240l - (((long)(Height - ViewerHeight) * 554l) / (long)EndX);

#ifdef DEBUG
        // DEBUG: ******************************************************************************
        fprintf(OutFile, "Xformed: <%ld,%ld> -> <%ld,%ld> \n", ColStart,RowStart,ColEnd,RowEnd);
        // **************************************************************************************
#endif

        // More trivial rejection (Clipping polygons that can not be seen).
        if ((ColStart == ColEnd) || (( ColStart < 0) && (ColEnd < 0)) || ((ColStart > 640) && (ColEnd > 640)))
            continue;

        // Now, insert the edge into the edge list.
        if (RowStart != RowEnd )
        {
            if (RowStart > RowEnd )
                newEdge = new Edge( RowEnd, ColEnd, RowStart, ColStart );
            else
                newEdge = new Edge( RowStart, ColStart, RowEnd, ColEnd );

            if (RowEnd > EndRow)
                EndRow = RowEnd;
            if (RowStart > EndRow)
                EndRow = RowStart;


            // Now insert the edge into the list.
            if( tmpEdgeList == NULL)
            {
                newEdge->nextEdge = NULL;
                tmpEdgeList = newEdge;
            }
            else
            {
                Edge* srch = tmpEdgeList;
                Edge* prev = NULL;
                while(srch != NULL)
                {
                    if( srch->StartR < newEdge->StartR )
                    {
                        // insert Node Here.
                        if (NULL == prev)
                        {
                            newEdge->nextEdge = tmpEdgeList;
                            tmpEdgeList = newEdge;
                        }
                        else
                        {
                            newEdge->nextEdge = srch;
                            prev->nextEdge = newEdge;
                        }
                        break;
                    }
                    prev = srch;
                    srch = srch->nextEdge;
                }
                if( srch == NULL)
                {
                    newEdge->nextEdge = srch;
                    prev->nextEdge = newEdge;
                }
            }
        }

    }

#ifdef DEBUG
    // DEBUG: ************************************************************************************************
    for (Edge* node = tmpEdgeList; node != NULL; node = node->nextEdge)
        fprintf(OutFile, " Edge: <%ld,%ld> -> <%ld,%ld> \n",node->StartC, node->StartR, node->EndC, node->EndR);
    fclose(OutFile);
    // *******************************************************************************************************
#endif

    // Step 2. Determine if the SSector is partially visible. If not, then return.
    //		   ( If the list contains any edges )

    if (( tmpEdgeList != NULL ) && ( tmpEdgeList->nextEdge != NULL ))
    {
        Edge *left, *right, *nextEdge;

        // Step 3. Start with the 1st 2 edges. Calculate the deltas.
        if( tmpEdgeList->StartC < (tmpEdgeList->nextEdge)->StartC )
        {
            left = tmpEdgeList;
            right = tmpEdgeList->nextEdge;
            nextEdge = right->nextEdge;
        }
        else
        {
            right = tmpEdgeList;
            left = tmpEdgeList->nextEdge;
            nextEdge = left->nextEdge;
        }

        StartCol = left->StartC * 65536l;
        EndCol = right->StartC * 65536l;
        DeltaL = (65536l * (left->EndC - left->StartC)) / (left->EndR - left->StartR);
        DeltaR = (65536l * (right->EndC - right->StartC)) / (right->EndR - right->StartR);

        // Step 4. Start rendering the polygon...
        EndRow = (EndRow > 400)?400: EndRow;
        short StartRow = ((left->StartR) > 0)? (short)(left->StartR):0;

        // myTrace << "| Visible |";

        for(short row = StartRow; row < EndRow; ++row)
        {
            // Step 4a. If we go past an edge, get the next one in the sorted list, and
            //          recompute the deltas.
            if (( row > left->EndR ) && (row <= right->EndR ))
            {
                if ( nextEdge == NULL ) return 1;

                left = nextEdge;
                nextEdge = left->nextEdge;

                StartCol = left->StartC  * 65536l;
                DeltaL = (65536l * (left->EndC - left->StartC)) / (left->EndR - left->StartR);
            }
            else if ((row > right->EndR) && (row <= left->EndR))
            {
                if ( nextEdge == NULL ) return 1;

                right = nextEdge;
                nextEdge = right->nextEdge;

                EndCol = right->StartC * 65536l;
                DeltaR = (65536l * (right->EndC - right->StartC)) / (right->EndR - right->StartR);
            }
            else if ((row > left->EndR) && (row > right->EndR))
            {
                if ( nextEdge == NULL || nextEdge->nextEdge == NULL) return 1;

                if( nextEdge->StartC < (nextEdge->nextEdge)->StartC )
                {
                    left = nextEdge;
                    right = left->nextEdge;
                    nextEdge = right->nextEdge;
                }
                else
                {
                    right = nextEdge;
                    left = right->nextEdge;
                    nextEdge = left->nextEdge;
                }
                StartCol = left->StartC * 65536l;
                EndCol = right->StartC * 65536l;
                DeltaL = (65536l * (left->EndC - left->StartC)) / (left->EndR - left->StartR);
                DeltaR = (65536l * (right->EndC - right->StartC)) / (right->EndR - right->StartR);
            }

            // Step 4b. Render the row sliver.
            for (short col = (short)(StartCol>>16); col <= (short)(EndCol>>16); ++col)
            {
                if(( col > 0 ) && ( col < 640))
                {
                    *(DoubleBuffer + col + row * 640l) = 30;
                }
            }

            StartCol += DeltaL;
            EndCol += DeltaR;
        }
    }

    return 1;
}

short Renderer::drawWallTopPegged(short StartX, short StartY, short EndX, short EndY, short top, short bottom, short Angle, Texture* PtrTxt, short XOffset, short YOffset, long Pdist, long BOffset )
{
    //
    // This will actually draw the textured wall polygon on the screen buffer. (320x200 buffer size).
    //
    long  ColStart;       // Starting column for the polygon.
    long  ColEnd;         // Ending column for the polygon.
    long  StartTop;       // Starting row for the 1st column.
    long  StartBtm;		  // Ending row for the 1st column.
    long  EndTop;		  // Starting row for the last column.
    long  EndBtm;         // Ending row for the last column.
    long  DeltaTop;       // Amount to change Ytop for each column.
    long  DeltaBtm;       // Amount to change the Ysize for each column.
    long  CurrentRow;
    long  CurrentCol;
    long  CurrentTop;
    long  CurrentBtm;
    short TxtCol = 0;
    short txtwidth = 0;
    short txtheight = 0;
    char* txtPtr = NULL;
    char* txtrowPtr = NULL;
    long  DeltaTxt = 0;
    long  currTxtl = 0;
    unsigned char * ScrnPtr;
    long tmptop;
    long tmpbottom;
    long CurrentScale;
    long EndScale;
    long DeltaScale;
    long TempTop;

    // Try to do trivial rejection (If this is a clear polygon, or it is completely behind the view window).
    if ((PtrTxt == NULL) || ( StartX < 1 && EndX < 1 ))
        return 0;

    // We need to clip the incoming segment at the X=1 plane...
    if( StartX < 1 && EndX >= 1 )
    {
        if( StartY != EndY )
            StartY = StartY + ((1-StartX) * ( EndY - StartY )) / (EndX - StartX);
        StartX = 1;
    }
    if( StartX >= 1 && EndX < 1)
    {
        if( StartY != EndY )
            EndY = StartY + ((1-StartX) * ( EndY - StartY )) / (EndX - StartX);
        EndX = 1;
    }

    // Transform the starting / ending points to screen coordinates. Looking down +X axis.
    //
    tmptop = ((long)top - ViewerHeight) * 554l;
    tmpbottom =((long)bottom - ViewerHeight) * 554l;

    ColStart    =  320l - ((long)StartY * 554l) / (long)StartX;
    ColEnd  =  320l - ((long)EndY * 554l) / (long)EndX;

    EndTop  =  240l - (tmptop / (long)EndX);
    StartTop    =  240l - (tmptop / (long)StartX);

    EndBtm  =  240l - (tmpbottom / (long)EndX);
    StartBtm    =  240l - (tmpbottom / (long)StartX);

    // More trivial rejection (Clipping polygons that can not be seen).
    if ((ColStart == ColEnd) || (( ColStart < 0) && (ColEnd < 0)) || ((ColStart > 640) && (ColEnd > 640)))
        return 0;

    // Calculate the deltas to interpolate the Ystart and Yend for each screen column.
    //
    //  Future change: Use a stepping table (1<<14)/DeltaWidth, where DeltaWidth is in [1,1024].
    //
    // myTrace << "| Visible |";

    txtPtr = PtrTxt->TxtPtr;
    txtheight = PtrTxt->TxtHeight;
    txtwidth = PtrTxt->TxtWidth;

    CurrentTop = StartTop <<14;
    CurrentBtm = StartBtm <<14;
    DeltaTop = ((EndTop - StartTop) * 1l<<14) / (ColEnd - ColStart);
    DeltaBtm = ((EndBtm - StartBtm) * 1l<<14) / (ColEnd - ColStart);
    CurrentScale = (1161822208l / (long)StartX); // ( 128 * 554 * 1<<14 ) / Distance
    EndScale = (1161822208l / (long)EndX );
    DeltaScale = ( EndScale - CurrentScale ) / (ColEnd - ColStart);


    // For each column of the polygon,
    for ( CurrentCol = ColStart; CurrentCol <= ColEnd; ++CurrentCol )
    {
        // Scale the texture column to size (Yend-Ystart), starting at row Ystart.
        if ((CurrentCol > 0) && (CurrentCol < 640))
        {
            //	The texture column is given by TxtCol = (Pdist * Tan(Angle of column ray to Pdist vector)) - Bo
            TxtCol = (short)(((Pdist * tan((colangle[CurrentCol] + (Angle - ViewerAngle + ANGLE_90)))) - BOffset)>> 16) - XOffset;
            TxtCol = (unsigned)TxtCol % txtwidth;
            txtrowPtr = txtPtr + (long)TxtCol * txtheight;
            if (( (CurrentBtm - CurrentTop) >> 14) > 0)
            {
                //
                // This could be in a table of (1<<14)/dHeight, dHeight goes from 1 to ...
                // Then DeltaTxt = txtheight * ScaleTable[( CurrentBtm - CurrentTop) >> 14].
                //
                DeltaTxt = 2097152l /(CurrentScale >> 14);
                currTxtl = ((long)-YOffset) << 14;

                if(( currTxtl >> 14 ) < 0 )
                    currTxtl += (txtheight - YOffset) << 14;

                if ( ( currTxtl >> 14 ) > txtheight )
                    currTxtl -= ((long)txtheight) << 14;


                        TempTop = CurrentTop >> 14;

                if ( TempTop < 0l )
                {
                    currTxtl += DeltaTxt * (0 - TempTop);

                    if(( currTxtl >> 14 ) < 0 )
                    currTxtl += (txtheight - YOffset) << 14;
                    if ( ( currTxtl >> 14 ) > txtheight )
                    currTxtl -= ((long)txtheight) << 14;
                    if ( ( currTxtl >> 14 ) > txtheight )
                    currTxtl -= ((long)txtheight) << 14;
                    if ( ( currTxtl >> 14 ) > txtheight )
                    currTxtl -= ((long)txtheight) << 14;

                    TempTop = 0l;
                }

                ScrnPtr = (DoubleBuffer + CurrentCol + TempTop * 640l);

                for ( CurrentRow = TempTop; CurrentRow < (CurrentBtm >> 14) && CurrentRow < 400; ++CurrentRow )
                {
                    if(0 != *(txtrowPtr + (currTxtl >> 14)))
                        *(ScrnPtr) = *(txtrowPtr + (currTxtl >> 14));
                    ScrnPtr += 640l;
                    currTxtl += DeltaTxt;
                    if(( currTxtl >> 14 ) < 0 )
                        currTxtl += (txtheight - YOffset) << 14;
                    if ( ( currTxtl >> 14 ) > txtheight )
                        currTxtl -= ((long)txtheight) << 14;
                }
            }
        }

        // Increment the deltas for the top and bottom edges.
        CurrentTop += DeltaTop;
        CurrentBtm += DeltaBtm;
        CurrentScale += DeltaScale;

    }
    return 1;
}

short Renderer::drawWallBtmPegged(short StartX, short StartY, short EndX, short EndY, short top, short bottom, short Angle, Texture* PtrTxt, short XOffset, short YOffset, long Pdist, long BOffset )
{
    //
    // This will actually draw the textured wall polygon on the screen buffer. (320x200 buffer size).
    //
    long  ColStart;       // Starting column for the polygon.
    long  ColEnd;         // Ending column for the polygon.
    long  StartTop;       // Starting row for the 1st column.
    long  StartBtm;		  // Ending row for the 1st column.
    long  EndTop;		  // Starting row for the last column.
    long  EndBtm;         // Ending row for the last column.
    long  DeltaTop;       // Amount to change Ytop for each column.
    long  DeltaBtm;       // Amount to change the Ysize for each column.
    long  CurrentRow;
    long  CurrentCol;
    long  CurrentTop;
    long  CurrentBtm;
    short TxtCol = 0;
    short txtwidth = 0;
    short txtheight = 0;
    char* txtPtr = NULL;
    char* txtRowPtr = NULL;
    long  DeltaTxt = 0;
    long  currTxtl = 0;
    unsigned char * ScrnPtr;
    long tmptop;
    long tmpbottom;
    long CurrentScale;
    long EndScale;
    long DeltaScale;
    long TempBottom;

    // Try to do trivial rejection (If this is a clear polygon, or it is completely behind the view window).
    if ((PtrTxt == NULL) || ( StartX < 1 && EndX < 1 ))
        return 0;

    // We need to clip the incoming segment at the X=1 plane...
    if( StartX < 1 && EndX >= 1 )
    {
        if( StartY != EndY )
            StartY = StartY + ((1-StartX) * ( EndY - StartY )) / (EndX - StartX);
        StartX = 1;
    }
    if( StartX >= 1 && EndX < 1)
    {
        if( StartY != EndY )
            EndY = StartY + ((1-StartX) * ( EndY - StartY )) / (EndX - StartX);
        EndX = 1;
    }

    // Transform the starting / ending points to screen coordinates. Looking down +X axis.
    //
    // Optimization: Possibly precalculate 65536/StartX and 65536/EndX...
    //
    tmptop = ((long)top - ViewerHeight) * 554l;
    tmpbottom =((long)bottom - ViewerHeight) * 554l;

    ColStart    =  320l - ((long)StartY * 554l) / (long)StartX;
    ColEnd  =  320l - ((long)EndY * 554l) / (long)EndX;

    EndTop  =  240l - (tmptop / (long)EndX);
    StartTop    =  240l - (tmptop / (long)StartX);

    EndBtm  =  240l - (tmpbottom / (long)EndX);
    StartBtm    =  240l - (tmpbottom / (long)StartX);

    // More trivial rejection (Clipping polygons that can not be seen).
    if ((ColStart == ColEnd) || (( ColStart < 0) && (ColEnd < 0)) || ((ColStart > 640) && (ColEnd > 640)))
        return 0;

    // Calculate the deltas to interpolate the Ystart and Yend for each screen column.
    //
    //  Future change: Use a stepping table (1<<14)/DeltaWidth, where DeltaWidth is in [1,1024].
    //
    CurrentTop = StartTop <<14;
    CurrentBtm = StartBtm <<14;
    DeltaTop = ((EndTop - StartTop) * 1l<<14) / (ColEnd - ColStart);
    DeltaBtm = ((EndBtm - StartBtm) * 1l<<14) / (ColEnd - ColStart);

    // We can do this because 1/z is linear in screen space.
    CurrentScale = (1161822208l / (long)StartX); // 128 (texture height) * 554 (scaling factor) * (1 << 14) / Distance.
    EndScale = (1161822208l / (long)EndX );
    DeltaScale = ( EndScale - CurrentScale ) / (ColEnd - ColStart);

    txtPtr = PtrTxt->TxtPtr;
    txtheight = PtrTxt->TxtHeight;
    txtwidth = PtrTxt->TxtWidth;

    // For each column of the polygon,
    for ( CurrentCol = ColStart; CurrentCol <= ColEnd; ++CurrentCol )
    {
        // Scale the texture column to size (Yend-Ystart), starting at row Ystart.
        if ((CurrentCol > 0) && (CurrentCol < 640))
        {
            //	The texture column is given by TxtCol = (Pdist * Tan(Angle of column ray to Pdist vector)) - Bo
            TxtCol = (unsigned short)((((Pdist * tan((colangle[CurrentCol] + (Angle - ViewerAngle + ANGLE_90)))) - BOffset)>> 16) - XOffset) % txtwidth;
            txtRowPtr = txtPtr + (long)TxtCol * txtheight;

            if (( (CurrentBtm - CurrentTop) >> 14) > 0)
            {
                //
                // This could be in a table of (1<<14)/dHeight, dHeight goes from 1 to 1024...
                // Then DeltaTxt = txtheight * ScaleTable[( CurrentBtm - CurrentTop) >> 14].
                //
                // Currently DeltaTxt = ( 128 * 1<<14 ) / Height of the texture at the specific distance.
                // since 128 is guarenteed to be the texture height, and we're using 18:14 fixed point.

                DeltaTxt = 2097152l /(CurrentScale >> 14);
                currTxtl = (txtheight + YOffset) << 14;

                if(( currTxtl >> 14 ) < 0 )
                    currTxtl += (txtheight - YOffset) << 14;
                if ( ( currTxtl >> 14 ) > txtheight )
                    currTxtl -= ((long)txtheight) << 14;


                TempBottom = CurrentBtm >> 14;
                if (  TempBottom > 399 )
                {
                    currTxtl -= DeltaTxt * (TempBottom - 399l);

                    if(( currTxtl >> 14 ) < 0 )
                        currTxtl += (long)txtheight << 14;
                    if ( ( currTxtl >> 14 ) > txtheight )
                        currTxtl -= (long)txtheight << 14;
                    if ( ( currTxtl >> 14 ) > txtheight )
                        currTxtl -= (long)txtheight << 14;
                    if ( ( currTxtl >> 14 ) > txtheight )
                        currTxtl -= (long)txtheight << 14;
                    if ( ( currTxtl >> 14 ) > txtheight )
                        currTxtl -= (long)txtheight << 14;

                    TempBottom = 399l;
                }

                ScrnPtr = (DoubleBuffer + CurrentCol + TempBottom * 640l);
                for ( CurrentRow = TempBottom; CurrentRow >= (CurrentTop >> 14) && CurrentRow >= 0; --CurrentRow )
                {
                    if(0 != *( txtRowPtr + (currTxtl >> 14) ))
                        *(ScrnPtr) = *( txtRowPtr + (currTxtl >> 14) );
                    ScrnPtr -= 640l;
                    currTxtl -= DeltaTxt;
                    if(( currTxtl >> 14 ) < 0 )
                        currTxtl += (long)txtheight << 14;
                    if ( ( currTxtl >> 14 ) > txtheight )
                        currTxtl -= (long)txtheight << 14;
                }
            }
        }

        // Increment the deltas for the top and bottom edges.
        CurrentTop += DeltaTop;
        CurrentBtm += DeltaBtm;
        CurrentScale += DeltaScale;

    }
    return 1;
}

short Renderer::changeViewPosn( short X, short Y, short Angle )
{

    // This routine has the effect of translating and rotating all of the vertices
    // and placing them in a temporary holding area, which will be used in rendering the
    // final view.
    long tempX;
    long tempY;

    register long cosine = this->cos(-Angle);
    register long sine = this->sin(-Angle);

    //  Transform all vertices to view coordinates, and place new vertices in cache.
    for (int i=0; i < WadFile->NbrVertices; ++i)
    {
        // Copy the world vertex to the cache.
        VerticesCache[i].x = WadFile->Vertices[i].x - X;
        VerticesCache[i].y = WadFile->Vertices[i].y - Y;

        // Now rotate the vertex so that the player is looking down the positive X axis.
        tempX = (long)VerticesCache[i].x * cosine - (long)VerticesCache[i].y * sine;
        tempY = (long)VerticesCache[i].x * sine + (long)VerticesCache[i].y * cosine;

        VerticesCache[i].x = (short)(tempX >> 16);
        VerticesCache[i].y = (short)(tempY >> 16);
    }

    // We need to get the floor height at this place, and set it.
    ViewerX = X;
    ViewerY = Y;
    ViewerHeight = WadFile->Sectors[getSectorfromXY(X, Y)].FloorHeight + 56;
    ViewerAngle = Angle;

    return ViewerHeight;
}

