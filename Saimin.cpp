#define _USE_MATH_DEFINES
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <complex>
#include <vector>
#include <tchar.h>
#include <mmsystem.h>
#include <gl/GL.h>
#include <assert.h>
#include "MRegKey.hpp"

typedef std::complex<double> comp_t;
#ifdef UNICODE
    typedef std::wstring str_t;
#else
    typedef std::string str_t;
#endif

LPCTSTR g_pszClassName = TEXT("Saimin");

enum ADULTCHECK
{
    ADULTCHECK_NOT_CONFIRMED = 0,
    ADULTCHECK_CONFIRMED_CHILD = -1,
    ADULTCHECK_CONFIRMED_ADULT = 1
};

#define TYPE_COUNT 6
#define TIMER_ID 999

HINSTANCE g_hInstance = NULL;
HWND g_hwnd = NULL;
HDC g_hDC = NULL;
HGLRC g_hGLRC = 0;
DWORD g_dwCount = 0;
HWND g_hCmb1 = NULL;
HWND g_hCmb2 = NULL;
HWND g_hCmb3 = NULL;
HWND g_hPsh1 = NULL;
HWND g_hStc1 = NULL;
INT g_nStc1Width = 100;
INT g_nStc1Height = 100;
WNDPROC g_fnStc1OldWndProc = NULL;
INT g_nType = 0;
INT g_nRandomType = 0;
str_t g_strText;
str_t g_strSound;
ADULTCHECK g_nAdultCheck = ADULTCHECK_NOT_CONFIRMED;
BOOL g_bDual = TRUE;
INT g_nDirection = 1;
std::vector<str_t> g_texts;
HANDLE g_hThread = NULL;

#ifndef _countof
    #define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

void circle(double x, double y, double r)
{
    INT N = 15;
    glBegin(GL_POLYGON);
    for (int i = 0; i < N; i++)
    {
        comp_t comp = std::polar(r, 2 * M_PI * i / N);
        glVertex2d(x + comp.real(), y + comp.imag());
    }
    glEnd();
}

void line(double x0, double y0, double x1, double y1, double width)
{
    circle(x0, y0, width);
    circle(x1, y1, width);

    comp_t comp0(cos(M_PI / 2), sin(M_PI / 2));
    comp_t comp1(x1 - x0, y1 - y0);
    double abs = std::abs(comp1);
    if (abs == 0)
        return;

    comp_t comp2 = comp1 / abs;
    comp_t p0 = width * comp2 * comp0;
    comp_t p1 = width * comp2 / comp0;

    glBegin(GL_POLYGON);
    glVertex2d(x0 + p0.real(), y0 + p0.imag());
    glVertex2d(x0 + p1.real(), y0 + p1.imag());
    glVertex2d(x1 + p1.real(), y1 + p1.imag());
    glVertex2d(x1 + p0.real(), y1 + p0.imag());
    glEnd();
}

LPTSTR doLoadString(INT id)
{
    static TCHAR s_szText[MAX_PATH];
    s_szText[0] = 0;
    LoadString(NULL, id, s_szText, _countof(s_szText));
    return s_szText;
}

LPTSTR getSoundPath(LPCTSTR pszFileName)
{
    static TCHAR s_szText[MAX_PATH];
    if (pszFileName[0])
    {
        GetModuleFileName(NULL, s_szText, _countof(s_szText));
        PathRemoveFileSpec(s_szText);
        PathAppend(s_szText, TEXT("Sounds"));
        PathAppend(s_szText, pszFileName);
    }
    else
    {
        s_szText[0] = 0;
    }
    return s_szText;
}

void doListSound(HWND hCmb)
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("Sounds"));
    PathAppend(szPath, TEXT("*.wave"));

    SendMessage(hCmb, CB_ADDSTRING, 0, (LPARAM)TEXT(""));

    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(szPath, &find);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            SendMessage(hCmb, CB_ADDSTRING, 0, (LPARAM)find.cFileName);
        } while (FindNextFile(hFind, &find));

        FindClose(hFind);
    }
}

