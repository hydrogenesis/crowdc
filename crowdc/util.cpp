#include "util.h"
#include <tlhelp32.h>
#include <locale.h>

int ListProcessThreads( DWORD dwOwnerPID, DWORD * dwThreadIdTable ) 
{
	int iThreadCount = 0; 
	HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0UL );
	if( hThreadSnap == INVALID_HANDLE_VALUE ) return 0;

	THREADENTRY32 te32;
	te32.dwSize = sizeof( THREADENTRY32 ); 

	if( ! Thread32First( hThreadSnap, &te32 ) )
	{
		CloseHandle( hThreadSnap );
		return 0;
	}

	do 
	{ 
		if( te32.th32OwnerProcessID == dwOwnerPID )
		{
			dwThreadIdTable[ iThreadCount++ ] = te32.th32ThreadID;
		}
	} while( Thread32Next( hThreadSnap, &te32 ) && iThreadCount < MAX_THREAD_CNT ); 

	CloseHandle( hThreadSnap );
	return iThreadCount;
}

// throttle process by certain percentage
static int throttle_process_worker(LPVOID lpVoid) {
	ThrottleInfo& ti = *(LPThrottleInfo) lpVoid;
	DWORD thread_id[MAX_THREAD_CNT];
	HANDLE thread_handle[MAX_THREAD_CNT];
	while (!ti.should_stop) {
		// Refresh threads
		int num_threads = ListProcessThreads(ti.process_id, thread_id);
		if (num_threads <= 0) {
			Sleep(1000UL);	// sleep for 1 second
			continue;
		}
		bool has_failure = false;
		for (int i = 0; i < num_threads; ++i) {
			thread_handle[i] = OpenThread(THREAD_SUSPEND_RESUME, FALSE, thread_id[i]);
			if (thread_handle[i] == NULL) {
				has_failure = true;
				break;
			}
		}
		if (has_failure) {
			Sleep(1000UL);	// sleep for 1 second
			continue;
		}

		DWORD time_elapsed = 0;
		DWORD time_throttle = ti.throttle_time * ti.percentage / 100;
		DWORD time_normal = ti.throttle_time * (100 - ti.percentage) / 100;;
		while (time_elapsed < ti.revisit_time && !ti.should_stop) {
			// Suspend all threads regardlessly
			for (int i = 0; i < num_threads; ++i) {
				SuspendThread(thread_handle[i]);
			}
			Sleep(time_throttle);
			time_elapsed += time_throttle;

			// Resume all threads recordingly
			for (int i = 0; i < num_threads; ++i) {
				ResumeThread(thread_handle[i]);
			}
			Sleep(time_normal);
			time_elapsed += time_normal;
		}

		// Cleanup threads for the next round
		for (int i = 0; i < num_threads; ++i) {
			if (thread_handle[i]) {
				CloseHandle(thread_handle[i]);
			}
		}
	}
}

unsigned __stdcall throttle_process(LPVOID lpVoid) {
	setlocale(LC_ALL, "English_us.1252"); // _tcsicmp doesn't work properly in the "C" locale
	timeBeginPeriod(1);
	int ret = throttle_process_worker(lpVoid);
	timeEndPeriod(1);
	return (unsigned) ret;
}