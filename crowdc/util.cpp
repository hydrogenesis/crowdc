#include "util.h"
#include <tlhelp32.h>
#include <locale.h>
#include <fstream>
#include <process.h>
#include "md5.h"

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Pdh.lib")

// throttle process by certain percentage
static int throttle_process_worker(LPVOID lpVoid) {
	ThrottleInfo& ti = *(LPThrottleInfo) lpVoid;
	DWORD thread_id[MAX_THREAD_CNT];
	HANDLE thread_handle[MAX_THREAD_CNT];
	PDH_HQUERY cpu_query;
	PDH_HCOUNTER cpu_total;
	init_cpu_usage(&cpu_query, &cpu_total);
	int percentage = ti.percentage;

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
		
		double current_usage = get_cpu_usage(&cpu_query, &cpu_total);
		if (current_usage > 100.0) current_usage = 100.0;
		if (current_usage < 0.0) current_usage = 0.0;

		int delta = (int)current_usage - (100 - ti.percentage);
		percentage += delta;
		// set throttle percentage between [ti.percentage, 100]
		if (percentage > 100) percentage = 100;
		if (percentage < ti.percentage) percentage = ti.percentage;
		
		DWORD time_elapsed = 0;
		DWORD time_throttle = ti.throttle_time * percentage / 100;
		DWORD time_normal = ti.throttle_time * (100 - percentage) / 100;
		while (time_elapsed < (DWORD) ti.revisit_time && !ti.should_stop) {
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
	return 0;
}

unsigned __stdcall throttle_process(LPVOID lpVoid) {
	setlocale(LC_ALL, "English_us.1252"); // _tcsicmp doesn't work properly in the "C" locale
	timeBeginPeriod(1);
	int ret = throttle_process_worker(lpVoid);
	timeEndPeriod(1);
	return (unsigned) ret;
}

HANDLE create_throttle_thread(LPThrottleInfo throttle_info) {
	DWORD child_thread_id;
	HANDLE thread;
	throttle_info->should_stop.store(false);
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof( SECURITY_ATTRIBUTES );
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = FALSE;

	thread = 
		CreateThread1(
		&sa,
		(DWORD) 0,
		&throttle_process,
		(LPVOID) (LPThrottleInfo) throttle_info,
		(DWORD) 0,
		&child_thread_id
		);
	if( thread != NULL ) {
		SetThreadPriority( thread, THREAD_PRIORITY_TIME_CRITICAL );
	}
	return thread;
}

HANDLE
CreateThread1(
    __in_opt  LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in      SIZE_T dwStackSize,
    __in      unsigned (__stdcall * lpStartAddress)(void *),
    __in_opt  LPVOID lpParameter,
    __in      DWORD dwCreationFlags,
    __out_opt LPDWORD lpThreadId
    )
{
	HANDLE hThread = (HANDLE)
		(ULONG_PTR) // On VC6, _beginthreadex returns unsigned long (which is bad for Win64)
		_beginthreadex( 
			lpThreadAttributes,
			(unsigned int) dwStackSize,
			lpStartAddress,
			lpParameter,
			dwCreationFlags,
			(unsigned int*) lpThreadId
		);
	
	if( ! hThread && ! dwStackSize )
	{
		hThread = (HANDLE)(ULONG_PTR) _beginthreadex( 
									lpThreadAttributes,
									127 * 1024,
									lpStartAddress,
									lpParameter,
									0x0,
									NULL
								);
	}

	return hThread;
}

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

unsigned int split(const std::string &txt, std::vector<std::string> *strs, char ch)
{
    unsigned int pos = txt.find( ch );
    unsigned int initialPos = 0;
    strs->clear();

    // Decompose statement
    while( pos != std::string::npos ) {
        strs->push_back( txt.substr( initialPos, pos - initialPos ) );
        initialPos = pos + 1;

        pos = txt.find( ch, initialPos );
    }

    // Add the last one
    strs->push_back( txt.substr( initialPos, min( pos, txt.size() ) - initialPos ) );

    return strs->size();
}

void join(const std::vector<std::string>& strs, std::string* out, const std::string& delim, int begin, int end) {
	if (end < begin) return;
	if (strs.size() < (std::size_t) end) return;
	out->clear();
	for (int i = begin; i < end; ++i) {
		out->append(strs[i]);
		if (i < end - 1) out->append(delim);
	}
}

void init_cpu_usage(PDH_HQUERY* cpu_query, PDH_HCOUNTER* cpu_total) {
	PdhOpenQuery(NULL, NULL, cpu_query);
    PdhAddCounter(*cpu_query, L"\\Processor(_Total)\\% Processor Time", NULL, cpu_total);
    PdhCollectQueryData(*cpu_query);
}

double get_cpu_usage(PDH_HQUERY* cpu_query, PDH_HCOUNTER* cpu_total) {
	PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(*cpu_query);
    PdhGetFormattedCounterValue(*cpu_total, PDH_FMT_DOUBLE, NULL, &counterVal);
    return counterVal.doubleValue;
}

boolean calc_file_md5_string(const std::string& file, std::string* md5string) {
	std::ifstream ifs(file);
	if (!ifs.good()) return false;
	ifs.close();

	MD5 md5;
	md5string->assign(md5.digestFile(file.c_str()));
	return true;
}

boolean create_uuid_string(std::string* uuid) {
	UUID id;
	UuidCreate( &id );
	char* buffer;
    if (UuidToStringA(&id, (RPC_CSTR*)&buffer) == RPC_S_OK)
    {
        uuid->assign(buffer);
        RpcStringFreeA((RPC_CSTR*)&buffer);
		return true;
    }
	return false;
}