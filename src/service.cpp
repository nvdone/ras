//NVD RunAsService
//Copyright © 2022, Nikolay Dudkin

//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//GNU General Public License for more details.
//You should have received a copy of the GNU General Public License
//along with this program.If not, see<https://www.gnu.org/licenses/>.

#include <windows.h>
#include <shlobj.h>
#include <wchar.h>

#include "..\\lib\\cmdline.hpp"

#ifdef WATCOM
	#define SWSCANF swscanf
#else
	#define SWSCANF swscanf_s
#endif

#define INTERNALNAME L"NVD RunAsService"
#define ONEMINUTE 60000

HANDLE stopEventHandle = NULL;
SERVICE_STATUS_HANDLE myServiceStatusHandle;
SERVICE_STATUS myServiceStatus;

wchar_t *serviceName = NULL;
int loopMinutes = 0;
int delayMinutes = 0;

PROCESS_INFORMATION payloadPI;
wchar_t *payloadPath = NULL;
wchar_t *payloadDir = NULL;

void WINAPI ServiceMain(DWORD, LPTSTR*);
void WINAPI ServiceControlHandler(DWORD dwCtrl);
BOOL ReportStatusToSCManager(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

int ServiceInstall(DWORD dwStartType, wchar_t *account, wchar_t *password, wchar_t *directory);
int ServiceUninstall();
int ServiceStart();
int ServiceStop();

int IsPayloadRunning();
void StartPayloadProcess();
void ServiceBody();
int WrapError(int error, const wchar_t* message, int silent);

#ifdef WATCOM
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPTSTR cmdline, int)
#else
int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPTSTR cmdline, _In_ int)
#endif
{
	int retval = 0;

	CmdLine *cl = new CmdLine(cmdline);

	int silent = cl->HasParam(L"-silent", 1) | cl->HasParam(L"-run", 1); //No messages if this is actually a service, or if requested by the user

	if (!cl->HasParam(L"-install", 1) || cl->HasParam(L"-path", 1)) //Cannot install unless actual executable payload path specified
	{
		serviceName = cl->GetOptionValue(L"-servicename", 1, 0); //Cannot do anything unless service name is specified
	}

	if (cl->HasParam(L"-?", 1) || !serviceName)
	{
		if (!silent)
		{
			wchar_t myExePath[MAX_PATH];
			wchar_t message[MAX_PATH + 1024];

			memset(myExePath, 0, sizeof(wchar_t) * MAX_PATH);
			GetModuleFileName(NULL, myExePath, MAX_PATH - 1);

			memset(message, 0, sizeof(wchar_t) * MAX_PATH + 1024);
			swprintf(message, MAX_PATH + 1023, L"%s\r\n(C) 2022, Nikolay Dudkin\r\n\r\nUsage:\r\n%s\r\n-servicename:\"service name\"\r\n[-install\r\n\t-path:\"path to executable\"\r\n\t[-setdirectory:\"path to working directory\"]\r\n\t[-starttype:<auto|demand|disabled>]\r\n\t[-account:\"account name\"\r\n\t-password:\"account password\"]\r\n\t[-loop:M]\r\n\t[-delay:M]]\r\n[-uninstall]\r\n[-start]\r\n[-stop]\r\n[-silent]", INTERNALNAME, myExePath);

			MessageBox(NULL, message, INTERNALNAME, MB_OK | MB_ICONINFORMATION);
		}

		retval |= serviceName ? 0 : 1;
		delete cl;
		return retval;
	}

	if (cl->HasParam(L"-install", 1) || cl->HasParam(L"-uninstall", 1) || cl->HasParam(L"-start", 1) || cl->HasParam(L"-stop", 1))
	{
		if (!IsUserAnAdmin())
		{
			wchar_t myExePath[MAX_PATH];
			memset(myExePath, 0, sizeof(wchar_t) * MAX_PATH);

			if (GetModuleFileName(NULL, myExePath, MAX_PATH))
			{
				SHELLEXECUTEINFO sei;
				memset(&sei, 0, sizeof(SHELLEXECUTEINFO));
				sei.cbSize = sizeof(SHELLEXECUTEINFO);
				sei.lpVerb = L"runas";
				sei.lpFile = myExePath;
				sei.lpParameters = cmdline;
				ShellExecuteEx(&sei);

				delete cl;
				return 0;
			}
			else
			{
				delete cl;
				return WrapError(2, L"Failed to restart elevated, error: %d!", silent);
			}
		}
	}

	wchar_t* p = cl->GetOptionValue(L"-loop", 1, 1);
	if (!p || SWSCANF(p, L"%d", &loopMinutes) != 1)
		loopMinutes = 0;

	p = cl->GetOptionValue(L"-delay", 1, 1);
	if (!p || SWSCANF(p, L"%d", &delayMinutes) != 1)
		delayMinutes = 0;

	//Order matters. This way one can reinstall and restart service with a single command

	if (cl->HasParam(L"-stop", 1))
		retval |= WrapError(ServiceStop(), L"Failed to stop service, error: %d!", silent);

	if (cl->HasParam(L"-uninstall", 1))
		retval |= WrapError(ServiceUninstall(), L"Failed to uninstall service, error: %d!", silent);

	if (cl->HasParam(L"-install", 1))
	{
		payloadPath = cl->GetOptionValue(L"-path", 1, 0);
		if (payloadPath)
		{
			DWORD startType = SERVICE_AUTO_START;
			if (cl->HasParam(L"-starttype", 1))
			{
				if (!wcscmp(cl->GetOptionValue(L"-starttype", 1, 1), L"demand"))
					startType = SERVICE_DEMAND_START;
				if (!wcscmp(cl->GetOptionValue(L"-starttype", 1, 1), L"disabled"))
					startType = SERVICE_DISABLED;
			}

			wchar_t* account = cl->GetOptionValue(L"-account", 1, 0);
			wchar_t* password = account ? cl->GetOptionValue(L"-password", 1, 0) : NULL;

			retval |= WrapError(ServiceInstall(startType, account, password, cl->GetOptionValue(L"-setdirectory", 1, 0)), L"Failed to install service, error: %d!", silent);
		}
		else
			retval |= WrapError(4, L"Failed to install service, no path set, error: %d!", silent);
	}

	if (cl->HasParam(L"-start", 1))
		retval |= WrapError(ServiceStart(), L"Failed to start service, error: %d!", silent);

	if (cl->HasParam(L"-run", 1))
	{
		payloadPath = cl->GetOptionValue(L"-path", 1, 0);
		if (payloadPath)
		{
			payloadDir = cl->GetOptionValue(L"-setdirectory", 1, 0);

			SERVICE_TABLE_ENTRY dispatchTable[] = { { (LPWSTR)serviceName, (LPSERVICE_MAIN_FUNCTION)ServiceMain }, { NULL, NULL } };
			StartServiceCtrlDispatcher(dispatchTable);
		}
		else
			retval |= 8;
	}

	delete cl;
	return retval;
}

