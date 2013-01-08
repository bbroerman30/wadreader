#include <stdio.h>
#include <string.h>
#include "Wad.h"

GameWad::GameWad( void )
{
	// Default constructor - Initializes data members.
	strcpy(WadFileName,"");
	WadFilePtr = NULL;
	WadFileInitialized = 0;
	DirectoryOffset = 0;
	NumberOfLumps = 0;
	NbrTextures = 0;
	Things = NULL;
	Vertices = NULL;
	Linedefs = NULL;
	Sectors = NULL;
	SSectors = NULL;
	Sidedefs = NULL;
	Nodes = NULL;
	Pnames = NULL;
	Textures = NULL;
}

GameWad::GameWad( char* Fname )
{
	strcpy(WadFileName, Fname);
	WadFilePtr = NULL;
	WadFileInitialized = 0;
	DirectoryOffset = 0;
	NumberOfLumps = 0;
	NbrTextures = 0;
	Things = NULL;
	Vertices = NULL;
	Linedefs = NULL;
	Sectors = NULL;
	SSectors = NULL;
	Sidedefs = NULL;
	Nodes = NULL;
	Pnames = NULL;
	Textures = NULL;

	InitWadFile( Fname );
}

GameWad::~GameWad( void )
{
	clearMap();

	if (NULL != Pnames )
	{
		delete Pnames;
		Pnames = NULL;
	}

	if (NULL != Textures )
	{
		Texture* curr;

		curr = Textures;
		while( curr != NULL )
		{
			curr = curr->nextTxt;
			delete curr->PrevTxt;
		}
		Textures = NULL;
	}
}

int GameWad::clearMap(void)
{
	if (NULL != Things )
	{
		delete Things;
		Things = NULL;
	}

	if (NULL != Vertices )
	{
		delete Vertices;
		Vertices = NULL;
	}

	if (NULL != Linedefs )
	{
		delete Linedefs;
		Linedefs = NULL;
	}

	if (NULL != Sectors )
	{
		delete Sectors;
		Sectors = NULL;
	}

	if (NULL != SSectors )
	{
		delete SSectors;
		SSectors = NULL;
	}

	if (NULL != Sidedefs )
	{
		delete Sidedefs;
		Sidedefs = NULL;
	}

	if (NULL != Nodes )
	{
		delete Nodes;
		Nodes = NULL;
	}

	return 1;
}


int GameWad::InitWadFile( char* Fname )
{
	WadHdrLoad          WadHeader;
	
	// Step 1. Open the file.
	strcpy(WadFileName, Fname);
	if( NULL == (WadFilePtr = fopen(Fname, "rb")))
	{
		return -1;
	}

	// Step 2. Read in the header. Ensure we're reading a 'IWAD' file.
	if( 1 != fread((void *)&WadHeader, sizeof(WadHdrLoad), 1, WadFilePtr) )
	{
		return -1;
	}

	if ( WadHeader.Type[0] != 'I' && WadHeader.Type[1] != 'W' && WadHeader.Type[2] != 'A' && WadHeader.Type[3] != 'D' )
	{
		return -2;
	}
	
	NumberOfLumps = WadHeader.NbrLumps;
	DirectoryOffset = WadHeader.DirectoryOffset;

	if (findDirectoryEntry("PLAYPAL"))
		loadPalettes();

	if (findDirectoryEntry("COLORMAP"))
		loadColorMaps();

	if (findDirectoryEntry("PNAMES"))
		loadPNames();

	if (findDirectoryEntry("TEXTURE1"))
		loadTextures();

	if (findDirectoryEntry("TEXTURE2"))
		loadTextures();
	//
	// This is so we can check that this step was done later.
	//
	WadFileInitialized = 1;

	// Step 5. Close the file.
	fclose(WadFilePtr);

	return 1;
}

