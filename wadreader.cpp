#include <windows.h>
#include <time.h>
#include "Wad.h"
#include "Render.h"

// For keyboard handling (in moving player), these query the keyboard in real-time
#define KEY_DOWN(vk_code)	((GetAsyncKeyState(vk_code) & 0x8000) ? 1 : 0)
#define KEY_UP(vk_code)		((GetAsyncKeyState(vk_code) & 0x8000) ? 0 : 1)

// Information needed by the application itself.
HWND ghWnd;			// Handle to the main window.
BOOL bIsActive;		// Tells us when we should be rendering, and when we should stop.
BOOL bRunGame;		// Tells us wether we are running the game or showing the title screen.
HINSTANCE hInst;	// current application instance
LPCTSTR lpszAppName  = "WadReader"; // The application and window class name.
LPCTSTR lpszTitle    = "WadReader"; // The main window title.
short ClientWidth;
short ClientHeight;
short ClientxPos;
short ClientyPos;

// Pointers to the WAD file and Rendering engine.
GameWad*   TheWadFile;
Renderer*  GameRenderer;

short PlayerX;
short PlayerY;
short PlayerHeight;
short ViewAngle;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

//
// This copies the double buffer to the visible window. It also performs the stretching
// when the window is not the default size.
//
void blit( HBITMAP DblBuff )
{
	HDC hDC, hMemDC;
	HBITMAP DefaultBitmap;
	hDC = GetDC(ghWnd);
	
	// Create memory device context compatible with the engines window dc
	hMemDC = CreateCompatibleDC(GetDC(ghWnd));

	// Save the default bitmap and select the engines double buffer into the memory dc
	DefaultBitmap = (HBITMAP)SelectObject(hMemDC, DblBuff);

	// Copy double buffer to visible windows client area
	StretchBlt(hDC, 0, 0, ClientWidth, ClientHeight, hMemDC, 0, 0, 640, 400,  SRCCOPY);

	// put back the default bitmap
	SelectObject(hMemDC, DefaultBitmap);
	
	// delete the memory dc
	DeleteDC(hMemDC);

	// delete the default bitmap
	DeleteObject(DefaultBitmap);

	// release the engines window dc 
	ReleaseDC(ghWnd,hDC);
}

//
// This is a delay that can be cancelled by pressing SPACE or ESCape. 
// It is used during the start-up screens. 
//
void delay2(long time) // time is in seconds. 
{ 
    time_t start; 
 
    start= clock(); 
    while (KEY_DOWN(VK_ESCAPE) || KEY_DOWN(VK_SPACE)) // Wait for the keys to clear. 
        ; 
    while ((clock() - start) < time*CLK_TCK) // Start the delay 
    { 
        if (KEY_DOWN(VK_ESCAPE) || KEY_DOWN(VK_SPACE)) // if space or ESC, break out. 
            break; 
    } 
} 

//
// This prints an error message, clears all the tables, and informs the
// message loop that we need to exit the program.
//
int Die(char *string)
{
    MessageBox(ghWnd, string, lpszTitle, MB_APPLMODAL | MB_ICONERROR | MB_OK );
    SendMessage(ghWnd, WM_DESTROY, 0, 0);
	return -1;
}
// 
//     This method performs the user movement and collision detection.
//
void ProccessUserMovement( void )
{
	short dx, dy;  // Deltas to add to the position.
	short x, y;    // The new positions.
	
	x = PlayerX;
	y = PlayerY;

    // Step 1. Check for movement. The lower nibble of KeyState is for movement ONLY. ( Arrow Keys )
    if ( KEY_DOWN(VK_LEFT) || KEY_DOWN(VK_RIGHT) || KEY_DOWN(VK_UP) || KEY_DOWN(VK_DOWN) )
    { 
        // Reset dx and dy. 
        dx=dy=0; 
        // If the player is pressing the Right arrow, 
        if (KEY_DOWN(VK_LEFT)) 
        { 
            // Decrement the view angle by 5 degrees, wrap at zero. 
            ViewAngle += 910; 
        } 
        // Else, if the player is pressing the Left arrow. 
        if (KEY_DOWN(VK_RIGHT)) 
        { 
            // Increment the view angle by 5 degrees, Wrap at 360. 
            ViewAngle -= 910; 
        } 
        // Else, if the player wants to go FORWARD... 
        if (KEY_DOWN(VK_UP)) 
        { 
            // Calculate the dx and dy to move forward, at the current angle. (Trig) 
            dx = (short)((GameRenderer->cos(ViewAngle)*50l)>>16); 
            dy = (short)((GameRenderer->sin(ViewAngle)*50l)>>16); 
        } 
        // Else, if the player wants to move BACKWARDS 
        if (KEY_DOWN(VK_DOWN)) 
        { 
            // Calculate the dx and dy to move backward, at the current angle. (Trig) 
            dx = -(short)((GameRenderer->cos(ViewAngle)*50l)>>16); 
            dy = -(short)((GameRenderer->sin(ViewAngle)*50l)>>16); 
        } 

        // Now , add the deltas to the current location, 
        x += dx; 
        y += dy; 
            
        // Step 4. Now, we do collision detection... (Bumping into walls..) 
		//         Check the blockmap for the linedefs in this block
		//         See if we're crossing any.
		//         Are they 1 sided or 2?
		//         Are they passable or not?
		//         If not, then don't allow movement
		//         If they are, then Trigger any special effects.

		PlayerX = x;
		PlayerY = y;
		PlayerHeight = GameRenderer->changeViewPosn( PlayerX, PlayerY, ViewAngle );
	}
	return;
}

