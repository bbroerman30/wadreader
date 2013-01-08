#ifndef DOOMRENDERHDR
#define DOOMRENDERHDR
#include <windows.h>
#include "types.h"
#include "Wad.h"

class Renderer
{
private:
	// Pointers to double buffer info and address. 
	HWND             ghWnd;
	BITMAPINFO *     BitMapInfo ;
	HBITMAP          DblBuff;
	unsigned char *  DoubleBuffer;

	// Game map related stuff.
	GameWad *WadFile;
	Vertex  *VerticesCache;
	short    ViewerHeight;
	short    ViewerAngle;
	short	 ViewerX;
	short	 ViewerY;

	// Trig tables used in rendering.
	long    SinTable[8192];
	long    CosTable[8192];
	long	TanTable[8192];
	short   colangle[640];
	long    invTanTable[2049];
	long    DeltaTxtTbl[8192];

public:
	Renderer( HWND hWnd, GameWad* Wad );
	~Renderer( void );
	short    render(short x, short y);
	short    changeViewPosn ( short X, short Y, short Angle );
	HBITMAP  getDIB() { return DblBuff; };
	int      Save_Pcx(char *filename);
	long     sin(short Angle);
	long     cos(short Angle);
	long     tan(short Angle);
	short    invTan(long y, long x);

private:
	short getSideofLinedef( short X, short Y, short LineDef );
	short getSideofCutPlane ( short X, short Y, short Node );
	short getSectorfromXY( short X, short Y );
	short traverseBSPTree( short RootNode );
	short renderSSector( short SSector );
	short drawWallTopPegged( short StartX,
					   short StartY, 
					    short EndX, 
					    short EndY, 
					    short top, 
					    short bottom, 
					    short Angle, 
					    Texture* TxtPtr, 
					    short XOffset, 
					    short YOffset, 
						long  Pdist, 
						long  BOffset );

	short drawWallBtmPegged( short StartX,
					   short StartY, 
					    short EndX, 
					    short EndY, 
					    short top, 
					    short bottom, 
					    short Angle, 
					    Texture* TxtPtr, 
					    short XOffset, 
					    short YOffset, 
						long  Pdist, 
						long  BOffset );
	short drawFlat( unsigned short StartSeg,
					unsigned short numSegs,
					short Height );
	short initTables(void);
	void  init_double_buffer(void);
	void  clearToColor(char c);
	void  ChangePalette ( short PaletteNbr );
	void  copyscrn (char *Buffer);
	long  DistToPoint (long x, long y);
	short PointToAngle ( long x, long y );
	short CheckBoundingBox ( short UpperX,
		                     short UpperY,
							 short LowerX,
							 short LowerY );
};

#endif