int GameWad::LoadMap( int episode, int level )
{
	//
	// This function loads a level, i.e. E1M1 into the appropriate structures.
	//
	
	char                MapName[5];
	char                EntryName[9];
	int                 ReadLvl = 0;
	int                 StillReadingLumps = 1;
	WadHdrLoad          WadHeader;
	DirectoryEntryLoad  DirectoryEntry;

	if (0 == WadFileInitialized)
		return -1;

	//
	// If there is another map loaded, unload it.
	//
	clearMap();
	
	// Step 1. Open the file. (It was closed after the call to ReadWadFile)
	if( NULL == (WadFilePtr = fopen(WadFileName, "rb")))
		return -1;
	
	// Step 2. Read in the header to ensure we're reading the 'IWAD' file.
	if( 1 != fread((void *)&WadHeader, sizeof(WadHdrLoad), 1, WadFilePtr) )
		return -1;
	
	if ( WadHeader.Type[0] != 'I' && WadHeader.Type[1] != 'W' && WadHeader.Type[2] != 'A' && WadHeader.Type[3] != 'D' )
		return -2;
	
	//
	// Step 3. Generate the name of the tag we're going to search for.
	//
	sprintf(MapName, "E%1dM%1d", episode, level);
	
	if( 1 == findDirectoryEntry((const char*)MapName))
	{
		//
		// The next 10 lumps define the level. They could be in any order.
		//
		for (int i=0; i < 10; ++i) 
		{
			// Step 4a. Get the entry from the file.
			fread((void *)&DirectoryEntry, sizeof(DirectoryEntryLoad), 1, WadFilePtr);
			memset(EntryName, 0, 9);
			strncpy(EntryName, DirectoryEntry.LumpName, 8);

			if ( 0 == strcmp(EntryName,"THINGS"))
			{
				// Read the list of things.
				int NumThings = DirectoryEntry.LumpSize / sizeof(Thing);
				loadThings(DirectoryEntry.LumpStartOffset, NumThings);
			}

			if ( 0 == strcmp(EntryName,"LINEDEFS"))
			{
				// Read the linedefs.
				int NumLineDefs = DirectoryEntry.LumpSize / sizeof(LineDef);
				loadLineDefs(DirectoryEntry.LumpStartOffset, NumLineDefs);
			}

			if ( 0 == strcmp(EntryName,"SIDEDEFS"))
			{				
				// Read the sidedefs.
				int NumSideDefs = DirectoryEntry.LumpSize / sizeof(SideDefLdr);
				loadSideDefs(DirectoryEntry.LumpStartOffset, NumSideDefs);
			}

			if ( 0 == strcmp(EntryName,"VERTEXES"))
			{
				// Read the vertex list.
				int NumVertexes = DirectoryEntry.LumpSize / sizeof(Vertex);
				loadVertexes(DirectoryEntry.LumpStartOffset, NumVertexes);
			}

			if ( 0 == strcmp(EntryName,"SEGS"))
			{
				// Read the list of segments.
				int NumSegments = DirectoryEntry.LumpSize / sizeof(Seg);
				loadSegments(DirectoryEntry.LumpStartOffset, NumSegments);
			}

			if ( 0 == strcmp(EntryName,"SSECTORS"))
			{
				// Read the list of sub sectors.
				int NumSSectors = DirectoryEntry.LumpSize / sizeof(SSector);
				loadSSectors(DirectoryEntry.LumpStartOffset, NumSSectors);
			}

			if ( 0 == strcmp(EntryName,"NODES"))
			{
				// Read the list of nodes ( BSP Tree ).
				int NumNodes = DirectoryEntry.LumpSize / sizeof(Node);
				loadNodes(DirectoryEntry.LumpStartOffset, NumNodes);
			}

			if ( 0 == strcmp(EntryName,"SECTORS"))
			{	
				// Read the list of sectors.
				int NumSectors = DirectoryEntry.LumpSize / sizeof(Sector);
				loadSectors(DirectoryEntry.LumpStartOffset, NumSectors);
			}

			


		}		

		// Close the file.
		fclose(WadFilePtr);

		// Indicate a successful load.
		return 1;
	}

	// Close the file.
	fclose(WadFilePtr);

	// Indicate we couldn't find the level.
	return -1;
}

int GameWad::findDirectoryEntry(const char* SearchName, long* LumpSize) 
{
	//
	//  Will search the WAD directory for a specific lump. If found, it will position the
	// file pointer to point to that lump, and return NON-ZERO (True). If the found entry is
	// a flag, i.e. E1M1, the pointer will point to the directory entry just after the one found.
	// If *LumpSize is specified, it will place the size of the lump there.
	//   If the lump is not found, the file pointer will not be changed, and the function will
	// return zero (False).
	//
	char                  EntryName[9];
	long                  startingOffset;
	DirectoryEntryLoad    DirectoryEntry;

	startingOffset = ftell(WadFilePtr);

	// Move the file pointer to the beginning of the Directory.
	if( 0 != fseek(WadFilePtr, DirectoryOffset, SEEK_SET))
		return 0;
	
	// For each item in the directory,
	for( int i=0; i < NumberOfLumps; ++i )
	{
		// Get the entry from the file.
		fread((void *)&DirectoryEntry, sizeof(DirectoryEntryLoad), 1, WadFilePtr);
		memset(EntryName, 0, 9);
		strncpy(EntryName, DirectoryEntry.LumpName, 8);

		if (strcmp(EntryName, SearchName) == 0)
		{
			if (DirectoryEntry.LumpSize > 0)
			{
				fseek(WadFilePtr, DirectoryEntry.LumpStartOffset, SEEK_SET);
			}
			if (NULL != LumpSize)
				*LumpSize = DirectoryEntry.LumpSize;
			return 1;
		}
	}

	fseek(WadFilePtr, startingOffset, SEEK_SET);
	return 0;
}

