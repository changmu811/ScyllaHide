#include "CustomExceptionHandler.h"
#include "Injector.h"
#include "..\InjectorCLI\RemoteHook.h"


t_WaitForDebugEvent dWaitForDebugEvent = 0;
DWORD WaitForDebugEventBackupSize = 0;

typedef void (__cdecl * t_LogWrapper)(const WCHAR * format, ...);
extern t_LogWrapper LogWrap;

extern struct HideOptions pHideOptions;


void handleOutputDebugString( LPDEBUG_EVENT lpDebugEvent )
{
	if (lpDebugEvent->u.DebugString.nDebugStringLength > 0 && lpDebugEvent->u.DebugString.lpDebugStringData != 0)
	{
		HANDLE hProcess = OpenProcess(PROCESS_VM_READ, 0, lpDebugEvent->dwProcessId);

		if (hProcess)
		{
			char* localbuffer = (char*)VirtualAlloc(0, lpDebugEvent->u.DebugString.nDebugStringLength + sizeof(WCHAR), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			if (localbuffer)
			{
				ZeroMemory(localbuffer, lpDebugEvent->u.DebugString.nDebugStringLength + sizeof(WCHAR));
				if (ReadProcessMemory(hProcess, lpDebugEvent->u.DebugString.lpDebugStringData, localbuffer, lpDebugEvent->u.DebugString.nDebugStringLength, NULL))
				{
					LogWrap(L"[ScyllaHide] OUTPUT_DEBUG_STRING: %S", localbuffer);
				}

				VirtualFree(localbuffer, 0, MEM_RELEASE);
			}

			CloseHandle(hProcess);
		}		
	}
	else
	{
		LogWrap(L"[ScyllaHide] Detected possible Anti-Debug method - OUTPUT_DEBUG_STRING");
	}
}

void handleRipEvent( LPDEBUG_EVENT lpDebugEvent )
{
	if (lpDebugEvent->u.RipInfo.dwError == 0)
	{
		LogWrap(L"[ScyllaHide] Detected possible Anti-Debug method - RIP Exception");
		return;
	}

	if (lpDebugEvent->u.RipInfo.dwType == 0)
	{
		LogWrap(L"[ScyllaHide] RIP Exception: Error 0x%X Type NONE", lpDebugEvent->u.RipInfo.dwError);
	}
	else if (lpDebugEvent->u.RipInfo.dwType == SLE_ERROR)
	{
		LogWrap(L"[ScyllaHide] RIP Exception: Error 0x%X Type SLE_ERROR", lpDebugEvent->u.RipInfo.dwError);
	}
	else if (lpDebugEvent->u.RipInfo.dwType == SLE_MINORERROR)
	{
		LogWrap(L"[ScyllaHide] RIP Exception: Error 0x%X Type SLE_MINORERROR", lpDebugEvent->u.RipInfo.dwError);
	}
	else if (lpDebugEvent->u.RipInfo.dwType == SLE_WARNING)
	{
		LogWrap(L"[ScyllaHide] RIP Exception: Error 0x%X Type SLE_WARNING", lpDebugEvent->u.RipInfo.dwError);
	}
	
}

bool AnalyzeDebugStructure( LPDEBUG_EVENT lpDebugEvent )
{
	if (pHideOptions.dontConsumePrintException != 0 && lpDebugEvent->dwDebugEventCode == OUTPUT_DEBUG_STRING_EVENT)
	{
		handleOutputDebugString(lpDebugEvent);
		return true;
	}
	else if (pHideOptions.dontConsumeRipException != 0 && lpDebugEvent->dwDebugEventCode == RIP_EVENT)
	{
		handleRipEvent(lpDebugEvent);
		return true;
	}
	else if (lpDebugEvent->dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
	{
		//FIX F******* OLLY1
		if (lpDebugEvent->u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION)
		{
			LogWrap(L"[ScyllaHide] Ignoring Illegal Instruction Exception at %p", lpDebugEvent->u.Exception.ExceptionRecord.ExceptionAddress);
			return true;
		}
	}

	return false;
}

BOOL WINAPI HookedWaitForDebugEvent(LPDEBUG_EVENT lpDebugEvent, DWORD dwMilliseconds)
{
	BOOL retV = dWaitForDebugEvent(lpDebugEvent, dwMilliseconds);

	if (retV)
	{
		while(1)
		{
			if (AnalyzeDebugStructure(lpDebugEvent))
			{
				ContinueDebugEvent(lpDebugEvent->dwProcessId, lpDebugEvent->dwThreadId, DBG_EXCEPTION_NOT_HANDLED);

				retV = dWaitForDebugEvent(lpDebugEvent, dwMilliseconds);
				if (!retV)
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	return retV;
}


void HookDebugLoop()
{
	BYTE * WaitForIt = (BYTE *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "WaitForDebugEvent");

	if (*WaitForIt == 0xE9)
	{
		MessageBoxW(0, L"WaitForDebugEvent is hooked already!", L"Error", MB_ICONERROR);
	}
	else
	{
		dWaitForDebugEvent = (t_WaitForDebugEvent)DetourCreate(WaitForIt,HookedWaitForDebugEvent, true);
	}	
}