BOOL loadSetting(void)
{
    LONG error;
    str_t str;

    MRegKey keyApp(HKEY_CURRENT_USER, TEXT("Software\\Katayama Hirofumi MZ\\Saimin"), FALSE);
    if (!keyApp)
        return FALSE;

    error = keyApp.QueryDword(TEXT("AdultCheck"), (DWORD&)g_nAdultCheck);
    if (error)
        g_nAdultCheck = ADULTCHECK_NOT_CONFIRMED;

    error = keyApp.QueryDword(TEXT("Type"), (DWORD&)g_nType);
    if (error)
        g_nType = 0;

    error = keyApp.QueryDword(TEXT("Dual"), (DWORD&)g_bDual);
    if (error)
        g_bDual = 1;

    error = keyApp.QuerySz(TEXT("Sound"), str);
    if (error)
        str = TEXT("");
    g_strSound = str;

    error = keyApp.QuerySz(TEXT("Text"), str);
    if (error)
        str = TEXT("");
    g_strText = str;

    INT nTextCount = 0;
    error = keyApp.QueryDword(TEXT("TextCount"), (DWORD&)nTextCount);
    if (error)
        nTextCount = 0;

    TCHAR szText[MAX_PATH];
    for (INT i = 0; i < nTextCount; ++i)
    {
        wsprintf(szText, TEXT("Text%d"), i);
        error = keyApp.QuerySz(szText, str);
        if (error)
            break;
        g_texts.push_back(str);
    }

    return TRUE;
}

BOOL saveSetting(void)
{
    MRegKey keySoftware(HKEY_CURRENT_USER, TEXT("Software"), TRUE);
    MRegKey keyCompany(keySoftware, TEXT("Katayama Hirofumi MZ"), TRUE);
    MRegKey keyApp(keyCompany, TEXT("Saimin"), TRUE);
    if (!keyApp)
        return FALSE;

    keyApp.SetDword(TEXT("AdultCheck"), g_nAdultCheck);
    keyApp.SetDword(TEXT("Type"), g_nType);
    keyApp.SetDword(TEXT("Dual"), g_bDual);

    keyApp.SetSz(TEXT("Sound"), g_strSound.c_str());
    keyApp.SetSz(TEXT("Text"), g_strText.c_str());

    INT nTextCount = INT(g_texts.size());
    keyApp.SetDword(TEXT("TextCount"), nTextCount);

    TCHAR szText[MAX_PATH];
    for (INT i = 0; i < nTextCount; ++i)
    {
        wsprintf(szText, TEXT("Text%d"), i);
        keyApp.SetSz(szText, g_texts[i].c_str());
    }

    return TRUE;
}

void drawType1(RECT& rc, BOOL bFlag)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;

    glViewport(px, py, cx, cy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + cx, py + cy, py, -1.0, 1.0);

    INT size = ((rc.right - rc.left) + (rc.bottom - rc.top)) / 2;
    double qx, qy;
    {
        comp_t comp;
        if (g_dwCount % 500 > 300)
            comp = std::polar(50.0, M_PI * g_dwCount * 0.02);
        else
            comp = std::polar(50.0, M_PI * g_dwCount * 0.04);
        qx = (rc.left + rc.right) / 2 + comp.real();
        qy = (rc.top + rc.bottom) / 2 + comp.imag();
    }

    double factor = (0.99 + fabs(sin(g_dwCount * 0.2)) * 0.01);

    INT dr0 = 15;
    double dr = dr0 / 2 * factor;
    INT flag2 = bFlag ? g_nDirection : -g_nDirection;
    INT ci = 5;
    for (INT i = 0; i <= ci; ++i)
    {
        INT count = 0;
        double x, y, oldx = qx, oldy = qy;
        for (double radius = 0; radius < size; radius += dr0 / 4)
        {
            double theta = flag2 * count * dr0 / 2;
            if (g_nDirection == 1)
                theta *= 0.75;
            double value = 0.4 * sin(g_dwCount * 0.1) + 0.6;
            glColor3d(1.0, 1.0, value);

            double radian = flag2 * theta * M_PI / 180.0 + i * 360 * M_PI / 180 / ci;
            comp_t comp = std::polar(radius, radian - flag2 * M_PI * g_dwCount * 0.03);
            x = qx + comp.real();
            y = qy + comp.imag();
            if (oldx == MAXLONG && oldy == MAXLONG)
            {
                oldx = x;
                oldy = y;
            }
            line(oldx, oldy, x, y, dr);

            oldx = x;
            oldy = y;
            ++count;
        }
    }

    glColor3d(1.0, 0.5, 0.5);
    circle(qx, qy, dr0);
    glColor3d(1.0, 0.2, 0.2);
    circle(qx, qy, dr0 / 2);
}

