#include "InstanceData.hpp"
#include "resource.h"
#include "targetver.h"

#include <cguid.h>
#include <cinttypes>
#include <ctime>
#include <shellapi.h>
#include <tchar.h>


BOOL             InitInstance(HINSTANCE);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AboutDialogMessageHandler(HWND, UINT, WPARAM, LPARAM);


void RegisterWindowClass(HINSTANCE hInstance)
{
	auto windowClass = WNDCLASSEXW{0};

    windowClass.cbSize        = sizeof(WNDCLASSEX);
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = WndProc;
    windowClass.cbClsExtra    = 0;
    windowClass.cbWndExtra    = 0;
    windowClass.hInstance     = hInstance;
    windowClass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MONITORKEEPER));
    windowClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClass.lpszMenuName  = MAKEINTRESOURCEW(IDC_MONITORKEEPER);
    windowClass.lpszClassName = _T("MONITORKEEPER");
    windowClass.hIconSm       = LoadIcon(windowClass.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassExW(&windowClass);
}


int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    RegisterWindowClass(hInstance);
    if (!InitInstance(hInstance))
        return FALSE;

    const auto acceleratorsHandle = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MONITORKEEPER));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, acceleratorsHandle, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}


// Called by EnumDesktopWindows whenever a window changes state.
// This will capture a lot of events.
BOOL CALLBACK SaveWindowsCallback(HWND hwnd, LPARAM lParam)
{
	const auto numMonitors = (int)lParam;

	//
	// only track windows that are visible, don't have a parent, 
	// have at least one style that is in the OVERLAPPEDWINDOW style and
	// do not have the WS_EX_NOACTIVE style.
	//
	// we include WS_EX_TOOLBAR because they sometimes are useful and get
	// moved as well.
	//
	if (IsWindowVisible(hwnd) && GetParent(hwnd) == NULL)
	{
		const auto style = GetWindowLong(hwnd, GWL_STYLE);
		const auto exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
		if (((style & WS_OVERLAPPEDWINDOW) || (exStyle & WS_EX_APPWINDOW)) && !(exStyle & WS_EX_NOACTIVATE))
			InstanceData::Instance.SaveWindow(hwnd, numMonitors);
	}
	return TRUE;
}


// Process when the number of monitors changes. If the monitor count has changed, we attempt to restore.
void ProcessMonitors()
{
	const int numMonitors = GetSystemMetrics(SM_CMONITORS);
	if (numMonitors > 1 && (InstanceData::Instance.IsForcedDisplayChange || numMonitors != InstanceData::Instance.NumMonitors))
		InstanceData::Instance.RestoreWindowPositions(numMonitors);

	InstanceData::Instance.NumMonitors = numMonitors;
	InstanceData::Instance.ProcessingDisplayChange = false;
}


// called by hook for window changes
void ProcessDesktopWindows()
{
	const auto numMonitors = GetSystemMetrics(SM_CMONITORS);

	// If we haven't processed a change of monitors yet, don't save positions until we've repositioned things.
	if (numMonitors != InstanceData::Instance.NumMonitors)
		return;

	if (InstanceData::Instance.EnableLogging)
	{
		TCHAR buffer[64];
		wsprintf(buffer, _T("@ %") _T(PRId64) _T(": monitors=%d\n"), std::time(nullptr), numMonitors);
		InstanceData::Instance.AddLogMessage(buffer);
	}

	InstanceData::Instance.TagWindowsUnused();
	EnumDesktopWindows(NULL, SaveWindowsCallback, numMonitors);
}


// save windows positions after a slight delay
VOID CALLBACK SaveTimerCallback(HWND windowHandle, UINT, UINT_PTR timerId, DWORD)
{
	ProcessDesktopWindows();
	KillTimer(windowHandle, timerId);
}


// Our window hook, grabbing the event when the active window changes.
VOID CALLBACK WinEventProcCallback(HWINEVENTHOOK, DWORD event, HWND windowHandle, LONG objectId, LONG childId, DWORD, DWORD)
{
	auto shouldProcess =
		!InstanceData::Instance.ProcessingDisplayChange
		&& windowHandle != NULL
		&& objectId == OBJID_WINDOW
		&& childId == CHILDID_SELF
		&& event == EVENT_OBJECT_LOCATIONCHANGE;

	if (shouldProcess)
		SetTimer(InstanceData::Instance.MainWindowHandle, 2, 200, SaveTimerCallback);
}


// reposition windows after a slight delay
VOID CALLBACK ProcessMonitorsTimerCallback(HWND windowHandle, UINT, UINT_PTR timerId, DWORD)
{
	ProcessMonitors();
	KillTimer(windowHandle, timerId);
}


HWINEVENTHOOK RegisterWindowsEventCallback()
{
	return SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, NULL, WinEventProcCallback, 0, 0, WINEVENT_OUTOFCONTEXT);
}