void WINAPI ServiceMain(DWORD, LPTSTR*)
{
	myServiceStatusHandle = RegisterServiceCtrlHandler(serviceName, ServiceControlHandler);

	if (myServiceStatusHandle)
	{
		myServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		myServiceStatus.dwServiceSpecificExitCode = 0;

		if (!ReportStatusToSCManager(SERVICE_START_PENDING, NO_ERROR, 3000))
			return;

		ServiceBody();
	}

	if (myServiceStatusHandle)
		ReportStatusToSCManager(SERVICE_STOPPED, NO_ERROR, 0);
}

VOID WINAPI ServiceControlHandler(DWORD dwCtrl)
{
	if (dwCtrl == SERVICE_CONTROL_STOP)
	{
		ReportStatusToSCManager(SERVICE_STOP_PENDING, NO_ERROR, 0);

		if (stopEventHandle)
			SetEvent(stopEventHandle);
	}

	ReportStatusToSCManager(myServiceStatus.dwCurrentState, NO_ERROR, 0);
}

BOOL ReportStatusToSCManager(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	if (dwCurrentState == SERVICE_START_PENDING)
		myServiceStatus.dwControlsAccepted = 0;
	else
		myServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	myServiceStatus.dwCurrentState = dwCurrentState;
	myServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
	myServiceStatus.dwWaitHint = dwWaitHint;

	if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
		myServiceStatus.dwCheckPoint = 0;
	else
		myServiceStatus.dwCheckPoint++;

	return SetServiceStatus(myServiceStatusHandle, &myServiceStatus);
}

int ServiceInstall(DWORD dwStartType, wchar_t *account, wchar_t * password, wchar_t *directory)
{
	SC_HANDLE scManager;
	SC_HANDLE scService;

	wchar_t myExePath[MAX_PATH];
	wchar_t serviceBinPath[MAX_PATH];

	memset(myExePath, 0, sizeof(wchar_t) * MAX_PATH);
	memset(serviceBinPath, 0, sizeof(wchar_t) * MAX_PATH);

	if (!GetModuleFileName(NULL, myExePath, MAX_PATH))
		return 16;

	swprintf(serviceBinPath, MAX_PATH - 1, L"\"%s\" -servicename:%s -run -path:\"%s\" -loop:%d -delay:%d%s%s%s", myExePath, serviceName, payloadPath, loopMinutes, delayMinutes, directory ? L" -setdirectory:\"" : L"", directory ? directory : L"", directory ? L"\"" : L"");

	scManager = OpenSCManager(NULL, NULL, GENERIC_ALL);
	if (!scManager)
		return 32;

	scService = CreateService( scManager, serviceName, serviceName, SC_MANAGER_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, dwStartType, SERVICE_ERROR_NORMAL, serviceBinPath, NULL, NULL, NULL, account, password);
	if (!scService)
		return 64;

	CloseServiceHandle(scService);
	CloseServiceHandle(scManager);
	return 0;
}