void drawType2(RECT& rc, BOOL bFlag)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;

    glViewport(px, py, cx, cy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + cx, py + cy, py, -1.0, 1.0);

    double size = ((rc.right - rc.left) + (rc.bottom - rc.top)) * 0.4;
    double qx, qy;
    {
        comp_t comp = std::polar(size / 50.0, M_PI * g_dwCount * 0.05);
        qx = (rc.left + rc.right) / 2 + comp.real();
        qy = (rc.top + rc.bottom) / 2 + comp.imag();
    }

    double factor = (0.99 + fabs(sin(g_dwCount * 0.2)) * 0.01);

    INT dr0 = 30;
    double dr = dr0 / 2 * factor;
    double radius;
    if (g_dwCount % 1000 < 500)
        radius = INT(g_dwCount * 4) % dr0;
    else
        radius = INT(dr0 - g_dwCount * 4) % dr0;
    INT flag2 = g_nDirection;
    for (; radius < size; radius += dr0)
    {
        flag2 = -flag2;
        double length = 2 * radius * M_PI;
        INT N = INT(length * 2 / dr0);
        double oldx = MAXLONG, oldy = MAXLONG;
        double x, y, x0 = MAXLONG, y0 = MAXLONG;
        for (INT k = 0; k < N; ++k)
        {
            double radian = 2 * M_PI * k / N;
            double value = 0.4 * sin(g_dwCount * 0.1) + 0.6;
            glColor3d(1.0, value, value);
            comp_t comp;
            if (bFlag)
            {
                comp = std::polar(radius * factor, radian + flag2 * M_PI * g_dwCount * 0.02);
            }
            else
            {
                comp = std::polar(radius * factor, radian - flag2 * M_PI * g_dwCount * 0.02);
            }
            x = qx + comp.real();
            y = qy + comp.imag();
            if (oldx == MAXLONG && oldy == MAXLONG)
            {
                x0 = x;
                y0 = y;
                oldx = x;
                oldy = y;
            }
            line(oldx, oldy, x, y, dr * 0.5);
            oldx = x;
            oldy = y;
        }
        double value = 0.4 * sin(g_dwCount * 0.1) + 0.6;
        glColor3d(1.0, value, value);
        line(x0, y0, oldx, oldy, dr * 0.5);
    }
    glColor3d(1.0, 0.3, 0.3);
    circle(qx, qy, dr);
    glColor3d(1.0, 0.6, 0.6);
    circle(qx, qy, dr / 2);
}

void drawType3(RECT& rc, BOOL bFlag)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;

    glViewport(px, py, cx, cy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + cx, py + cy, py, -1.0, 1.0);

    double size = ((rc.right - rc.left) + (rc.bottom - rc.top)) / 2;
    double qx, qy;
    {
        comp_t comp = std::polar(size * 0.01, M_PI * g_dwCount * 0.1);
        qx = (rc.left + rc.right) / 2 + comp.real();
        qy = (rc.top + rc.bottom) / 2 + comp.imag();
    }

    double dr0 = 20;
    double dr = dr0 / 2 * 0.7;
    INT flag2 = bFlag ? g_nDirection : -g_nDirection;
    INT ci = 5;
    for (INT i = 0; i < ci; ++i)
    {
        INT count = 0;
        double oldx = qx, oldy = qy;
        for (double radius = 0; radius < size; radius += dr0)
        {
            double theta = count * dr0 * 1.5;
            double value = 0.6 + 0.4 * sin(g_dwCount * 0.1 + count * 0.1);
            glColor3d(1.0, value, 1.0);

            double radian = flag2 * theta * M_PI / 180.0 + i * 360 * M_PI / 180 / ci;
            comp_t comp = std::polar(radius, radian - flag2 * M_PI * g_dwCount * 0.06);
            double x = qx + comp.real();
            double y = qy + comp.imag();
            line(oldx, oldy, x, y, dr);

            oldx = x;
            oldy = y;
            ++count;
        }
    }

    glColor3d(1.0, 0.5, 0.5);
    circle(qx, qy, dr0);
    glColor3d(1.0, 0.2, 0.2);
    circle(qx, qy, dr0 / 2);
}