//
//   This method sets up the rendering engine.
//    
int StartEngine(void) 
{ 
	//
	// Load the WAD file.
	//
	TheWadFile = new GameWad();
	if( -1 == TheWadFile->InitWadFile(".\\Doom.wad") )
	{
		MessageBox(ghWnd, "Unable to load WAD file!","Error", 0 );
		delete TheWadFile;
		exit(-1);
	}

	TheWadFile->LoadMap(1,2);
	TheWadFile->getPlayerStart(PlayerX, PlayerY, ViewAngle);
	ViewAngle = (short)(ViewAngle * 184.04444444); // Convert to BAM angle.
	
	//Initialize the Rendering engine.
	GameRenderer = new Renderer(ghWnd, TheWadFile);
	GameRenderer->changeViewPosn( PlayerX, PlayerY, ViewAngle );
	
	return 1;
} 

//
//     This method performs the main game loop. It calls a set of methods to do background processing,
//  then it performs the user movement, and then processing of triggers. Finally, it renders the view.
//
void MainLoop(void)
{
    // Step 1. Process background things (doors, animations, lights, etc.)

	// Step 2. Allow the user to move
	ProccessUserMovement();
	
    // Step 3. Render the new view.
	GameRenderer->render(PlayerX, PlayerY);
	blit(GameRenderer->getDIB());

	return;
}

//
//    The main entry function. It creates the main widow, and calls the necessary functions to 
// initialize the engine, and then enters the main message loop.
//
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	HWND hWnd; 
	WNDCLASSEX wc;
	bIsActive = FALSE;
	bRunGame = FALSE;

	// Register the main application window class. 
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)WndProc;       
	wc.cbClsExtra = 0;                      
	wc.cbWndExtra = 0;                      
	wc.hInstance = hInstance;              
	wc.hIcon = LoadIcon(hInstance, lpszTitle); 
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;              
	wc.lpszClassName = lpszAppName;              
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hIconSm = (HICON)LoadImage(hInstance, lpszTitle, IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

	if(!RegisterClassEx(&wc))
	{
		return(FALSE);
	}

	hInst = hInstance; 

	// Create the main application window.
	hWnd = CreateWindowEx(WS_OVERLAPPED, lpszAppName, lpszTitle, WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME, 
			      50, 50, (640 + 6), (400 + 44), NULL, NULL, hInstance, NULL);

	if(!hWnd)
	{
		return(FALSE);
	}

	ShowWindow(hWnd, nCmdShow); 
	UpdateWindow(hWnd); 
	ghWnd = hWnd;
	
	StartEngine() ;
	bRunGame = TRUE;

	// We're going to try and use the windows timer to set the upper limit to 60 frames / sec.
	SetTimer(ghWnd, 1, 16, NULL);

	// enter main event loop
	while(1) 
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// test if this is a quit
			if(msg.message == WM_QUIT)
			{
				break;
			}
	
			// translate any accelerator keys
			TranslateMessage(&msg);

			// send the message to the window proc
			DispatchMessage(&msg);
		}
	}

	KillTimer(ghWnd, 1);

	return(msg.wParam); 
}

//
//     The winproc for the main window handles messages from the menu bar, and from user input.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;

	switch(message){
		case WM_ACTIVATEAPP:
			bIsActive = (BOOL) wParam;
			break;

		case WM_KEYDOWN:
			// Check to see if the user wants to see the auto map.
			if (((int)wParam == VK_TAB ) && (bRunGame == TRUE ))
			{
				// Render Automap
			}
			
			// Check to see if the user wants to quit (ESC KEY).
			if((int)wParam == VK_ESCAPE)
				PostMessage(ghWnd,WM_CLOSE,0,0);

			// Check to se if the user is requesting a screen shot ( F1 Key ). 
		    if ((int)wParam == VK_F1)
			{
				GameRenderer->Save_Pcx("scrnsht.pcx"); 
			}

		    // Now check to see if the player wants to change the torch level ( F2 Key ). 
			if (((int)wParam == VK_F2) && (bRunGame == TRUE ))
			{
				// Change the ambient light level.
			}
			
		    // Check to see if the player wants to open a door ( SPACE key ). 
			if (((int)wParam == VK_SPACE) && (bRunGame == TRUE ))
			{ 
				// This will activate a linedef just in front of us.
			} 			
			break;

		case WM_SIZE:
			ClientWidth = LOWORD(lParam);
			ClientHeight = HIWORD(lParam);
			break;

		case WM_MOVE :
			ClientxPos = (int) LOWORD(lParam); 
			ClientyPos = (int) HIWORD(lParam);
			break;

		case WM_PAINT:
			BeginPaint(hWnd, &ps );
			EndPaint(hWnd, &ps );
			break;

		case WM_TIMER:
			if( bRunGame == TRUE )
			{
				MainLoop();  // call main logic module.
			}
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
} 