int GameWad::loadThings(long StartOffset, long NumItems)
{
	//
	// This will load the various "Thing" definitions.
	//
	
	Things = new Thing[NumItems];
	long originalOffset = 0;

	originalOffset = ftell(WadFilePtr);
	fseek(WadFilePtr, StartOffset, SEEK_SET);
	fread((void *)&Things[0], sizeof(Thing), NumItems, WadFilePtr); 
	fseek(WadFilePtr, originalOffset, SEEK_SET);

	for (int i=0; i< NumItems; ++i)
		if (Things[i].ThingType == 1)
		{
			PlayerX = Things[i].StartX;
			PlayerY = Things[i].StartY;
			PlayerAngle = Things[i].StartAngle;			
		}

	return 1;
}

int GameWad::loadLineDefs(long StartOffset, long NumItems)
{	
	Linedefs = new LineDef[NumItems];
	long originalOffset = 0;

	originalOffset = ftell(WadFilePtr);
	fseek(WadFilePtr, StartOffset, SEEK_SET);
	fread((void *)&Linedefs[0], sizeof(LineDef), NumItems, WadFilePtr); 
	fseek(WadFilePtr, originalOffset, SEEK_SET);

	return 1;
}

int GameWad::loadSideDefs(long StartOffset, long NumItems)
{	
	SideDefLdr* TmpSidedefs = new SideDefLdr[NumItems];
	int i = 0;

	Sidedefs = new SideDef[NumItems];
	long originalOffset = 0;


	// Load the sideDefs from the WAD file.

	originalOffset = ftell(WadFilePtr);
	fseek(WadFilePtr, StartOffset, SEEK_SET);
	fread((void *)&TmpSidedefs[0], sizeof(SideDefLdr), NumItems, WadFilePtr); 
	fseek(WadFilePtr, originalOffset, SEEK_SET);

	// Now move them into the final holding structure.

	for( i = 0; i < NumItems; ++i )
	{
		Sidedefs[i].TxtureXOffset = TmpSidedefs[i].TxtureXOffset;
		Sidedefs[i].TxtureYOffset = TmpSidedefs[i].TxtureYOffset;
		
		strncpy(Sidedefs[i].UpperTxtName,TmpSidedefs[i].UpperTxtName,8);
		Sidedefs[i].UpperTxtName[8] = '\0';
		Sidedefs[i].UpperTxtPtr = getTexturePtr(Sidedefs[i].UpperTxtName);
		
		strncpy(Sidedefs[i].LowerTxtName,TmpSidedefs[i].LowerTxtName,8);
		Sidedefs[i].LowerTxtName[8] = '\0';
		Sidedefs[i].LowerTxtPtr = getTexturePtr(Sidedefs[i].LowerTxtName);
		
		strncpy(Sidedefs[i].NormalTxtName,TmpSidedefs[i].NormalTxtName,8);
		Sidedefs[i].NormalTxtName[8] = '\0';
		Sidedefs[i].NormalTxtPtr = getTexturePtr(Sidedefs[i].NormalTxtName);
		
		Sidedefs[i].SectorNumber = TmpSidedefs[i].SectorNumber;
	}

	delete[] TmpSidedefs;
	return 1;
}

int GameWad::loadVertexes(long StartOffset, long NumItems)
{
	long originalOffset = 0;
	
	Vertices = new Vertex[NumItems];
	NbrVertices = NumItems;
	originalOffset = ftell(WadFilePtr);
	fseek(WadFilePtr, StartOffset, SEEK_SET);
	fread((void *)&Vertices[0], sizeof(Vertex), NumItems, WadFilePtr); 
	fseek(WadFilePtr, originalOffset, SEEK_SET);

	return 1;
}