void drawType4(RECT& rc, BOOL bFlag)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;

    glViewport(px, py, cx, cy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + cx, py + cy, py, -1.0, 1.0);

    double size = ((rc.right - rc.left) + (rc.bottom - rc.top)) * 0.4;
    double qx, qy;
    {
        comp_t comp = std::polar(20.0, M_PI * g_dwCount * 0.01);
        qx = (rc.left + rc.right) / 2 + comp.real();
        qy = (rc.top + rc.bottom) / 2 + comp.imag();
    }

    double factor = (0.99 + fabs(sin(g_dwCount * 0.2)) * 0.01);

    INT dr0 = 20;
    double dr = dr0 / 4 * factor;
    INT flag2 = (bFlag ? -1 : 1);
    INT ci = 10;
    for (INT i = 0; i < ci; ++i)
    {
        INT count = 0;
        double oldx = MAXLONG, oldy = MAXLONG;
        for (double radius = -size; radius < 0; radius += 16.0)
        {
            double theta = count * dr0 * 0.4;
            double value = 0.6 + 0.4 * sin(g_dwCount * 0.1 + count * 0.1);
            glColor3d(value, 1.0, 1.0);

            double radian = -flag2 * theta * M_PI / 180.0 * 2.0 + i * 360 * M_PI / 180 / ci;
            comp_t comp = std::polar(radius, radian + flag2 * M_PI * g_dwCount * 0.03);
            double x = qx + comp.real() * (2 + sin(g_dwCount * 0.04));
            double y = qy + comp.imag() * (2 + sin(g_dwCount * 0.04));
            if (oldx == MAXLONG && oldy == MAXLONG)
            {
                oldx = x;
                oldy = y;
            }
            line(oldx, oldy, x, y, dr);

            oldx = x;
            oldy = y;
            ++count;
        }
        oldx = oldy = MAXLONG;
        for (double radius = 0; radius < size; radius += 16.0)
        {
            double theta = count * dr0 * 0.4;
            double value = 0.6 + 0.4 * sin(g_dwCount * 0.1 + count * 0.1);
            glColor3d(value, 1.0, 1.0);

            double radian = -flag2 * theta * M_PI / 180.0 * 2.0 + i * 360 * M_PI / 180 / ci;
            comp_t comp = std::polar(radius, radian + flag2 * M_PI * g_dwCount * 0.03);
            double x = qx + comp.real() * (2 + sin(g_dwCount * 0.02));
            double y = qy + comp.imag() * (2 + sin(g_dwCount * 0.02));
            if (oldx == MAXLONG && oldy == MAXLONG)
            {
                oldx = x;
                oldy = y;
            }
            line(oldx, oldy, x, y, dr);

            oldx = x;
            oldy = y;
            ++count;
        }
    }

    glColor3d(1.0, 0.4, 0.4);
    circle(qx, qy, dr * 2);
    glColor3d(1.0, 0.6, 0.6);
    circle(qx, qy, dr);
}

void updateRandom(void)
{
    INT nOldRandomType = g_nRandomType;
    INT nNewRandomType;
    do
    {
        nNewRandomType = rand() % (TYPE_COUNT - 2) + 1;
    } while (nNewRandomType == 0 || nNewRandomType == nOldRandomType || nNewRandomType == TYPE_COUNT - 1);

    g_nRandomType = nNewRandomType;
    g_bDual = (rand() % 3) < 2;
}

void drawRandom(RECT& rc, BOOL bFlag)
{
    if ((g_dwCount % 500) == 0)
        updateRandom();

    switch (g_nRandomType)
    {
    case 0:
        assert(0);
        break;
    case 1:
        drawType1(rc, bFlag);
        break;
    case 2:
        drawType2(rc, bFlag);
        break;
    case 3:
        drawType3(rc, bFlag);
        break;
    case 4:
        drawType4(rc, bFlag);
        break;
    case TYPE_COUNT - 1:
        assert(0);
        break;
    }
}

