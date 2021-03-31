#include "SavedWindowData.hpp"

#include <tchar.h>

#include "InstanceData.hpp"


SavedWindowData::SavedWindowData()
{
    _unusedCount = 0;
    _hwnd = NULL;
    _wndClass[0] = '\0';
}

void SavedWindowData::Save(HWND hwnd, int numMonitors)
{
    _hwnd = hwnd;
    _unusedCount = 0;
    RealGetWindowClass(_hwnd, _wndClass, sizeof(_wndClass) / sizeof(TCHAR));

    if (numMonitors < MIN_MONITORS || numMonitors > MAX_MONITORS)
        return;

    auto& placement = _windowPlacement[numMonitors - MIN_MONITORS];
    placement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(_hwnd, &placement);

    #ifdef _DEBUG
    ReportAction(_T("Saved"), placement, numMonitors);
    #endif
}

void SavedWindowData::Restore(int numMonitors)
{
    if (numMonitors < MIN_MONITORS || numMonitors > MAX_MONITORS)
        return;

    auto& placement = _windowPlacement[numMonitors - MIN_MONITORS];
    if (!IsWindow(_hwnd) || placement.length != sizeof(WINDOWPLACEMENT))
        return;

    TCHAR szTempClass[64];
    RealGetWindowClass(_hwnd, szTempClass, sizeof(szTempClass) / sizeof(TCHAR));
    if (lstrcmp(szTempClass, _wndClass) != 0)
        return;

    // don't worry about "minimized position", it is a concept from Windows 3.0.
    placement.flags = WPF_ASYNCWINDOWPLACEMENT;

    if (placement.showCmd == SW_MAXIMIZE)
    {
        // we need to treat this special, first restore it to the correct position,
        // then maximize. Otherwise, it will just maximize it on the current screen
        // and ingore the coordinates.
        placement.showCmd = SW_SHOWNOACTIVATE;
        SetWindowPlacement(_hwnd, &placement);
        placement.showCmd = SW_MAXIMIZE;
    }
    else if (placement.showCmd == SW_MINIMIZE || placement.showCmd == SW_SHOWMINIMIZED)
    {
        placement.showCmd = SW_SHOWMINNOACTIVE;
    }
    else if (placement.showCmd == SW_NORMAL)
    {
        placement.showCmd = SW_SHOWNOACTIVATE;
    }
    SetWindowPlacement(_hwnd, &placement);

    ReportAction(_T("Restored"), placement, numMonitors);
}

bool SavedWindowData::IsStillUsed() const
{
    return _unusedCount <= 2;
}

bool SavedWindowData::CheckUnusedCount()
{
    return ++_unusedCount >= 100;
}

void SavedWindowData::ReportAction(const TCHAR* action, const WINDOWPLACEMENT& placement, int numMonitors)
{
    if (!InstanceData::Instance.EnableLogging)
        return;

    const auto* showCmd = _T("Unknown");
    switch (placement.showCmd)
    {
    case SW_RESTORE:
    case SW_SHOWNORMAL:
        showCmd = _T("SW_SHOWNORMAL"); break;
    case SW_MAXIMIZE:
        showCmd = _T("SW_MAXIMIZE"); break;
    case SW_MINIMIZE:
    case SW_SHOWMINIMIZED:
        showCmd = _T("SW_MINIMIZE"); break;
    case SW_SHOWNOACTIVATE:
        showCmd = _T("SW_SHOWNOACTIVATE"); break;
    case SW_SHOWMINNOACTIVE:
        showCmd = _T("SW_SHOWMINNOACTIVE"); break;
    }

    const auto x = placement.rcNormalPosition.left;
    const auto y = placement.rcNormalPosition.top;

    TCHAR message[256];
    wsprintf(message, _T("%s 0x%X %s: monitors=%d; x=%d; y=%d; show=%s\n"), action, _hwnd, _wndClass, numMonitors, x, y, showCmd);
    InstanceData::Instance.AddLogMessage(message);
}