int GameWad::loadSegments(long StartOffset, long NumItems)
{
	Segs = new Seg[NumItems];
	long originalOffset = 0;

	originalOffset = ftell(WadFilePtr);
	fseek(WadFilePtr, StartOffset, SEEK_SET);
	fread((void *)&Segs[0], sizeof(Seg), NumItems, WadFilePtr); 
	fseek(WadFilePtr, originalOffset, SEEK_SET);


	//
	// Now populate the SegNode's 
	//

	return 1;
}

int GameWad::loadSSectors(long StartOffset, long NumItems)
{
	SSectors = new SSector[NumItems];
	long originalOffset = 0;

	originalOffset = ftell(WadFilePtr);
	fseek(WadFilePtr, StartOffset, SEEK_SET);
	fread((void *)&SSectors[0], sizeof(SSector), NumItems, WadFilePtr); 
	fseek(WadFilePtr, originalOffset, SEEK_SET);

	//
	// Now populate teh SSectorNodes
	//

	return 1;
}


int GameWad::loadNodes(long StartOffset, long NumItems)
{
	long originalOffset = 0;

	Nodes = new Node[NumItems+1];
	MaxNode = NumItems - 1;
	originalOffset = ftell(WadFilePtr);
	fseek(WadFilePtr, StartOffset, SEEK_SET);
	fread((void *)&Nodes[0], sizeof(Node), NumItems, WadFilePtr); 
	fseek(WadFilePtr, originalOffset, SEEK_SET);
	return 1;
}


int GameWad::loadSectors(long StartOffset, long NumItems)
{
	Sectors = new Sector[NumItems];
	long originalOffset = 0;

	originalOffset = ftell(WadFilePtr);
	fseek(WadFilePtr, StartOffset, SEEK_SET);
	fread((void *)&Sectors[0], sizeof(Sector), NumItems, WadFilePtr); 
	fseek(WadFilePtr, originalOffset, SEEK_SET);

	return 1;
}

int GameWad::loadPalettes( void )
{
	//
	// This will load the 14 palettes used by the system.
	//
	for (int i=0; i<14; ++i)
		fread((void*)&GamePalettes[i][0], 768, 1, WadFilePtr);
	return 1;	
}

int GameWad::loadColorMaps( void )
{
	//
	// This will load the 34 palettes used by the system.
	//
		
	for (int i=0; i<34; ++i)
		fread((void*)&ColorMap[i][0], 256, 1, WadFilePtr);

	return 1;
}

int GameWad::loadPNames( void )
{
	long NumItems = 0;

	// Read the number of Pnames
	fread((void*)&NumItems, sizeof(long), 1, WadFilePtr);
	Pnames = new PName[NumItems];

	// Read each PName and save it in the array.
	for (int i=0; i<NumItems; ++i)
	{
		Pnames[i].index = i;
		fread((void*)&Pnames[i].Name[0], 1, 8, WadFilePtr);
		Pnames[i].Name[8] = '\0';
	}

	return 1;
}