void drawType(RECT& rc, INT nType, BOOL bFlag)
{
    switch (nType)
    {
    case 0:
        // Do nothing
        break;
    case 1:
        drawType1(rc, bFlag);
        break;
    case 2:
        drawType2(rc, bFlag);
        break;
    case 3:
        drawType3(rc, bFlag);
        break;
    case 4:
        drawType4(rc, bFlag);
        break;
    case TYPE_COUNT - 1:
        drawRandom(rc, bFlag);
        break;
    }
}

void draw(RECT& rc)
{
    if (IsIconic(g_hwnd))
        return;

    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    if (g_bDual)
    {
        RECT rc2;
        if (cx > cy)
        {
            SetRect(&rc2, 0, 0, cx / 2, cy);
            drawType(rc2, g_nType, FALSE);
            SetRect(&rc2, cx / 2, 0, cx, cy);
            drawType(rc2, g_nType, TRUE);
        }
        else
        {
            SetRect(&rc2, 0, 0, cx, cy / 2);
            drawType(rc2, g_nType, FALSE);
            SetRect(&rc2, 0, cy / 2, cx, cy);
            drawType(rc2, g_nType, TRUE);
        }
    }
    else
    {
        drawType(rc, g_nType, FALSE);
    }

    SwapBuffers(g_hDC);
}

BOOL stc1_OnEraseBkgnd(HWND hwnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    HBRUSH hbr = CreateSolidBrush(RGB(0, 191, 0));
    FillRect(hdc, &rc, hbr);
    DeleteObject(hbr);
    return TRUE;
}

void stc1_OnPaint(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    PAINTSTRUCT ps;
    if (HDC hdc = BeginPaint(hwnd, &ps))
    {
        EndPaint(hwnd, &ps);
    }
}

void updateTextRegion(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    INT cx = rc.right - rc.left;
    INT cy = rc.bottom - rc.top;

    static HRGN s_hRgn = NULL;
    if (HDC hdc = GetDC(g_hStc1))
    {
        LOGFONT lf;
        GetObject(GetStockFont(DEFAULT_GUI_FONT), sizeof(lf), &lf);

        INT nMin = (cx + cy) / 2;
        nMin /= (INT(g_strText.size()) + 1);

        if (nMin > 100)
            nMin = 100;
        if (nMin < 40)
            nMin = 40;

        g_nStc1Height = lf.lfHeight = nMin;
        HFONT hFont = CreateFontIndirect(&lf);

        SIZE siz;

        SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ hFontOld = SelectObject(hdc, hFont);
        GetTextExtentPoint32(hdc, g_strText.c_str(), INT(g_strText.size()), &siz);
        g_nStc1Width = siz.cx;

        BeginPath(hdc);
        TextOut(hdc, 0, 0, g_strText.c_str(), INT(g_strText.size()));
        EndPath(hdc);

        SelectObject(hdc, hFontOld);

        HRGN hRgn = PathToRegion(hdc);
        ShowWindow(g_hStc1, SW_HIDE);
        SetWindowRgn(g_hStc1, hRgn, TRUE);
        ShowWindow(g_hStc1, SW_SHOWNOACTIVATE);
        DeleteObject(s_hRgn);
        s_hRgn = hRgn;

        ReleaseDC(g_hStc1, hdc);
    }
}

LRESULT CALLBACK
stc1WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_ERASEBKGND, stc1_OnEraseBkgnd);
        HANDLE_MSG(hwnd, WM_PAINT, stc1_OnPaint);
    default:
        return CallWindowProc(g_fnStc1OldWndProc, hwnd, uMsg, wParam, lParam);
    }
}

