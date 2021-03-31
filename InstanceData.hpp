#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <mutex>
#include <unordered_map>

class SavedWindowData;

class InstanceData
{
private:
	static const int LOG_BUFFER_SIZE = 32 * 1024;

public:
	static InstanceData Instance;

	void AddLogMessage(LPCTSTR message);
	void ToggleLogging();
	void TagWindowsUnused();
	void RestoreWindowPositions(int monitors);
	void SaveWindow(HWND hwnd, int numMonitors);

	HINSTANCE        AppHandle;
	HWINEVENTHOOK	 WindowsEventHandle;
	int				 NumMonitors;
	HWND			 MainWindowHandle;
	std::atomic_bool ProcessingDisplayChange;
	bool             IsForcedDisplayChange;
	bool             EnableLogging;
	TCHAR			 LogBuffer[LOG_BUFFER_SIZE];

private:
	InstanceData();
	~InstanceData();

	std::unordered_map<HWND, SavedWindowData> _windowData;
	std::mutex _windowDataMutex;
	std::mutex _logMutex;
};