int GameWad::loadTextures( void )
{
	//
	// This will load the various Texture definitions.
	//
	long           StartIndex = 0;
	long           StartOffset = 0;
	long           nextoffset = 0;
	long           NumItems = 0;
	long*		   TxtOffset;
	int            i = 0, j=0;
	TextureLdrRec  TextureRecord;
	PatchDscr*	   TxtPatchRec;
	Texture*       CurrTexture = NULL;

	StartOffset = ftell(WadFilePtr);

	// Read the number of Textures
	fread((void*)&NumItems, sizeof(long), 1, WadFilePtr);

	// Read the texture offsets
	TxtOffset = new long[NumItems];
	fread((void*)TxtOffset, sizeof(long), NumItems, WadFilePtr);

	// For each texture definition:
	for (i=0; i<NumItems; ++i)
	{
        // Create the texture entry.
		CurrTexture = new Texture;
		CurrTexture->nextTxt = NULL; 
		CurrTexture->PrevTxt = NULL;
				
	    if( NULL == Textures )
	    { 
			Textures = CurrTexture;
			NbrTextures = 1;
		}
		else
		{
			CurrTexture->nextTxt = Textures;
			Textures->PrevTxt = CurrTexture;
			Textures = CurrTexture;
			NbrTextures++;
		}
		
		// Get the texture header.
		fseek(WadFilePtr, TxtOffset[i] + StartOffset, SEEK_SET);
		fread((void*)&TextureRecord, sizeof(TextureLdrRec), 1, WadFilePtr);

		// Set up the texture item in the array.
		strncpy(CurrTexture->TxtName, TextureRecord.TxtName, 8);
		CurrTexture->TxtName[8] = '\0';
		CurrTexture->TxtWidth = TextureRecord.TxtWidth;
		CurrTexture->TxtHeight = TextureRecord.TxtHeight;
		CurrTexture->TxtPtr = new char[(unsigned long)TextureRecord.TxtWidth * (unsigned long)TextureRecord.TxtHeight];
		memset( CurrTexture->TxtPtr, 0, (unsigned long)TextureRecord.TxtWidth * (unsigned long)TextureRecord.TxtHeight);

	
		TxtPatchRec = new PatchDscr[TextureRecord.NumPatches];
		
		if (NULL != TxtPatchRec)
			fread((void*)&TxtPatchRec[0], sizeof(PatchDscr), TextureRecord.NumPatches, WadFilePtr);

		// Now, for each texture patch, 
		for (j=0;((NULL != TxtPatchRec) && (j< TextureRecord.NumPatches)); ++j)
		{
			long PatchWidth = 0;
			long PatchHeight = 0;
			long TxtPatchStartX = TxtPatchRec[j].StartX;
			long TxtPatchStartY = TxtPatchRec[j].StartY;
			
			// Now load the patch data
			char* Patch = ParseTextureData(Pnames[TxtPatchRec[j].PnameNbr].Name, PatchWidth, PatchHeight);

			// and add it to the current texture 
			// (Let's transpose the texture too. It'll make rendering MUCH faster!)

			if( NULL != Patch )
			{
				for( long col = TxtPatchStartX; col < TxtPatchStartX + PatchWidth; ++col )
					for( long row = TxtPatchStartY; row < TxtPatchStartY + PatchHeight; ++row )
						if (((col >= 0) && (col < CurrTexture->TxtWidth)) && ((row >= 0) && (row < CurrTexture->TxtHeight)))
							*(CurrTexture->TxtPtr + col * CurrTexture->TxtHeight + row  ) =
								*(Patch + col - TxtPatchStartX + (row - TxtPatchStartY) * PatchWidth );
				delete Patch;
			}


		}

		if (NULL != TxtPatchRec)
			delete TxtPatchRec;	
	}

	delete TxtOffset;


	return 1;
}

char* GameWad::ParseTextureData( const char* DirectoryName, long& Width, long& Height)
{
	long          OrigOffset = 0;
	long          PatchLumpStart = 0;
	long          row = 0;
	unsigned char firstRow;
	unsigned char numPels;
	long          StartRow = 0;
	long*         ColumnOffsets;
	char*         PatchData = NULL;
	char          bt;
	PatchLdrRec   PatchHdr;

	Width = 0;
	Height = 0;

	// Find the Patch Lump in the WAD file.
	OrigOffset = ftell(WadFilePtr);
	if (findDirectoryEntry(DirectoryName))
	{

		PatchLumpStart = ftell(WadFilePtr);
		// If found, load the header for the patch.
		fread((void*)&PatchHdr, sizeof(PatchLdrRec), 1, WadFilePtr);
		PatchData = new char[PatchHdr.Width * PatchHdr.Height];
		memset( PatchData, 0, PatchHdr.Width * PatchHdr.Height );
		Width = (long)PatchHdr.Width;
		Height = (long)PatchHdr.Height;
		
		// Now read the offsets to the columns.
		ColumnOffsets = new long[PatchHdr.Width];
		fread((void*)&ColumnOffsets[0], sizeof(long), PatchHdr.Width, WadFilePtr);

		// Now, for each column, there will be an offset pointer.
		for( int i=0; i< PatchHdr.Width; ++i)
		{
			// Go to the column.
			fseek(WadFilePtr, ColumnOffsets[i] + PatchLumpStart, SEEK_SET);
			// Parse the posts.
			firstRow = fgetc(WadFilePtr);
			while(firstRow != 0xFF )
			{
				numPels = fgetc(WadFilePtr);
				fgetc(WadFilePtr);
				for( row = firstRow; row < firstRow + numPels; ++row )
				{
					bt = (unsigned char)getc(WadFilePtr);
					if( row >= 0 && row < PatchHdr.Height)
						*(PatchData + (long)row*(PatchHdr.Width) + i ) = bt;
				}
				getc(WadFilePtr);
				firstRow = fgetc(WadFilePtr);
			}
		}
	}		
	fseek(WadFilePtr, OrigOffset, SEEK_SET);

	return PatchData;
}

