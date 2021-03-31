#define WIN32_LEAN_AND_MEAN
#include <windows.h>


class SavedWindowData
{
public:
	SavedWindowData();

	void Save(HWND hwnd, int numMonitors);
	void Restore(int numMonitors);
	bool IsStillUsed() const;
	bool CheckUnusedCount();

private:
	static const int MAX_MONITORS = 5;
	static const int MIN_MONITORS = 2;

	void ReportAction(const TCHAR* action, const WINDOWPLACEMENT& placement, int numMonitors);

	int				_unusedCount;
	WINDOWPLACEMENT	_windowPlacement[MAX_MONITORS - MIN_MONITORS + 1];
	HWND			_hwnd;
	TCHAR			_wndClass[64];  // window class, for verification.
};
