#ifndef DOOMBASICTYPES
#define DOOMBASICTYPES

#include <stdio.h>

#define ANGLE_360 0x10000
#define ANGLE_315 0x0E000
#define ANGLE_270 0x0C000
#define ANGLE_225 0x0A000
#define ANGLE_180 0x08000
#define ANGLE_135 0x06000
#define ANGLE_90  0x04000
#define ANGLE_45  0x02000

struct Texture
{
	char  TxtName[9];
	short TxtWidth;
	short TxtHeight;
	char* TxtPtr;
	Texture* nextTxt;
	Texture* PrevTxt;

	Texture( void) { TxtPtr = NULL; };
	~Texture( void ) { if(NULL != TxtPtr) delete TxtPtr; };
};

typedef struct PCX_HEADER_TYPE  
{  
    char  manufacturer;    // We don't care about these...  
    char  version;  
    char  encoding;  
    char  bits_per_pixel;  // We want an 8 here...  
    short x,y;             // We ignore these.  
    short width,height;    // Will be either 64x64 or 320x200  
    short horiz_rez;       // Forget about these...  
    short vert_rez;  
    char  EGA_palette[48]; // We won't even touch this.  
    char  reserved;  
    char  Num_Planes;      // We want 1  
    short bytes_per_line;  // Either 64 or 320  
    short palette_type;    // We can forget about these too..  
    char  padding[58];  
} PCX_HEADER;  

struct Thing
{
	short StartX;
	short StartY;
	short StartAngle;
	short ThingType;
	short ThingOptions;
};

struct WadHdrLoad
{
	char Type[4];
	long NbrLumps;
	long DirectoryOffset;
};

struct DirectoryEntryLoad
{
	long LumpStartOffset;
	long LumpSize;
	char LumpName[8];
};

struct LineDef
{
	short StartVertex;
	short EndVertex;
	short Attributes;
	short SpecialEffect;
	short EffectTagNumber;
	short RightSideDef;
	short LeftSideDef;
	// LeftSectorNbr
	// RightSectorNbr
};

struct SideDefLdr
{
	short TxtureXOffset;
	short TxtureYOffset;
	char  UpperTxtName[8];
	char  LowerTxtName[8];
	char  NormalTxtName[8];
	short SectorNumber;
};

struct SideDef
{
	short TxtureXOffset;
	short TxtureYOffset;
	char  UpperTxtName[9];
	Texture* UpperTxtPtr;
	char  LowerTxtName[9];
	Texture* LowerTxtPtr;
	char  NormalTxtName[9];
	Texture* NormalTxtPtr;
	short SectorNumber;
};

struct Vertex
{
	short x;
	short y;
};

struct Seg
{
	short StartVertex;
	short EndVertex;
	short Angle;
	short Linedef;
	short Direction;
	short StartOffset;
};

struct SegNode
{
	short StartVertex;
	short EndVertex;
	short Angle;
	short Linedef;
	short Direction;
	short StartOffset;
	SideDef Sides[2];
};


struct SSector
{
	short NumSegs;
	short StartSeg;
};

struct SSectorNode
{
	short NumSegs;
	short StartSeg;
	short Sector;
	short FloorHeight;
	short CeilingHeight;
	char* FloorTxt;
	char* CeilTxt;
	short LightLevel;
	Thing *things;
};


struct Sector
{
	short FloorHeight;
	short CeilingHeight;
	char  FloorTxtName[8];
	char  CeilTxtName[8];
	short LightLevel;
	short SpecialFlags;
	short EffectTagNumber;
};

struct Node
{
	short CutPlaneStartX;
	short CutPlaneStartY;
	short CutPlaneDx;
	short CutPlaneDy;
	short RightBBUpperY;
	short RightBBLowerY;
	short RightBBLowerX;
	short RightBBUpperX;
	short LeftBBUpperY;
	short LeftBBLowerY;
	short LeftBBLowerX;
	short LeftBBUpperX;
	short RightChild;
	short LeftChild;
};

struct TextureLdrRec
{
	char TxtName[8];
	short pad1, pad2;
	short TxtWidth;
	short TxtHeight;
	short pad3, pad4;
	short NumPatches;
};

struct PatchDscr
{
	short StartX;
	short StartY;
	short PnameNbr;
	short StepDir;
	short ColorMap;
};

struct PName
{
	int  index;
	char Name[9];
};


struct PatchLdrRec
{
	short Width;
	short Height;
	short XOffset;
	short YOffset;
};

#endif
