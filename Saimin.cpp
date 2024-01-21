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

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    TCHAR szPath[MAX_PATH], szParam[MAX_PATH * 2];

    GetModuleFileName(NULL, szPath, _countof(szPath));
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("krakra\\index.html"));

    _sntprintf(szParam, _countof(szParam), TEXT("--new-window --app=\"%s\""), szPath);
    if ((INT_PTR)ShellExecute(NULL, NULL, TEXT("msedge.exe"), szParam, NULL,
                              SW_SHOWNORMAL) <= 32)
    {
        ShowError();
        return 1;
    }

    return 0;
}
