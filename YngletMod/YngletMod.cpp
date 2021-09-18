#include <Windows.h>
#include <strsafe.h>
#include <TlHelp32.h>
#include <PathCch.h>
#include <iostream>

void ErrorExit(LPCTSTR lpszFunction)
{
  // Retrieve the system error message for the last-error code

  LPVOID lpMsgBuf;
  LPVOID lpDisplayBuf;
  DWORD dw = GetLastError();

  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    dw,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&lpMsgBuf,
    0, NULL);

  // Display the error message and exit the process

  lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
    (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen(lpszFunction) + 40) * sizeof(TCHAR));
  StringCchPrintf((LPTSTR)lpDisplayBuf,
    LocalSize(lpDisplayBuf) / sizeof(TCHAR),
    TEXT("%s failed with error %d: %s"),
    lpszFunction, dw, lpMsgBuf);
  MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

  LocalFree(lpMsgBuf);
  LocalFree(lpDisplayBuf);
  ExitProcess(dw);
}

DWORD GetProcId(const wchar_t* procName) {
  DWORD procId = 0;
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

  if (hSnap != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32 procEntry;
    procEntry.dwSize = sizeof(procEntry);

    if (Process32First(hSnap, &procEntry)) {
      do {
        if (!_wcsicmp(procEntry.szExeFile, procName)) {
          procId = procEntry.th32ProcessID;
          break;
        }
      } while (Process32Next(hSnap, &procEntry));
    }
  }
  CloseHandle(hSnap);
  return procId;
}

int main()
{
  const wchar_t* procName = L"Ynglet.exe";
  const WCHAR* dllName = L"\\IL2CppDLL.dll";

  WCHAR dllPath[MAX_PATH];
  GetModuleFileName(NULL, dllPath, MAX_PATH);
  PathCchRemoveFileSpec(dllPath, MAX_PATH);
  const int dllNameSize = (wcslen(dllName) + 1) * sizeof(WCHAR);
  memcpy_s(dllPath + wcslen(dllPath), dllNameSize, dllName, dllNameSize);
  const int dllPathSize = (wcslen(dllPath) + 1) * sizeof(WCHAR);
  std::wcout << dllPath << std::endl;
  std::cout << "Waiting for Ynglet to start..." << std::endl;
  DWORD procId = 0;
  while (!procId) {
    procId = GetProcId(procName);
    Sleep(1000);
  }
  std::cout << "Ynglet process found, proceeding to dll injection." << std::endl;

  HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, procId);
  if (!hProc) ErrorExit(L"OpenProcess");
  if (hProc != INVALID_HANDLE_VALUE) {
    void* loc = VirtualAllocEx(hProc, 0, dllPathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (loc) {
      WriteProcessMemory(hProc, loc, dllPath, dllPathSize, 0);
    }
    else {
      ErrorExit(L"VirtualAllocEx");
    }

    HMODULE hKernel = GetModuleHandle(L"kernel32.dll");
    if (!hKernel) ErrorExit(L"GetModuleHandle");

    HANDLE hThread = CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, loc, 0, 0);
    if (hThread) {
      CloseHandle(hThread);
    }
    else {
      ErrorExit(L"CreateRemoteThread");
    }
  }
  if (hProc) {
    CloseHandle(hProc);
  }
  return 0;
}