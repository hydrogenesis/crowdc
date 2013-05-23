#pragma once
#include <windows.h>
#include <atomic>

#define MAX_THREAD_CNT 256

typedef struct {
	DWORD process_id;				// process id
	std::atomic_int percentage;		// throttle percentage, ranging in [0, 100]
	std::atomic_int throttle_time;	// in milliseconds, in most cases 100
	std::atomic_int revisit_time;	// in milliseconds, time for revisiting all the subthreads, eg.: 10000 for 10 seconds
	std::atomic_bool should_stop;	// specifies whether the throttling should stop
} ThrottleInfo, *LPThrottleInfo;

// returns throttle thread handle
HANDLE create_throttle_thread(LPThrottleInfo throttle_info);
unsigned __stdcall throttle_process(LPVOID lpVoid);
static int throttle_process_worker(LPVOID lpVoid);
int ListProcessThreads( DWORD dwOwnerPID, DWORD * dwThreadIdTable );

HANDLE
CreateThread1(
    __in_opt  LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in      SIZE_T dwStackSize,
    __in      unsigned (__stdcall * lpStartAddress)(void *),
    __in_opt  LPVOID lpParameter,
    __in      DWORD dwCreationFlags,
    __out_opt LPDWORD lpThreadId
    );