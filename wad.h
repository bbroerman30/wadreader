#ifndef DOOMWADHDR
#define DOOMWADHDR
#include <stdio.h>
#include <windows.h>
#include "types.h"

class GameWad
{
	friend class Renderer;

private:
	//
	// Private Data
	//
	int       WadFileInitialized;
	char      WadFileName[256];
	FILE*     WadFilePtr;
	long      DirectoryOffset;
	long      NumberOfLumps;
	short     MaxNode;
	long      NbrVertices;
	long      NbrTextures;
	//
	//  Game information
	//
	unsigned char GamePalettes[14][768];
	char      ColorMap[34][256];
	Thing*    Things;
	Vertex*   Vertices;
	LineDef*  Linedefs;
	Seg*      Segs;
	Sector*   Sectors;
	SSector*  SSectors;
	SideDef*  Sidedefs;
	Node*     Nodes;
	PName*    Pnames;
	Texture*  Textures;
	short	  PlayerX;
	short	  PlayerY;
	short     PlayerAngle;

public:
	//
	// Public Methods.
	//
	GameWad( void );
	GameWad( char* Fname );
	~GameWad( void );
	int   InitWadFile( char* Fname );
	int   LoadMap( int episode, int level );
	void  getPlayerStart( short& X, short& Y, short& Ang);
	Texture* getTexturePtr( const char* txtName );
    char* getBSPTree( void );
	RGBQUAD* getPalette( int paletteNbr );

private:
	//
	// Private Methods.
	//
	int   clearMap(void);
	int   loadPalettes( void );
	int   loadColorMaps( void );
	int   loadPNames( void );
	int   loadTextures( void );
	int   loadThings(long StartOffset, long NumItems);
	int   loadLineDefs(long StartOffset, long NumItems);
	int   loadSideDefs(long StartOffset, long NumItems);
	int   loadVertexes(long StartOffset, long NumItems);
	int   loadSegments(long StartOffset, long NumItems);
	int   loadSSectors(long StartOffset, long NumItems);
	int   loadNodes(long StartOffset, long NumItems);
	int   loadSectors(long StartOffset, long NumItems);
	int   findDirectoryEntry(const char* SearchName, long* LumpSize = NULL) ;
	char* ParseTextureData( const char* DirectoryName, long& Width, long& Height);
	int   Save_Pcx(char *filename, unsigned char  *Buffer, unsigned long width, unsigned long height);
};

#endif