#include "InstanceData.hpp"

#undef min
#undef max
#include <algorithm>
#include <memory>
#include <tchar.h>

#include "SavedWindowData.hpp"

/*static*/ InstanceData InstanceData::Instance;

InstanceData::InstanceData()
{
    AppHandle = NULL;
    WindowsEventHandle = NULL;
    NumMonitors = 1;
    MainWindowHandle = NULL;
    ProcessingDisplayChange = false;
    IsForcedDisplayChange = false;
    EnableLogging = false;
    LogBuffer[0] = '\0';
}

InstanceData::~InstanceData()
{
    if (WindowsEventHandle)
        UnhookWinEvent(WindowsEventHandle);
    WindowsEventHandle = NULL;
}

void InstanceData::AddLogMessage(LPCTSTR message)
{
    auto lock = std::scoped_lock(_logMutex);

    int currentLen = lstrlen(LogBuffer);
    int extra = lstrlen(message);
    if (currentLen + extra + 1 >= LOG_BUFFER_SIZE)
    {
        auto reduceBy = std::min(std::max(currentLen + extra + 1 - LOG_BUFFER_SIZE, 1024), currentLen + 1);
        std::memmove(LogBuffer, LogBuffer + reduceBy, currentLen + 1 - reduceBy);
        currentLen -= reduceBy;
        LogBuffer[0] = LogBuffer[1] = LogBuffer[2] = _T('.');
        LogBuffer[3] = _T('\n');
    }

    lstrcpy(LogBuffer + currentLen, message);

    if (MainWindowHandle != NULL)
    {
        SetScrollPos(MainWindowHandle, SB_VERT, 10000, true);
        InvalidateRect(MainWindowHandle, NULL, TRUE);
    }
}

void InstanceData::ToggleLogging()
{
    EnableLogging = !EnableLogging;
    AddLogMessage(EnableLogging ? _T("Logging enabled\n") : _T("Logging disabled\n"));
}

// We are not notified on a window destroy, so we mark windows if we haven't seen them.
// If we don't see one multiple times in a row, drop it.
void InstanceData::TagWindowsUnused()
{
    auto lock = std::scoped_lock(_windowDataMutex);
    for (auto it = _windowData.begin(); it != _windowData.end();)
    {
        if (it->second.CheckUnusedCount())
            it = _windowData.erase(it);
        else
            ++it;
    }
}

void InstanceData::RestoreWindowPositions(int monitors)
{
    auto lock = std::scoped_lock(_windowDataMutex);
    for (auto& [_, data] : _windowData)
        if (data.IsStillUsed())
            data.Restore(monitors);
}

void InstanceData::SaveWindow(HWND hwnd, int numMonitors)
{
    auto lock = std::scoped_lock(_windowDataMutex);
    _windowData[hwnd].Save(hwnd, numMonitors);
}
