#include "utils/directx.h"
#include "tester.h"
#include <Shlwapi.h>

static const wchar_t* kClassName = L"foobar";


LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg ) {
	case WM_DESTROY:
		PostQuitMessage( 0 );
		return 0;
	case WM_KEYDOWN:
		if( wParam == VK_ESCAPE )
			PostQuitMessage( 0 );
		else
			tester::HandleKeyboard (wParam);
		return 0;
	case WM_PAINT:
		// TBD: draw
		ValidateRect( hWnd, NULL );
		return 0;
    }
    return DefWindowProc( hWnd, msg, wParam, lParam );
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	// change to folder containing the exe
	TCHAR modulePath[MAX_PATH];
	GetModuleFileName (NULL, modulePath, MAX_PATH);
	PathAppend (modulePath, L"..");
	SetCurrentDirectory (modulePath);

    // register the window class
    WNDCLASSEX wc = {
		sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, 
		hInstance, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, kClassName, NULL
	};
    RegisterClassEx( &wc );

    // create the window
	RECT rc = { 40, 40, 40+d3d::SCR_WIDTH, 40+d3d::SCR_HEIGHT };
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    HWND hWnd = CreateWindow( kClassName, L"Tester", WS_OVERLAPPEDWINDOW,
		rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top,
		NULL, NULL, wc.hInstance, NULL );

    // initialize
	if( d3d::initialize( hWnd ) ) {

		tester::Initialize();

        // Show the window
        ShowWindow( hWnd, SW_SHOWDEFAULT );
        UpdateWindow( hWnd );

        // message loop
        MSG msg; 
		msg.message = WM_NULL;

		PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE );
        while( WM_QUIT != msg.message ) {
			if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			} else {
				bool cont = tester::Perform();
				if( !cont )
					PostQuitMessage( 0 );
			}
        }

		tester::Shutdown();
    }

	d3d::shutdown();
    UnregisterClass( kClassName, wc.hInstance );

	return 0;
}