int ServiceUninstall()
{
	SC_HANDLE scManager;
	SC_HANDLE scService;
	SERVICE_STATUS_PROCESS serviceStatusProcess = { 0 };

	DWORD dummy;

	scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scManager)
		return 128;

	scService = OpenService(scManager, serviceName, SC_MANAGER_ALL_ACCESS);
	if (!scService)
	{
		CloseServiceHandle(scManager);
		return 256;
	}

	for (int i = 0; i < 30; i++)
	{
		if (!QueryServiceStatusEx(scService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatusProcess, sizeof(SERVICE_STATUS_PROCESS), &dummy))
		{
			CloseServiceHandle(scService);
			CloseServiceHandle(scManager);
			return 512;
		}

		switch (serviceStatusProcess.dwCurrentState)
		{
			case SERVICE_RUNNING:
				ControlService(scService, SERVICE_CONTROL_STOP, &myServiceStatus);
				break;

			case SERVICE_STOPPED:
				DeleteService(scService);
				i = 30;
				break;
			default:
				break;
		}

		Sleep(1000);
	}

	CloseServiceHandle(scService);
	CloseServiceHandle(scManager);
	return 0;
}

int ServiceStart()
{
	SC_HANDLE scManager;
	SC_HANDLE scService;

	scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scManager)
		return 1024;

	scService = OpenService(scManager, serviceName, SC_MANAGER_ALL_ACCESS);
	if (!scService)
	{
		CloseServiceHandle(scManager);
		return 2048;
	}

	if (!StartService(scService, 0, NULL))
	{
		CloseServiceHandle(scService);
		CloseServiceHandle(scManager);
		return 4096;
	}

	CloseServiceHandle(scService);
	CloseServiceHandle(scManager);
	return 0;
}

int ServiceStop()
{
	SC_HANDLE scManager;
	SC_HANDLE scService;
	SERVICE_STATUS serviceStatus;

	scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scManager)
		return 8192;

	scService = OpenService(scManager, serviceName, SC_MANAGER_ALL_ACCESS);
	if (!scService)
	{
		CloseServiceHandle(scManager);
		return 16384;
	}

	if (!ControlService(scService, SERVICE_CONTROL_STOP, &serviceStatus))
	{
		CloseServiceHandle(scService);
		CloseServiceHandle(scManager);
		return 32768;
	}

	CloseServiceHandle(scService);
	CloseServiceHandle(scManager);
	return 0;
}

int IsPayloadRunning()
{
	DWORD retval;

	if(GetExitCodeProcess(payloadPI.hProcess, &retval) == 0)
		return 0;

	return retval == STILL_ACTIVE;
}

void StartPayloadProcess()
{
	STARTUPINFO si;
	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);

	memset(&payloadPI, 0, sizeof(PROCESS_INFORMATION));

	CreateProcess(NULL, payloadPath, NULL, NULL, FALSE, 0, NULL, payloadDir, &si, &payloadPI);
}

void ServiceBody()
{
	stopEventHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!stopEventHandle)
		return;

	if (!ReportStatusToSCManager(SERVICE_RUNNING, NO_ERROR, 0))
	{
		CloseHandle(stopEventHandle);
		return;
	}

	if(delayMinutes)
	{
		if (WaitForSingleObject(stopEventHandle, ONEMINUTE * delayMinutes) != WAIT_TIMEOUT)
		{
			CloseHandle(stopEventHandle);
			return;
		}
	}

	StartPayloadProcess();

	while (WaitForSingleObject(stopEventHandle, loopMinutes ? ONEMINUTE * loopMinutes : INFINITE) == WAIT_TIMEOUT)
	{
		if(!IsPayloadRunning())
		{
			#pragma warning(disable:6001)
			if(payloadPI.hProcess)
				CloseHandle(payloadPI.hProcess);
			if (payloadPI.hThread)
				CloseHandle(payloadPI.hThread);
			#pragma warning(default:6001)

			StartPayloadProcess();
		}
	}

	#pragma warning(disable:6001)
	if(payloadPI.hProcess)
	{
		if(IsPayloadRunning())
			TerminateProcess(payloadPI.hProcess, 0);
		CloseHandle(payloadPI.hProcess);
	}
	if (payloadPI.hThread)
		CloseHandle(payloadPI.hThread);
	#pragma warning(default:6001)

	CloseHandle(stopEventHandle);
}

int WrapError(int error, const wchar_t *message, int silent)
{
	if (!error)
		return 0;

	if (!silent)
	{
		wchar_t buf[1024];
		memset(buf, 0, sizeof(wchar_t) * 1024);

		swprintf(buf, 1023, message, error);
		MessageBox(NULL, buf, INTERNALNAME, MB_OK | MB_ICONSTOP);
	}

	return error;
}
