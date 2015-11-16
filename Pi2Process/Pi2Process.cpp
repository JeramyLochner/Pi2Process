// Pi2Process.cpp : Defines the entry point for the console application.
//

#include "pch.h"

#include <windows.h>
#include <chrono>
#include <thread>
#include <ctime>
#include <iostream>

#include <algorithm>

#include <stdio.h>
#include <tchar.h>
#include <Psapi.h>


using namespace std;

#define DIV 1024

#define MESSAGE_WIDTH 30
#define NUMERIC_WIDTH 10

/* General Functions */
void printMessage(LPCSTR msg, bool addColon) {
	cout.width(MESSAGE_WIDTH);
	cout << msg;
	if (addColon) {
		cout << " : ";
	}
}

void printMessageLine(LPCSTR msg) {
	printMessage(msg, false);
	cout << endl;
}

void printMessageLine(LPCSTR msg, DWORD value) {
	printMessage(msg, true);
	cout.width(NUMERIC_WIDTH);
	cout << right << value << endl;
}

void printMessageLine(LPCSTR msg, DWORDLONG value) {
	printMessage(msg, true);
	cout.width(NUMERIC_WIDTH);
	cout << right << value << endl;
}

char* getCmdOption(char ** begin, char ** end, const std::string & option) {
	char ** itr = std::find(begin, end, option);
	if (itr != end && ++itr != end) {
		return *itr;
	}
	return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option) {
	return std::find(begin, end, option) != end;
}

void printGeneralInformation() {
	TCHAR szComputerNameEx[MAX_COMPUTERNAME_LENGTH + 1] = TEXT("<unknown>");
	DWORD size = sizeof(szComputerNameEx) / sizeof(szComputerNameEx[0]);

	time_t t;
	struct tm now;
	t = time(0);
	localtime_s(&now, &t);
	
	GetComputerNameEx((COMPUTER_NAME_FORMAT)0, szComputerNameEx, &size);

	cout << "Computer Name: " << szComputerNameEx << endl;
	cout << "Time (h:m:s): " << (now.tm_hour) << ':' << (now.tm_min) << ':' << (now.tm_sec) << endl;

}
/* End of General Functions */

/* Memory Status Functions */
void checkInput(HANDLE exitEvent) {
	for (;;) {
		char character;
		cin.get(character);
		if (character == 'q') {
			::SetEvent(exitEvent);
			break;
		}
	}
}

int memoryStatus(int seconds) { // -m Memory monitoring
	time_t t;
	struct tm now;
	t = time(0);
	localtime_s(&now, &t);

	printMessageLine("Starting to monitor memory consumption!");
	printMessageLine("Use Ctrl+C to kill pipeline");

	//TODO: Redo this process so it doesn't need an event/new thread
	HANDLE exitEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == exitEvent) {
		printMessageLine("Failed to create exitEvent.");
		return -1;
	}
	std::thread inputThread(checkInput, exitEvent);
	std::chrono::seconds interval(seconds);
	for (;;) {
		MEMORYSTATUSEX statex;
		statex.dwLength = sizeof(statex);

		BOOL success = ::GlobalMemoryStatusEx(&statex);
		if (!success) {
			DWORD error = GetLastError();
			printMessageLine("******************************");
			printMessageLine("Error getting memory information", error);
			printMessageLine("******************************");
		}
		else {
			t = time(0);
			localtime_s(&now, &t);
			DWORD load = statex.dwMemoryLoad;
			DWORDLONG physKb = statex.ullTotalPhys / DIV;
			DWORDLONG freePhysKb = statex.ullAvailPhys / DIV;
			DWORDLONG pageKb = statex.ullTotalPageFile / DIV;
			DWORDLONG freePageKb = statex.ullAvailPageFile / DIV;
			DWORDLONG virtualKb = statex.ullTotalVirtual / DIV;
			DWORDLONG freeVirtualKb = statex.ullAvailVirtual / DIV;
			DWORDLONG freeExtKb = statex.ullAvailExtendedVirtual / DIV;

			printMessageLine("******************************");
			cout << (now.tm_hour) << ':' << (now.tm_min) << ':' << (now.tm_sec) << endl;

			printMessageLine("Percent of memory in use", load);
			printMessageLine("KB of physical memory", physKb);
			printMessageLine("KB of free physical memory", freePhysKb);
			printMessageLine("KB of paging file", pageKb);
			printMessageLine("KB of free paging file", freePageKb);
			printMessageLine("KB of virtual memory", virtualKb);
			printMessageLine("KB of free virtual memory", freeVirtualKb);
			printMessageLine("KB of free extended memory", freeExtKb);

			printMessageLine("******************************");
		}

		if (WAIT_OBJECT_0 == ::WaitForSingleObject(exitEvent, 100)) {
			break;
		}

		std::this_thread::sleep_for(interval);
	}

	inputThread.join();
	::CloseHandle(exitEvent);
	printMessageLine("No longer monitoring memory consumption!");
	return 0;
}
/* End of Memory Status Functions */

