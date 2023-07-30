#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>

#ifndef _countof
    #define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

void ShowError(void)
{
    TCHAR szText[128], szTitle[128];
    LoadString(NULL, 100, szTitle, _countof(szTitle));
    LoadString(NULL, 101, szText, _countof(szText));
    MessageBox(NULL, szText, szTitle, MB_ICONERROR);
}

BOOL RememberFirstTime(void)
{
    HKEY hCompanyKey, hAppKey;
    LONG error;

    error = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Katayama Hirofumi MZ"), 0, NULL, 0,
                           KEY_WRITE, NULL, &hCompanyKey, NULL);
    if (error)
        return FALSE;

    error = RegCreateKeyEx(hCompanyKey, TEXT("Saimin"), 0, NULL, 0, KEY_WRITE, NULL, &hAppKey, NULL);
    if (error)
    {
        RegCloseKey(hCompanyKey);
        return FALSE;
    }

    DWORD dwValue = 1, cbValue = sizeof(dwValue);
    error = RegSetValueEx(hAppKey, TEXT("NonFirstTime"), 0, REG_DWORD, (LPBYTE)&dwValue, cbValue);
    RegCloseKey(hAppKey);
    RegCloseKey(hCompanyKey);
    return error == ERROR_SUCCESS;
}

BOOL IsFirstTime(void)
{
    HKEY hKey;
    LONG error;

    error = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Katayama Hirofumi MZ\\Saimin"), 0, KEY_READ | KEY_WRITE, &hKey);
    if (error)
    {
        RememberFirstTime();
        return TRUE;
    }

    DWORD dwValue = 0, cbValue = sizeof(dwValue);
    error = RegQueryValueEx(hKey, TEXT("NonFirstTime"), NULL, NULL, (LPBYTE)&dwValue, &cbValue);
    RegCloseKey(hKey);
    if (error || dwValue == 0)
    {
        RememberFirstTime();
        return TRUE;
    }

    return FALSE;
}

void ShowFirstTimeNotice(void)
{
    TCHAR szText[128], szTitle[128];
    LoadString(NULL, 100, szTitle, _countof(szTitle));
    LoadString(NULL, 102, szText, _countof(szText));
    MessageBox(NULL, szText, szTitle, MB_ICONINFORMATION);
}

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    TCHAR szPath[MAX_PATH], szParam[MAX_PATH * 2];

    if (IsFirstTime())
        ShowFirstTimeNotice();

    GetModuleFileName(NULL, szPath, _countof(szPath));
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("krakra\\index2.html"));

    _sntprintf(szParam, _countof(szParam),
               TEXT("--kiosk \"%s\" --edge-kiosk-type=fullscreen"), szPath);
    if ((INT_PTR)ShellExecute(NULL, NULL, TEXT("msedge.exe"), szParam, NULL,
                              SW_SHOWNORMAL) <= 32)
    {
        ShowError();
        return 1;
    }

    return 0;
}