BOOL createControls(HWND hwnd)
{
    DWORD style, exstyle;

    // cmb1: The Types
    style = CBS_HASSTRINGS | CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hCmb1 = CreateWindowEx(exstyle, TEXT("COMBOBOX"), NULL, style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)cmb1, g_hInstance, NULL);
    if (!g_hCmb1)
        return FALSE;
    SetWindowFont(g_hCmb1, GetStockFont(DEFAULT_GUI_FONT), TRUE);
    ComboBox_AddString(g_hCmb1, TEXT(""));
    ComboBox_AddString(g_hCmb1, TEXT("Type 1"));
    ComboBox_AddString(g_hCmb1, TEXT("Type 2"));
    ComboBox_AddString(g_hCmb1, TEXT("Type 3"));
    ComboBox_AddString(g_hCmb1, TEXT("Type 4"));
    ComboBox_AddString(g_hCmb1, TEXT("Random"));
    ComboBox_SetCurSel(g_hCmb1, g_nType);

    // cmb2: The Sounds
    style = CBS_HASSTRINGS | CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hCmb2 = CreateWindowEx(exstyle, TEXT("COMBOBOX"), NULL, style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)cmb2, g_hInstance, NULL);
    if (!g_hCmb2)
        return FALSE;
    SetWindowFont(g_hCmb2, GetStockFont(DEFAULT_GUI_FONT), TRUE);
    doListSound(g_hCmb2);
    SetWindowText(g_hCmb2, g_strSound.c_str());
    INT iSound = (INT)SendMessage(g_hCmb2, CB_FINDSTRINGEXACT, -1, (LPARAM)g_strSound.c_str());
    ComboBox_SetCurSel(g_hCmb2, iSound);
    if (iSound != CB_ERR)
    {
        PlaySound(getSoundPath(g_strSound.c_str()), NULL, SND_LOOP | SND_ASYNC);
    }

    // cmb3: Messages ComboBox
    style = CBS_HASSTRINGS | CBS_AUTOHSCROLL | CBS_DROPDOWN | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hCmb3 = CreateWindowEx(exstyle, TEXT("COMBOBOX"), NULL, style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)cmb3, g_hInstance, NULL);
    if (!g_hCmb3)
        return FALSE;
    SetWindowFont(g_hCmb3, GetStockFont(DEFAULT_GUI_FONT), TRUE);
    ComboBox_AddString(g_hCmb3, TEXT(""));
    for (INT i = 200; i <= 207; ++i)
    {
        ComboBox_AddString(g_hCmb3, doLoadString(i));
    }
    for (size_t i = 0; i < g_texts.size(); ++i)
    {
        if (g_texts[i].empty())
            continue;
        ComboBox_AddString(g_hCmb3, g_texts[i].c_str());
    }
    ComboBox_SetCurSel(g_hCmb3, CB_ERR);
    SetWindowText(g_hCmb3, g_strText.c_str());

    // psh1: The "Set" Button
    style = BS_PUSHBUTTON | BS_CENTER | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hPsh1 = CreateWindowEx(exstyle, TEXT("BUTTON"), TEXT("Set"), style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)psh1, g_hInstance, NULL);
    if (!g_hPsh1)
        return FALSE;
    SetWindowFont(g_hPsh1, GetStockFont(DEFAULT_GUI_FONT), TRUE);

    // stc1: The Message
    style = SS_CENTER | SS_NOPREFIX | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hStc1 = CreateWindowEx(exstyle, TEXT("STATIC"), NULL, style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)stc1, g_hInstance, NULL);
    if (!g_hStc1)
        return FALSE;
    g_fnStc1OldWndProc = SubclassWindow(g_hStc1, stc1WindowProc);
    SetWindowFont(g_hStc1, GetStockFont(DEFAULT_GUI_FONT), TRUE);
    SetWindowText(g_hStc1, TEXT("This is a test"));
    updateTextRegion(hwnd);

    PostMessage(hwnd, WM_SIZE, 0, 0);
    return TRUE;
}

void OnTimer(HWND hwnd, UINT id)
{
    if (id == TIMER_ID)
    {
        InvalidateRect(hwnd, NULL, TRUE);
        g_dwCount++;
        if (g_dwCount % 150 == 0 && (rand() % 3 < 2))
            g_nDirection = -g_nDirection;
    }
}