/* Process Functions */

void printProcessInfo(DWORD processID) {
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
	TCHAR szExecutableName[MAX_PATH] = TEXT("<unknown>");

	// Get handle to the process
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

	//Get process name
	if (NULL != hProcess) {
		HMODULE hMod;
		DWORD cbNeeded;
		if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
			GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
		}
		GetModuleFileNameEx(hProcess, 0, szExecutableName, MAX_PATH);
	}

	//Print the process name and identifier
	_tprintf(TEXT("%s (PID: %u) - %s\n"), szProcessName, processID, szExecutableName);

	//Release the handle to the process
	CloseHandle(hProcess);
}

int listProcesses() {		// -l - List

							//Get list of PIDs
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i;

	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
		return -1;
	}

	// # of processes
	cProcesses = cbNeeded / sizeof(DWORD);

	//Print the name+PID for each process
	for (i = 0; i < cProcesses; i++) {
		if (aProcesses[i] != 0) {
			printProcessInfo(aProcesses[i]);
		}
	}

	return 0;
}

int processMemoryInfo(DWORD processID) {
	HANDLE hProcess;
	PROCESS_MEMORY_COUNTERS pmc;

	printf("\nProcess ID: %u\n", processID);

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
	if (NULL == hProcess)
		return -1;
	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
		printf("\tPageFaultCount: 0x%08X\n", pmc.PageFaultCount);
		printf("\tPeakWorkingSetSize: 0x%08X\n", pmc.PeakWorkingSetSize);
		printf("\tWorkingSetSize: 0x%08X\n", pmc.WorkingSetSize);
		printf("\tQuotaPeakPagedPoolUsage: 0x%08X\n", pmc.QuotaPeakPagedPoolUsage);
		printf("\tQuotaPagedPoolUsage: 0x%08X\n", pmc.QuotaPagedPoolUsage);
		printf("\tQuotaPeakNonPagedPoolUsage: 0x%08X\n", pmc.QuotaPeakPagedPoolUsage);
		printf("\tQuotaNonPagedPoolUsage: 0x%08X\n", pmc.QuotaNonPagedPoolUsage);
		printf("\tPagefileUsage: 0x%08X\n", pmc.PagefileUsage);
		printf("\tPeakPagefileUsage: 0x%08X\n", pmc.PeakPagefileUsage);
	}

	CloseHandle(hProcess);

	return 0;
}
/* End of Processes Functions */

int main(int argc, char * argv[]) {

	if (cmdOptionExists(argv, argv + argc, "-h")) {
		cout << "**********" << endl;
		cout << "-h\t-\tHelp" << endl;
		cout << "-l\t-\tList Processes" << endl;
		cout << "-u <pid>\t-\tProcess Memory Usage" << endl;
		cout << "-m <time (s)>\tMonitor memory" << endl;
		cout << "**********" << endl;
		return 0;
	}
	else {
		printGeneralInformation();
	}
	if (cmdOptionExists(argv, argv + argc, "-m")) {
		char * seconds = getCmdOption(argv, argv + argc, "-m");
		return memoryStatus(strtol(seconds, NULL, 0));
	}
	else if (cmdOptionExists(argv, argv + argc, "-l")) {
		return listProcesses();
	}
	else if (cmdOptionExists(argv, +argv + argc, "-u")) {
		char * pid = getCmdOption(argv, argv + argc, "-u");
		return processMemoryInfo(atoi(pid));
	}
	else {
		printMessageLine("./MemoryStatus.exe -h for help");
	}
	return 0;
}