BOOL InitInstance(HINSTANCE hInstance)
{
	InstanceData::Instance.AppHandle = hInstance;

	const auto windowHandle = CreateWindowW(
		_T("MONITORKEEPER"),
		_T("MonitorKeeper"),
		WS_OVERLAPPEDWINDOW & ~WS_VISIBLE,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		nullptr,
		nullptr,
		InstanceData::Instance.AppHandle,
		nullptr);

	ProcessDesktopWindows();
	InstanceData::Instance.NumMonitors = GetSystemMetrics(SM_CMONITORS);

	if (!windowHandle)
		return FALSE;

	InstanceData::Instance.MainWindowHandle = windowHandle;
	InstanceData::Instance.WindowsEventHandle = RegisterWindowsEventCallback();

	SetScrollRange(windowHandle, SB_VERT, 0, 10000, false);

	NOTIFYICONDATA icon;
	icon.cbSize = sizeof(icon);
	icon.hWnd = windowHandle;
	icon.uID = 1;
	icon.szTip[0] = '\0';
	icon.uFlags = NIF_ICON|NIF_MESSAGE| NIM_SETVERSION;
	icon.uCallbackMessage = WM_USER + 100;
	icon.hIcon = LoadIcon(InstanceData::Instance.AppHandle, MAKEINTRESOURCE(IDI_MONITORKEEPER));
	icon.uVersion = NOTIFYICON_VERSION_4;
	Shell_NotifyIcon(NIM_ADD, &icon);

	return TRUE;
}


void OnDisplayChange(bool force)
{
	if (InstanceData::Instance.EnableLogging)
		InstanceData::Instance.AddLogMessage(_T("WM_DISPLAYCHANGE\n"));

	InstanceData::Instance.ProcessingDisplayChange = true;
	InstanceData::Instance.IsForcedDisplayChange = force;
	SetTimer(InstanceData::Instance.MainWindowHandle, 99, 500, ProcessMonitorsTimerCallback);
}


bool ProcessCommand(HWND windowHandle, WPARAM wParam)
{
	auto processed = true;

	switch (LOWORD(wParam))
	{
		case IDM_ABOUT:
			DialogBox(InstanceData::Instance.AppHandle, MAKEINTRESOURCE(IDD_ABOUTBOX), windowHandle, AboutDialogMessageHandler);
			break;

		case IDM_EXIT:
			DestroyWindow(windowHandle);
			break;

		case IDM_SHOWWINDOW:
			ShowWindow(windowHandle, SW_RESTORE);
			UpdateWindow(windowHandle);
			break;

		case IDM_FORCERESTORE:
			OnDisplayChange(true);
			break;

		case IDM_TOGGLELOGGING:
			InstanceData::Instance.ToggleLogging();
			break;

		default:
			processed = false;
			break;
	}

	return processed;
}


void ShowContextMenu(HWND windowHandle, WPARAM wParam)
{
	int x = LOWORD(wParam);
	int y = HIWORD(wParam);

	// Offset by icon dimensions (?)
	RECT r;
	NOTIFYICONIDENTIFIER id;
	id.cbSize = sizeof(id);
	id.hWnd = windowHandle;
	id.uID = 1;
	id.guidItem = GUID_NULL;
	Shell_NotifyIconGetRect(&id, &r);
	x += r.left;
	y += r.top;

	auto menuHandle = GetMenu(windowHandle);
	menuHandle = GetSubMenu(menuHandle, 1);
	TrackPopupMenu(menuHandle, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, x, y, 0, windowHandle, NULL);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
		case WM_DISPLAYCHANGE:
			OnDisplayChange(false);
			break;

    	case WM_COMMAND:
			if (!ProcessCommand(hWnd, wParam))
				return DefWindowProc(hWnd, message, wParam, lParam);
	        break;

		case WM_CLOSE:
			ShowWindow(hWnd, SW_HIDE);
			return false;

		case (WM_USER + 100): // notify icon
			if (LOWORD(lParam) == WM_CONTEXTMENU || LOWORD(lParam) == WM_RBUTTONUP)
				ShowContextMenu(hWnd, wParam);
			break;

		case WM_VSCROLL:
		{
			int pos = GetScrollPos(hWnd, SB_VERT);
			switch (LOWORD(wParam))
			{
				case SB_BOTTOM:
					pos = 10000;
					break;
				case SB_TOP:
					pos += 0;
					break;
				case SB_PAGEDOWN:
					pos += 1000;
					break;
				case SB_PAGEUP:
					pos -= 1000;
					break;
				case SB_THUMBPOSITION:
					pos = HIWORD(wParam);
					break;
				case SB_THUMBTRACK:
					pos = HIWORD(wParam);
					break;
			}

			if (pos < 0)
				pos = 0;
			if (pos > 10000)
				pos = 10000;

			SetScrollPos(hWnd, SB_VERT, pos, true);
			InvalidateRect(hWnd, NULL, true);
			break;
		}

		case WM_PAINT:
		{
			if (*InstanceData::Instance.LogBuffer)
			{
				PAINTSTRUCT ps;
				RECT r,r2;
				HDC hdc = BeginPaint(hWnd, &ps);
				GetClientRect(hWnd, &r);
				r2 = r;
				DrawText(hdc, InstanceData::Instance.LogBuffer,
					-1, &r2, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT);

				int pos = GetScrollPos(hWnd, SB_VERT);
				pos = pos * (r2.bottom - r.bottom)/ 10000;

				r.top = r.top - pos;
				DrawText(hdc, InstanceData::Instance.LogBuffer,
					-1, &r, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);

				EndPaint(hWnd, &ps);
			}
			break;
		}

		case WM_DESTROY:
		{
			NOTIFYICONDATA icon;
			icon.cbSize = sizeof(icon);
			icon.hWnd = hWnd;
			icon.uID = 1;
			Shell_NotifyIcon(NIM_DELETE, &icon);
			PostQuitMessage(0);
	        break;
		}

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}


INT_PTR CALLBACK AboutDialogMessageHandler(HWND dialogHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
		case WM_INITDIALOG:
			return (INT_PTR)TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(dialogHandle, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			break;
    }
    return (INT_PTR)FALSE;
}