void  GameWad::getPlayerStart( short& X, short& Y, short& Ang) 
{
	X = PlayerX;
	Y = PlayerY; 
	Ang = PlayerAngle; 
}

int GameWad::Save_Pcx(char *filename, unsigned char  *Buffer, unsigned long width, unsigned long height) 
{ 
    unsigned long count,     // the count for the RLE run. 
                  bcount,    // the count for the line. 
                  index,     // Overall byte count. 
                  TotalSize; 
    unsigned char data;      // The data byte written. 
    FILE          *outfile;  // The file pointer. 
    PCX_HEADER    header;    // The PCX header info. 

    outfile = fopen(filename, "wb"); 
    if(NULL == outfile) { 
        return -1; 
    } 
 
    // Build a PCX file header. Must be little endian, so beware 
    header.manufacturer = 10; 
    header.version = 5; 
    header.encoding = 1; 
    header.bits_per_pixel = 8; 
    header.x = 0; 
    header.y = 0;  
    header.width = (short)width-1; 
    header.height = (short)height-1; 
    header.horiz_rez = (short)width; 
    header.vert_rez = (short)height; 
    header.Num_Planes = 1; 
    header.bytes_per_line = (short)width; 
    header.palette_type = 1; 
 
    // Write the PCX file header 
    fwrite((void *)&header,sizeof(PCX_HEADER),1,outfile); 
 
    //  Do run length encoding for pixel runs of 2 to 63.  Add 192 to the 
    //  run count, and write it before the pixel to indicate how many are 
    //  present. Runs are not encoded across line boundaries, so 
    //  bcount keeps track of how many bytes have been written per line, 
    //  and when it hits "width", the run is written even if less than 63. 
 
    bcount = 0; 
    index = 0; 
    TotalSize = width * height; 
    while (index < TotalSize) 
    { 
        if((index < TotalSize-1) && (Buffer[index] == Buffer[index + 1]) && (bcount < width-1)) 
        { 
            // We have a run of two or more 
            data = Buffer[index]; 
            count = 2; 
			while((index + count < TotalSize) && 
				  (data == Buffer[index + count]) && 
				  (count < 63) && (bcount + count < width) ) 
                    count++; 
            index = index + count; 
            bcount = bcount + count; 
            putc((char)(count + 192), outfile); 
            putc(data, outfile); 
        } 
        else 
        { 
            // We are writing one byte.  If byte is 192-255, put a run count of 1 (193) in front of it 
            if(Buffer[index] >= 192) 
                putc((char)193, outfile); 
            putc((char)Buffer[index], outfile); 
            index++; 
            bcount++; 
        } 
        if(bcount == width)
            bcount = 0; 
    } 
 
    // Write the palette 
    // 256 color palette must be prefixed with this byte 
    putc((char)12, outfile); 
    for(index=0; index<768; index++) 
		putc((char)GamePalettes[0][index], outfile);

    fclose(outfile); 
	return 1;
}

/*
char* GameWad::getBSPTree( void )
{
	// Return a pointer to the BSP tree.

}
*/

RGBQUAD* GameWad::getPalette( int paletteNbr )
{
	RGBQUAD *palettePtr = new RGBQUAD[256];

	for (int i= 0; i < 256; ++i)
	{
		palettePtr[i].rgbRed = GamePalettes[0][i*3]; 
		palettePtr[i].rgbGreen = GamePalettes[0][i*3+1]; 
		palettePtr[i].rgbBlue = GamePalettes[0][i*3+2]; 
	}

	return palettePtr;
}

Texture* GameWad::getTexturePtr( const char* txtName )
{
	//
	// this method will search the array of loaded textures, and find the one with the
	// specified name. If found, it will place the width and height into the appropriate
	// reference parameters, and return the pointer to the texture data. If not found, it
	// will set the reference parameters to 0, and return NULL.
	//
	Texture* currTxt = NULL;

	currTxt = Textures;

	if( txtName[0] == '-' )
		return NULL;

	while( currTxt != NULL )
	{
		if( 0 == strncmp(currTxt->TxtName, txtName,8))
		{
			return currTxt;
		}
		currTxt = currTxt->nextTxt;
	}
	//
	// TODO: Print error box. Can't find texure...
	//
	return NULL;
}