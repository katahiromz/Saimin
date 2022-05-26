#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>

#ifdef UNICODE
    typedef std::wstring str_t;
#else
    typedef std::string str_t;
#endif

static std::wstring s_strMessage;
extern LPTSTR doLoadString(INT id);
extern str_t g_strText;
extern std::vector<str_t> g_texts;

static BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    HWND hCmb1 = GetDlgItem(hwnd, cmb1);

    ComboBox_AddString(hCmb1, TEXT(""));
    for (INT i = 200; i <= 209; ++i)
    {
        ComboBox_AddString(hCmb1, doLoadString(i));
    }
    for (size_t i = 0; i < g_texts.size(); ++i)
    {
        if (g_texts[i].empty())
            continue;
        ComboBox_AddString(hCmb1, g_texts[i].c_str());
    }
    ComboBox_SetCurSel(hCmb1, CB_ERR);
    SetWindowText(hCmb1, g_strText.c_str());
    return TRUE;
}

static BOOL doSendMessage(HWND hwnd, LPCTSTR pszText)
{
    HWND hTarget = FindWindow(TEXT("FREEGLUT"), doLoadString(106));
    if (hTarget)
    {
        COPYDATASTRUCT cds = { 0xDEADBEEF, lstrlen(pszText) * sizeof(TCHAR), (PVOID)pszText };
        SendMessage(hTarget, WM_COPYDATA, (WPARAM)hwnd, (LPARAM)&cds);
        return TRUE;
    }
    return FALSE;
}

static BOOL OnOK(HWND hwnd)
{
    HWND hCmb1 = GetDlgItem(hwnd, cmb1);

    TCHAR szText[MAX_PATH];
    INT iItem = ComboBox_GetCurSel(hCmb1);
    if (iItem == CB_ERR)
    {
        ComboBox_GetText(hCmb1, szText, _countof(szText));
    }
    else
    {
        ComboBox_GetLBText(hCmb1, iItem, szText);
    }

    doSendMessage(hwnd, szText);
    return TRUE;
}

static void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDOK:
        if (OnOK(hwnd))
            EndDialog(hwnd, id);
        break;
    case IDCANCEL:
        EndDialog(hwnd, id);
        break;
    case IDNO:
        doSendMessage(hwnd, TEXT(""));
        EndDialog(hwnd, id);
        break;
    }
}

static INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    }
    return 0;
}

BOOL saveSettings(void);

INT ShowInputBox(HINSTANCE hInstance, INT myargc, LPWSTR *myargv)
{
    if (myargc != 3)
        return -1;
    if (lstrcmpiW(myargv[1], L"--set-message") != 0)
        return -2;
    s_strMessage = myargv[2];
    if (DialogBox(hInstance, MAKEINTRESOURCE(1), NULL, DialogProc) == IDCANCEL)
        return -1;
    saveSettings();
    return 0;
}
