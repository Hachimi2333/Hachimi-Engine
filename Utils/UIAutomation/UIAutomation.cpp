#include <windows.h>
#include <cstdio>
#include <string>

static void PressKey(WORD vk)
{
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
    Sleep(150);
}

static void TypeText(HWND hwnd, const std::wstring& text)
{
    for (wchar_t ch : text)
    {
        PostMessageW(hwnd, WM_CHAR, ch, 1);
        Sleep(50);
    }
}

int main()
{
    std::wstring exePath = L"D:\\Workspace\\Cpp\\Hachimi-Engine\\Bin\\Debug-windows-x86_64\\Hachimi-Editor.exe";
    std::wstring cwd = L"D:\\Workspace\\Cpp\\Hachimi-Engine\\Utils\\UIAutomation";

    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exePath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, cwd.c_str(), &si, &pi))
    {
        printf("CreateProcess failed: %lu\n", GetLastError());
        return 1;
    }
    printf("launched pid=%lu\n", pi.dwProcessId);

    HWND hwnd = nullptr;
    for (int i = 0; i < 100; ++i)
    {
        hwnd = FindWindowW(nullptr, L"Hachimi-Editor");
        if (hwnd) break;
        Sleep(100);
    }
    if (!hwnd)
    {
        printf("window not found\n");
        TerminateProcess(pi.hProcess, 1);
        return 2;
    }
    printf("found window hwnd=%p\n", (void*)hwnd);

    SetForegroundWindow(hwnd);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    Sleep(500);

    // Focus the project name field, type a unique name, then navigate to Create Project.
    PressKey(VK_TAB);
    TypeText(hwnd, L"AutoTestProject");
    PressKey(VK_TAB); // Location
    PressKey(VK_TAB); // Browse...
    PressKey(VK_TAB); // Create Project
    PressKey(VK_RETURN);

    DWORD wait = WaitForSingleObject(pi.hProcess, 8000);
    DWORD code = 999;
    if (wait == WAIT_OBJECT_0)
    {
        GetExitCodeProcess(pi.hProcess, &code);
        printf("process exited code=0x%lX\n", code);
    }
    else
    {
        printf("process still running after 8s, requesting close\n");
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        wait = WaitForSingleObject(pi.hProcess, 5000);
        if (wait == WAIT_OBJECT_0)
        {
            GetExitCodeProcess(pi.hProcess, &code);
            printf("process closed code=0x%lX\n", code);
        }
        else
        {
            printf("process did not close, terminating\n");
            TerminateProcess(pi.hProcess, 2);
        }
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
