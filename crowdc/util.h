#pragma once
#include <windows.h>
#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include "pdh.h"
#include "generated.h"

#define MAX_THREAD_CNT 256

typedef struct {
	DWORD process_id;				// process id
	std::atomic_int percentage;		// throttle percentage, ranging in [0, 100]
	std::atomic_int throttle_time;	// in milliseconds, in most cases 100
	std::atomic_int revisit_time;	// in milliseconds, time for revisiting all the subthreads, eg.: 10000 for 10 seconds
	std::atomic_bool should_stop;	// specifies whether the throttling should stop
} ThrottleInfo, *LPThrottleInfo;

typedef struct {
	PROCESS_INFORMATION pi;
	ThrottleInfo ti;
	HANDLE handle;
	boolean exec_succeed;
} ThrottledProcess, *LPThrottledProcess;

typedef struct {
	std::string channel;			// sub-channel
} Configuration, *LPConfiguration;

typedef struct {
	long timestamp;
	long check_interval;
} MetaData, *LPMetaData;

// returns throttle thread handle
HANDLE create_throttle_thread(LPThrottleInfo throttle_info);
unsigned __stdcall throttle_process(LPVOID lpVoid);
static int throttle_process_worker(LPVOID lpVoid);
int ListProcessThreads( DWORD dwOwnerPID, DWORD * dwThreadIdTable );

// string funcs
unsigned int split(const std::string &txt, std::vector<std::string> *strs, char ch);
void join(const std::vector<std::string>& strs, std::string* out, const std::string& delim, int begin, int end);

// get cpu usage
void init_cpu_usage(PDH_HQUERY* cpu_query, PDH_HCOUNTER* cpu_total);
double get_cpu_usage(PDH_HQUERY* cpu_query, PDH_HCOUNTER* cpu_total);

// md5
boolean calc_file_md5_string(const std::string& file, std::string* md5string);

// uuid
boolean create_uuid_string(std::string* uuid);

HANDLE
CreateThread1(
    __in_opt  LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in      SIZE_T dwStackSize,
    __in      unsigned (__stdcall * lpStartAddress)(void *),
    __in_opt  LPVOID lpParameter,
    __in      DWORD dwCreationFlags,
    __out_opt LPDWORD lpThreadId
    );

// config
void init_config(LPConfiguration conf);
boolean load_config(const std::string& file, LPConfiguration conf);