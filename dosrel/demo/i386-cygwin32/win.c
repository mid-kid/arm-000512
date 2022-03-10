#include <windows.h>

main()
{
  WinMain (0,0,0,1);
}

int WINAPI WndProc (HWND hwnd, UINT message, int wParam, long lParam)
{
  HDC   hdc ;
  PAINTSTRUCT ps ;
  RECT  rect ;

  switch (message)
    {
    case WM_PAINT :
      hdc = BeginPaint (hwnd, &ps) ;

      GetClientRect (hwnd, &rect) ;

      DrawText (hdc,	"Hello, Windows, I'm totally free!", -1, &rect,
		DT_SINGLELINE | DT_CENTER | DT_VCENTER) ;

      EndPaint (hwnd, &ps) ;
      return 0 ;

    case WM_DESTROY :
      PostQuitMessage (0) ;
      return 0 ;
    }

  return DefWindowProc (hwnd, message, wParam, lParam) ;
}

int WINAPI WinMain (HINSTANCE hInstance, 
		    HINSTANCE hPrevInstance,
		    char * lpszCmdParam,
		    int nCmdShow)
{
  static char szAppName[] ="Hello World" ;
  HWND  hwnd ;
  MSG   msg ;
  WNDCLASS wndclass ;

  if (!hPrevInstance)
    {
      wndclass.style   = CS_HREDRAW | CS_VREDRAW ;
      wndclass.lpfnWndProc   = WndProc ;
      wndclass.cbClsExtra = 0 ;
      wndclass.cbWndExtra = 0 ;
      wndclass.hInstance  = hInstance ;
      wndclass.hIcon   = LoadIcon (NULL, IDI_APPLICATION) ;
      wndclass.hCursor    = LoadCursor (NULL, IDC_ARROW) ;
      wndclass.hbrBackground = GetStockObject (WHITE_BRUSH) ;
      wndclass.lpszMenuName  = NULL ;
      wndclass.lpszClassName = szAppName ;

      RegisterClass (&wndclass) ;
    }

  hwnd = CreateWindow (szAppName,    
		       szAppName,
		       WS_OVERLAPPEDWINDOW,   
		       CW_USEDEFAULT,   
		       CW_USEDEFAULT,   
		       CW_USEDEFAULT,   
		       CW_USEDEFAULT,   
		       NULL,      
		       NULL,      
		       hInstance,    
		       NULL) ;    

  ShowWindow (hwnd, nCmdShow) ;
  UpdateWindow (hwnd) ;
  while (GetMessage (&msg, NULL, 0, 0))
    {
      TranslateMessage (&msg) ;
      DispatchMessage (&msg) ;
    }
  return msg.wParam ;
}