DWORD WINAPI threadProc(LPVOID)
{
    while (g_hThread)
    {
        RECT rc;
        GetClientRect(g_hwnd, &rc);

        RECT rc2;
        rc2.left = (rc.left + rc.right - g_nStc1Width) / 2;
        rc2.top = (rc.top + rc.bottom - g_nStc1Height) / 2;
        rc2.right = rc2.left + g_nStc1Width;
        rc2.bottom = rc2.top + g_nStc1Height;
        INT offset = INT((rc.bottom - rc.top) * (sin(g_dwCount * 0.05) * 0.2 + 1) / 4);
        OffsetRect(&rc2, 0, offset);
        MoveWindow(g_hStc1, rc2.left, rc2.top, rc2.right - rc2.left, rc2.bottom - rc2.top, TRUE);

        Sleep(100);
    }

    return 0;
}

BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    g_hDC = GetDC(hwnd);
    if (!g_hDC)
        return FALSE;

    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    INT iPixelFormat = ::ChoosePixelFormat(g_hDC, &pfd);
    if (!iPixelFormat)
        return FALSE;

    if (!SetPixelFormat(g_hDC, iPixelFormat, &pfd))
        return FALSE;

    g_hGLRC = wglCreateContext(g_hDC);
    if (g_hGLRC == NULL)
        return FALSE;

    wglMakeCurrent(g_hDC, g_hGLRC);

    if (!createControls(hwnd))
        return FALSE;

    g_hThread = CreateThread(NULL, 0, threadProc, NULL, 0, NULL);

    SetTimer(hwnd, TIMER_ID, 0, NULL);
    return TRUE;
}

void OnDestroy(HWND hwnd)
{
    KillTimer(hwnd, TIMER_ID);
    CloseHandle(g_hThread);
    g_hThread = NULL;
    wglMakeCurrent(g_hDC, NULL);
    wglDeleteContext(g_hGLRC);
    ReleaseDC(hwnd, g_hDC);
    PostQuitMessage(0);
}

inline BOOL OnEraseBkgnd(HWND hwnd, HDC hdc)
{
    return TRUE;
}

inline void OnPaint(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    draw(rc);
    PAINTSTRUCT ps;
    if (HDC hDC = BeginPaint(hwnd, &ps))
    {
        EndPaint(hwnd, &ps);
    }
}

void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    INT nHeight = ComboBox_GetItemHeight(g_hCmb1);
    MoveWindow(g_hCmb1, rc.left, rc.bottom - 24, 80, nHeight * 10, TRUE);
    MoveWindow(g_hCmb2, rc.left + 90, rc.bottom - 24, 150, nHeight * 10, TRUE);
    MoveWindow(g_hCmb3, rc.right - 150 - 40, rc.bottom - 24, 150, nHeight * 10, TRUE);
    MoveWindow(g_hPsh1, rc.right - 40, rc.bottom - 24, 40, 24, TRUE);
}

void OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    g_bDual = !g_bDual;
}

void setType(INT nType)
{
    if (g_nType == nType)
        return;

    INT nOldType = g_nType;
    g_nType = nType;
    if (g_nType == TYPE_COUNT - 1)
    {
        g_nRandomType = nOldType;
        updateRandom();
    }
    ComboBox_SetCurSel(g_hCmb1, g_nType);
}

void OnRButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    if (GetAsyncKeyState(VK_SHIFT) < 0)
        setType((g_nType + TYPE_COUNT - 1) % TYPE_COUNT);
    else
        setType((g_nType + 1) % TYPE_COUNT);
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    INT iItem;
    TCHAR szText[256];
    switch (id)
    {
    case cmb1:
        iItem = ComboBox_GetCurSel(g_hCmb1);
        setType(iItem);
        break;
    case cmb2:
        iItem = ComboBox_GetCurSel(g_hCmb2);
        PlaySound(NULL, NULL, SND_PURGE);
        if (iItem == CB_ERR)
        {
            g_strSound.clear();
        }
        else
        {
            ComboBox_GetLBText(g_hCmb2, iItem, szText);
            StrTrim(szText, TEXT(" \t"));
            PlaySound(getSoundPath(szText), NULL, SND_LOOP | SND_ASYNC);
            g_strSound = szText;
        }
        break;
    case psh1:
        {
            iItem = ComboBox_GetCurSel(g_hCmb3);
            if (iItem == CB_ERR)
            {
                GetWindowText(g_hCmb3, szText, _countof(szText));
            }
            else
            {
                ComboBox_GetLBText(g_hCmb3, iItem, szText);
            }
            StrTrim(szText, TEXT(" \t\r\n"));

            iItem = ComboBox_FindStringExact(g_hCmb3, -1, szText);
            if (szText[0] && iItem == CB_ERR)
            {
                g_texts.push_back(szText);
                ComboBox_AddString(g_hCmb3, szText);
            }
            g_strText = szText;

            updateTextRegion(hwnd);
        }
        break;
    }
}

void OnMButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    g_nDirection = -g_nDirection;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
        HANDLE_MSG(hwnd, WM_ERASEBKGND, OnEraseBkgnd);
        HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
        HANDLE_MSG(hwnd, WM_SIZE, OnSize);
        HANDLE_MSG(hwnd, WM_TIMER, OnTimer);
        HANDLE_MSG(hwnd, WM_LBUTTONDOWN, OnLButtonDown);
        HANDLE_MSG(hwnd, WM_LBUTTONDBLCLK, OnLButtonDown);
        HANDLE_MSG(hwnd, WM_MBUTTONDOWN, OnMButtonDown);
        HANDLE_MSG(hwnd, WM_MBUTTONDBLCLK, OnMButtonDown);
        HANDLE_MSG(hwnd, WM_RBUTTONDOWN, OnRButtonDown);
        HANDLE_MSG(hwnd, WM_RBUTTONDBLCLK, OnRButtonDown);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

static BOOL registerClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcx = { sizeof(wcx) };
    wcx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcx.lpfnWndProc = WindowProc;
    wcx.hInstance = hInstance;
    wcx.hIcon = ::LoadIcon(g_hInstance, MAKEINTRESOURCE(1));
    wcx.hCursor = ::LoadCursor(NULL, IDC_ARROW);
    wcx.lpszClassName = g_pszClassName;
    if (!::RegisterClassEx(&wcx))
    {
        MessageBoxW(NULL, doLoadString(104), NULL, MB_ICONERROR);
        return FALSE;
    }
    return TRUE;
}

static BOOL createWindow(HINSTANCE instance)
{
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    DWORD exstyle = 0;
    g_hwnd = CreateWindowEx(exstyle, g_pszClassName, doLoadString(106), style,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                            NULL, NULL, instance, NULL);
    if (g_hwnd == NULL)
    {
        MessageBoxW(NULL, doLoadString(105), NULL, MB_ICONERROR);
        return FALSE;
    }
    return TRUE;
}

void dontUseChild(void)
{
    MessageBox(NULL, doLoadString(103), NULL, MB_ICONERROR);
}

BOOL doAdultCheck(void)
{
    if (g_nAdultCheck == ADULTCHECK_NOT_CONFIRMED)
    {
        str_t checkTitle = doLoadString(100);
        if (MessageBox(NULL, doLoadString(101), checkTitle.c_str(), MB_ICONWARNING | MB_YESNO) == IDYES)
        {
            dontUseChild();
            g_nAdultCheck = ADULTCHECK_CONFIRMED_CHILD;
            return FALSE;
        }

        if (MessageBox(NULL, doLoadString(102), checkTitle.c_str(), MB_ICONWARNING | MB_YESNO) == IDNO)
        {
            dontUseChild();
            g_nAdultCheck = ADULTCHECK_CONFIRMED_CHILD;
            return FALSE;
        }

        g_nAdultCheck = ADULTCHECK_CONFIRMED_ADULT;
    }

    if (g_nAdultCheck == ADULTCHECK_CONFIRMED_CHILD)
    {
        dontUseChild();
        return FALSE;
    }

    return TRUE;
}

INT APIENTRY
WinMain(HINSTANCE  hInstance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        INT       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_hInstance = hInstance;
    InitCommonControls();

    loadSetting();

    if (!doAdultCheck())
    {
        saveSetting();
        return 0;
    }

    if (!registerClass(hInstance))
        return -1;

    if (!createWindow(hInstance))
        return -2;

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    saveSetting();

#if defined(_MSC_VER) && !defined(NDEBUG)
    // for detecting memory leak (MSVC only)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    return (INT)msg.wParam;
}
