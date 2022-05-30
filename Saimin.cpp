#define _USE_MATH_DEFINES
#include "tess.h"
#include <vector>
#include <string>
#include <complex>
#include <math.h>
#include <assert.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <tchar.h>
#include <mmsystem.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <dlgs.h>
#include <algorithm>
#include "MRegKey.hpp"
#include "WinVoice.hpp"
#include "mic/mic.h"
#ifndef _OBJBASE_H_
    #include <objbase.h>
#endif

#define TYPE_COUNT 5
INT g_nType = 0;

#ifdef UNICODE
    typedef std::wstring str_t;
#else
    typedef std::string str_t;
#endif
typedef std::complex<double> dcomp_t;
typedef std::complex<float> fcomp_t;

enum ADULTCHECK
{
    ADULTCHECK_NOT_CONFIRMED = 0,
    ADULTCHECK_CONFIRMED_CHILD = -1,
    ADULTCHECK_CONFIRMED_ADULT = 1
};
ADULTCHECK g_nAdultCheck = ADULTCHECK_NOT_CONFIRMED;

#define MAX_STARS 64
struct STAR
{
    INT x, y;
    double size;
};
STAR g_stars[MAX_STARS];

HINSTANCE g_hInstance = NULL;
HWND g_hwnd = NULL;
HICON g_hIcon = NULL;
HICON g_hIconSm = NULL;
BOOL g_bMaximized = FALSE;
WNDPROC g_fnOldWndProc = NULL;
HWND g_hCmb1 = NULL;
HWND g_hCmb2 = NULL;
HWND g_hCmb4 = NULL;
HWND g_hChx1 = NULL;
HWND g_hChx2 = NULL;
str_t g_strText;
str_t g_strSound;
std::vector<str_t> g_texts;
HWND g_hPsh1 = NULL;
HWND g_hPsh2 = NULL;
HWND g_hStc1 = NULL;
INT g_cxStc1 = 100;
INT g_cyStc1 = 100;
WNDPROC g_fnStc1OldWndProc = NULL;
HANDLE g_hMessageThread = NULL;
HANDLE g_hSpeechThread = NULL;
HFONT g_hUIFont = NULL;
HFONT g_hMsgFont = NULL;
HBITMAP g_hbm = NULL;
GLsizei g_width, g_height;
int g_nMouseX = -1, g_nMouseY = -1;
bool g_bMouseLeftButton = false, g_bMouseRightButton = false;
int g_nDrawMode = 0;
GLuint g_nListId = 0;
double g_counter = 0;
DWORD g_dwOldTick = 0;
double g_eSpeed = 45.0;
DWORD g_dwFPS = 0;
BOOL g_bPerfCounter = FALSE;
LARGE_INTEGER g_freq, g_old_tick;
INT g_division = -1;
INT g_stc1deltay = 0;

WinVoice *g_pWinVoice = NULL;
BOOL g_bCoInit = FALSE;
BOOL g_bSpeech = FALSE;
BOOL g_bMic = FALSE;
HANDLE g_hMicThread = NULL;
static INT s_iSound = 1;

#ifndef _countof
    #define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

LPTSTR doLoadString(INT id)
{
    static TCHAR s_szText[MAX_PATH];
    s_szText[0] = 0;
    LoadString(NULL, id, s_szText, _countof(s_szText));
    return s_szText;
}

MCIERROR mciSendF(LPCTSTR fmt, ...);

template <typename T_STR>
inline bool
mstr_replace_all(T_STR& str, const T_STR& from, const T_STR& to)
{
    bool ret = false;
    size_t i = 0;
    for (;;) {
        i = str.find(from, i);
        if (i == T_STR::npos)
            break;
        ret = true;
        str.replace(i, from.size(), to);
        i += to.size();
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////////

bool isLargeDisplay(void)
{
    return g_width >= 1500 || g_height >= 1500;
}

void addStar(INT x, INT y)
{
    if (isLargeDisplay())
    {
        x += (std::rand() % 80) - 40;
        y += (std::rand() % 80) - 40;
        MoveMemory(&g_stars[0], &g_stars[1], sizeof(g_stars) - sizeof(STAR));
        g_stars[MAX_STARS - 1].x = x;
        g_stars[MAX_STARS - 1].y = y;
        g_stars[MAX_STARS - 1].size = 5 + std::rand() % 20;
    }
    else
    {
        x += (std::rand() % 40) - 20;
        y += (std::rand() % 40) - 20;
        MoveMemory(&g_stars[0], &g_stars[1], sizeof(g_stars) - sizeof(STAR));
        g_stars[MAX_STARS - 1].x = x;
        g_stars[MAX_STARS - 1].y = y;
        g_stars[MAX_STARS - 1].size = 5 + std::rand() % 10;
    }
}

inline void rectangle(double x0, double y0, double x1, double y1)
{
    glBegin(GL_POLYGON);
    glVertex2d(x0, y0);
    glVertex2d(x1, y0);
    glVertex2d(x1, y1);
    glVertex2d(x0, y1);
    glEnd();
}

inline void circle(double x, double y, double r, bool bFill = true, INT N = 10)
{
    if (bFill)
        glBegin(GL_POLYGON);
    else
        glBegin(GL_LINE_LOOP);
    for (int i = 0; i < N; i++)
    {
        dcomp_t comp = std::polar(r, 2 * M_PI * i / N);
        glVertex2d(x + comp.real(), y + comp.imag());
    }
    glEnd();
}

void line(double x0, double y0, double x1, double y1, double width, INT N = 12)
{
    width *= 0.5;

    circle(x0, y0, width, true, N);
    circle(x1, y1, width, true, N);

    dcomp_t comp0 = std::polar(1.0, M_PI / 2);
    dcomp_t comp1(x1 - x0, y1 - y0);
    double abs = std::abs(comp1);
    if (abs == 0)
        return;

    dcomp_t comp2 = comp1 / abs;
    dcomp_t p0 = width * comp2 * comp0;
    dcomp_t p1 = width * comp2 / comp0;

    glBegin(GL_POLYGON);
    glVertex2d(x0 + p0.real(), y0 + p0.imag());
    glVertex2d(x0 + p1.real(), y0 + p1.imag());
    glVertex2d(x1 + p1.real(), y1 + p1.imag());
    glVertex2d(x1 + p0.real(), y1 + p0.imag());
    glEnd();
}

void bezier3(
    INT count,
    GLdouble xy[][2],
    GLdouble x0, GLdouble y0,
    GLdouble x1, GLdouble y1,
    GLdouble x2, GLdouble y2,
    GLdouble x3, GLdouble y3)
{
    int i = 0;
    xy[i][0] = x0;
    xy[i][1] = y0;
    for (++i; i < count - 1; ++i)
    {
        GLdouble t = float(i) / (count - 1);
        GLdouble t_2 = t * t;
        GLdouble t_3 = t_2 * t;
        GLdouble one_minus_t = 1 - t;
        GLdouble one_minus_t_2 = one_minus_t * one_minus_t;
        GLdouble one_minus_t_3 = one_minus_t_2 * one_minus_t;
        GLdouble x = (one_minus_t_3 * x0) + (3 * one_minus_t_2 * t * x1) + (3 * one_minus_t * t_2 * x2) + (t_3 * x3);
        GLdouble y = (one_minus_t_3 * y0) + (3 * one_minus_t_2 * t * y1) + (3 * one_minus_t * t_2 * y2) + (t_3 * y3);
        xy[i][0] = x;
        xy[i][1] = y;
    }
    xy[i][0] = x3;
    xy[i][1] = y3;
}

void eye(double x0, double y0, double r, double opened = 1.0)
{
    double r025 = r * 0.25f;
    double r05 = r025 * 2 * opened;

    const int num = 10;
    GLdouble xy[2 * num][2];
    bezier3(num, &xy[0], x0 - r, y0, x0 - r025, y0 - r05, x0 + r025, y0 - r05, x0 + r, y0);
    bezier3(num, &xy[num], x0 + r, y0, x0 + r025, y0 + r05, x0 - r025, y0 + r05, x0 - r, y0);

    glBegin(GL_LINE_LOOP);
    for (INT i = 0; i < 2 * num; ++i)
    {
        glVertex2d(xy[i][0], xy[i][1]);
    }
    glEnd();

    circle(x0 - r, y0, 5 / 2);
    circle(x0 + r, y0, 5 / 2);

    circle(x0, y0, r / 3 * opened, true, 14);
}

void heart(double x0, double y0, double x1, double y1, double r0, double g0, double b0)
{
    double x2 = (0.6 * x0 + 0.4 * x1);
    double y2 = (0.6 * y0 + 0.4 * y1);
    dcomp_t comp = dcomp_t(x1 - x0, y1 - y0);
    dcomp_t comp0 = std::polar(1.0, M_PI * 0.5);
    dcomp_t p0 = comp * (comp0 / 16.0) + dcomp_t(x0, y0);
    dcomp_t p1 = comp / (comp0 * 16.0) + dcomp_t(x0, y0);
    dcomp_t p2 = comp * comp0 + dcomp_t(x0, y0);
    dcomp_t p3 = comp / comp0 + dcomp_t(x0, y0);
    INT i0 = tessBegin();
    tessBezier3(x2, y2, p0.real(), p0.imag(), p2.real(), p2.imag(), x1, y1, r0, g0, b0, 0);
    tessBezier3(x1, y1, p3.real(), p3.imag(), p1.real(), p1.imag(), x2, y2, r0, g0, b0, 0);
    INT i1 = tessEnd();
    INT id = tess(i1 - i0, vertices[i0], true);
    glCallList(id);
    glDeleteLists(id, 1);
}

void light(double x0, double y0, double radius)
{
    double r0 = radius;
    double rmid = radius * 0.333f;
    glBegin(GL_POLYGON);
    glVertex2d(x0 - r0, y0);
    glVertex2d(x0, y0 + rmid);
    glVertex2d(x0 + r0, y0);
    glVertex2d(x0, y0 - rmid);
    glEnd();

    glBegin(GL_POLYGON);
    glVertex2d(x0, y0 - r0);
    glVertex2d(x0 + rmid, y0);
    glVertex2d(x0, y0 + r0);
    glVertex2d(x0 - rmid, y0);
    glEnd();
}

void leftExtend(INT count, GLdouble xy[][2], double x0, double y0, double x1, double y1)
{
    double dx = x1 - x0;
    double dy = y1 - y0;

    dcomp_t comp(dx, dy);
    double abs = std::abs(comp);
    if (abs <= 0)
        return;

    comp /= abs;
    comp *= std::polar(1.0, -M_PI / 2);
    comp *= abs * 0.15;

    double cx0, cy0, cx1, cy1;
    cx0 = cx1 = (x0 + x1) / 2;
    cy0 = cy1 = (y0 + y1) / 2;
    cx0 += comp.real();
    cy0 += comp.imag();
    cx1 += comp.real();
    cy1 += comp.imag();
    bezier3(count, xy, x0, y0, cx0, cy0, cx1, cy1, x1, y1);
}

BOOL loadSettings(void)
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

    error = keyApp.QueryDword(TEXT("division"), (DWORD&)g_division);
    if (error)
        g_division = -1;

    error = keyApp.QueryDword(TEXT("Maximized"), (DWORD&)g_bMaximized);
    if (error)
        g_bMaximized = FALSE;

    error = keyApp.QuerySz(TEXT("Text"), str);
    if (error)
        str = TEXT("");
    g_strText = str;

    error = keyApp.QuerySz(TEXT("Sound"), str);
    if (error)
        str = TEXT("");
    g_strSound = str;

    error = keyApp.QueryDword(TEXT("Speech"), (DWORD&)g_bSpeech);
    if (error)
        g_bSpeech = FALSE;

    error = keyApp.QueryDword(TEXT("Mic"), (DWORD&)g_bMic);
    if (error)
        g_bMic = FALSE;

    INT nTextCount = 0;
    error = keyApp.QueryDword(TEXT("TextCount"), (DWORD&)nTextCount);
    if (error)
        nTextCount = 0;

    TCHAR szText[MAX_PATH];
    for (INT i = 0; i < nTextCount; ++i)
    {
        StringCchPrintf(szText, _countof(szText), TEXT("Text%d"), i);
        error = keyApp.QuerySz(szText, str);
        if (error)
            break;
        g_texts.push_back(str);
    }

    std::sort(g_texts.begin(), g_texts.end());
    g_texts.erase(std::unique(g_texts.begin(), g_texts.end()), g_texts.end());

    return TRUE;
}

BOOL saveSettings(void)
{
    MRegKey keySoftware(HKEY_CURRENT_USER, TEXT("Software"), TRUE);
    MRegKey keyCompany(keySoftware, TEXT("Katayama Hirofumi MZ"), TRUE);
    MRegKey keyApp(keyCompany, TEXT("Saimin"), TRUE);
    if (!keyApp)
        return FALSE;

    keyApp.SetDword(TEXT("AdultCheck"), g_nAdultCheck);
    keyApp.SetDword(TEXT("Type"), g_nType);
    keyApp.SetDword(TEXT("division"), g_division);
    keyApp.SetDword(TEXT("Maximized"), g_bMaximized);
    keyApp.SetDword(TEXT("Speech"), g_bSpeech);
    keyApp.SetDword(TEXT("Mic"), g_bMic);

    keyApp.SetSz(TEXT("Text"), g_strText.c_str());
    keyApp.SetSz(TEXT("Sound"), g_strSound.c_str());

    INT nTextCount = INT(g_texts.size());
    keyApp.SetDword(TEXT("TextCount"), nTextCount);

    TCHAR szText[MAX_PATH];
    for (INT i = 0; i < nTextCount; ++i)
    {
        StringCchPrintf(szText, _countof(szText), TEXT("Text%d"), i);
        keyApp.SetSz(szText, g_texts[i].c_str());
    }

    return TRUE;
}

void setDivision(INT value)
{
    g_division = value;

    INT division = -1, iItem = (INT)SendMessage(g_hCmb4, CB_GETCURSEL, 0, 0);
    switch (iItem)
    {
    case 0:
        division = -1;
        break;
    case 1:
        division = 1;
        break;
    case 2:
        division = 2;
        break;
    }

    if (g_division != division)
    {
        switch (g_division)
        {
        case -1:
            PostMessage(g_hCmb4, CB_SETCURSEL, 0, 0);
            break;
        case 1:
            PostMessage(g_hCmb4, CB_SETCURSEL, 1, 0);
            break;
        case 2:
            PostMessage(g_hCmb4, CB_SETCURSEL, 2, 0);
            break;
        }
    }

    saveSettings();
}

void setType(INT nType)
{
    g_nType = nType;

    nType = (INT)SendMessage(g_hCmb1, CB_GETCURSEL, 0, 0);
    if (g_nType != nType)
    {
        PostMessage(g_hCmb1, CB_SETCURSEL, g_nType, 0);
    }

    saveSettings();
}

static DWORD WINAPI speechThreadProc(LPVOID)
{
    if (!g_pWinVoice)
    {
        return -1;
    }

    // slow and low-pitch
    str_t str = TEXT("<pitch absmiddle=\"-10\"><rate absspeed=\"-4\">");
    str += g_strText;
    str += TEXT("</rate></pitch>");
    mstr_replace_all(str, str_t(TEXT("\uFF5E")), str_t(TEXT("\u30FC"))); // "`" --> "["

    while (g_hSpeechThread)
    {
        if (g_pWinVoice)
            g_pWinVoice->Speak(str);

        if (g_pWinVoice)
            g_pWinVoice->WaitUntilDone(INFINITE);
    }

    return 0;
}

void setSpeech(BOOL bSpeech)
{
    g_bSpeech = bSpeech;

    if (g_pWinVoice)
        g_pWinVoice->Pause();
    if (g_pWinVoice)
        g_pWinVoice->SetMute(TRUE);

    if (g_bSpeech && g_strText.size())
    {
        if (g_hSpeechThread)
        {
            HANDLE hThread = g_hSpeechThread;
            g_hSpeechThread = NULL;
            WaitForSingleObject(hThread, 200);
            CloseHandle(hThread);
        }
        if (g_pWinVoice)
        {
            WinVoice *pWinVoice = g_pWinVoice;
            g_pWinVoice = NULL;
            delete pWinVoice;
            g_pWinVoice = new WinVoice();

            g_hSpeechThread = CreateThread(NULL, 0, speechThreadProc, NULL, 0, NULL);
        }
    }
    else
    {
        if (g_hSpeechThread)
        {
            CloseHandle(g_hSpeechThread);
            g_hSpeechThread = NULL;
        }
    }

    if (bSpeech)
    {
        if (IsDlgButtonChecked(g_hwnd, chx1) != BST_CHECKED)
            CheckDlgButton(g_hwnd, chx1, BST_CHECKED);
    }
    else
    {
        if (IsDlgButtonChecked(g_hwnd, chx1) == BST_CHECKED)
            CheckDlgButton(g_hwnd, chx1, BST_UNCHECKED);
    }

    saveSettings();
}

void drawType1(INT px, INT py, INT dx, INT dy)
{
    glPushMatrix();

    glViewport(px, py, dx, dy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + dx, py + dy, py, -1.0, 1.0);

    double qx = px + double(dx) / 2;
    double qy = py + double(dy) / 2;
    double size = double(dx + dy) * 2 / 5;
    double count2 = g_counter;
    {
        dcomp_t comp;
        if (isLargeDisplay())
            comp = std::polar(60.0, count2 * 0.1);
        else
            comp = std::polar(30.0, count2 * 0.1);
        qx += comp.real();
        qy += comp.imag();
    }

    glColor3d(1.0, 1.0, 1.0);

    double dr0 = 15;
    double dr = dr0 / 2;
    INT flag2 = -1;
    INT ci = 6;
    for (INT i = 0; i <= ci; ++i)
    {
        INT count = 0;
        double x, y, oldx = qx, oldy = qy, f = 0.5;
        for (double radius = 0; radius < size; radius += f)
        {
            double theta = dr0 * count * 0.375;
            double radian = theta * (M_PI / 180.0) + i * (2 * M_PI) / ci;
            dcomp_t comp = std::polar(radius, flag2 * radian - count2 * (M_PI * 0.03));
            x = qx + comp.real();
            y = qy + comp.imag();
            line(oldx, oldy, x, y, dr * f * 0.666);
            oldx = x;
            oldy = y;
            ++count;
            f *= 1.02;
        }
    }

    glPopMatrix();
}

void drawType2(INT px, INT py, INT dx, INT dy, bool flag = false)
{
    glPushMatrix();

    glViewport(px, py, dx, dy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + dx, py + dy, py, -1.0, 1.0);

    double qx = px + double(dx) / 2;
    double qy = py + double(dy) / 2;
    double dxy = (dx + dy) / 2;
    double count2 = g_counter;
    double factor = (0.99 + std::fabs(std::sin(count2 * 0.2)) * 0.01);
    double size = (dx + dy) * 0.4;

    if (flag)
    {
        glEnable(GL_STENCIL_TEST);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_ALWAYS, 1, ~0);
        glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);

        double value = 0.2 + 0.2 * std::fabs(std::sin(count2 * 0.02));
        glColor3d(1.0, 1.0, 1.0);
        circle(qx, qy, dxy * value, true, INT(value * 100));

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilFunc(GL_NOTEQUAL, 0, ~0);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glDepthMask(GL_TRUE);

        glColor3d(0, 0, 0);
        circle(qx, qy, dxy * value, true, INT(value * 100));
    }

    double dr0 = 30;
    if (isLargeDisplay())
    {
        dr0 *= 2;
        count2 *= 2;
    }
    double dr = dr0 / 2 * factor;
    double radius = std::fmod(count2 * 4, dr0);
    if (flag)
        radius = dr0 - radius;

    if (radius < 0)
        radius = -radius;

    glLineWidth(5);
    glColor3d(1, 1, 1);
    for (; radius < size; radius += dr0)
    {
        INT n = INT(radius / 4);
        if (n < 40)
            n = 40;
        circle(qx, qy, radius, false, n);
        circle(qx, qy, radius + 4, false, n);
        circle(qx, qy, radius + 8, false, n);
        if (isLargeDisplay())
        {
            circle(qx, qy, radius - 4, false, n);
        }
    }

    if (flag)
    {
        glDisable(GL_STENCIL_TEST);
        glColorMask(1, 1, 1, 1);
        glDepthMask(1);
    }

    glPopMatrix();
}

void hsv2rgb(double& r, double& g, double& b, double h, double s, double v)
{
    r = g = b = v;
    if (s > 0)
    {
        h *= 6;
        int i = (int)h;
        double f = h - i;
        switch (i)
        {
        case 0:
        default:
            g *= 1 - s * (1 - f);
            b *= 1 - s;
            break;
        case 1:
            r *= 1 - s * f;
            b *= 1 - s;
            break;
        case 2:
            r *= 1 - s;
            b *= 1 - s * (1 - f);
            break;
        case 3:
            r *= 1 - s;
            g *= 1 - s * f;
            break;
        case 4:
            r *= 1 - s * (1 - f);
            g *= 1 - s;
            break;
        case 5:
            g *= 1 - s;
            b *= 1 - s * f;
            break;
        }
    }
}

void whiteOut(double qx, double qy, double dxy)
{
    double r9 = dxy * 0.75;
    INT angle_delta = 30;
    for (INT angle = 0; angle < 360; angle += angle_delta)
    {
        double radian = angle * (M_PI / 180);
        dcomp_t comp = std::polar(r9, radian);
        glBegin(GL_POLYGON);
        glColor4f(1.0, 1.0, 1.0, 0.0);
        glVertex2d(qx, qy);
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glVertex2d(qx + comp.real(), qy + comp.imag());
        radian = (angle + angle_delta) * (M_PI / 180);
        comp = std::polar(r9, radian);
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glVertex2d(qx + comp.real(), qy + comp.imag());
        glEnd();
    }
}

void drawType3(INT px, INT py, INT dx, INT dy)
{
    double qx = px + dx / 2.0;
    double qy = py + dy / 2.0;
    double dxy = (dx + dy) / 2.0;

    glViewport(px, py, dx, dy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + dx, py + dy, py, -1.0, 1.0);

    double count2 = g_counter;
    double factor = count2 * 0.03;

    double size = 32;
    if (isLargeDisplay())
        size *= 2;
    INT nCount2 = 0;
    double cxy = ((dx >= dy) ? dy : dx) * 1.2;
    double xy0 = (cxy + size) - std::fmod((cxy + size), size);
    dcomp_t comp0 = std::polar(1.0, factor * 1.0);
    for (double y = -xy0; y < cxy + size; y += size)
    {
        INT nCount = nCount2 % 3;
        for (double x = -xy0; x < cxy + size; x += size)
        {
            double h, s = 1.0, v = 1.0;
            switch (int(nCount) % 3)
            {
            case 0:
                h = std::fmod(0.2 + factor * 1.2, 1.0);
                break;
            case 1:
                h = std::fmod(0.4 + factor * 1.2, 1.0);
                break;
            case 2:
                h = std::fmod(0.8 + factor * 1.2, 1.0);
                break;
            }
            double r, g, b;
            hsv2rgb(r, g, b, h, s, v);

            if (r > 0.8)
                r = 1.0;
            if (g > 0.8)
                g = 1.0;
            if (b > 0.8)
                b = 1.0;

            glColor3d(r, g, b);

            double x0 = x - size / 2;
            double y0 = y - size / 2;
            double x1 = x0 + size;
            double y1 = y0 + size;
            glBegin(GL_POLYGON);
            dcomp_t comp1 = dcomp_t(x0, y0);
            comp1 *= comp0;
            glVertex2d(qx + comp1.real(), qy + comp1.imag());
            dcomp_t comp2 = dcomp_t(x0, y1);
            comp2 *= comp0;
            glVertex2d(qx + comp2.real(), qy + comp2.imag());
            dcomp_t comp3 = dcomp_t(x1, y1);
            comp3 *= comp0;
            glVertex2d(qx + comp3.real(), qy + comp3.imag());
            dcomp_t comp4 = dcomp_t(x1, y0);
            comp4 *= comp0;
            glVertex2d(qx + comp4.real(), qy + comp4.imag());
            glEnd();

            if (x >= 0)
                nCount = (nCount + 2) % 3;
            else
                nCount += 1;
        }
        if (y >= 0)
            nCount2 = (nCount2 + 2) % 3;
        else
            nCount2 += 1;
    }

    dxy = double((dx >= dy) ? dx : dy);

    glLineWidth(5);

    int i = 0;
    glColor3d(1.0, 20.0 / 255.0, 29 / 255.0);
    for (double r = std::fmod(count2 * 2, 100); r < cxy; r += 100) {
        if (r < 200)
            circle(qx, qy, r, false, 20);
        else
            circle(qx, qy, r, false, INT(r / 10.0f));
        ++i;
    }

    dxy = (dx >= dy) ? dx : dy;
    whiteOut(qx, qy, dxy);

    glColor3d(0, 0, 0);
    eye(qx, qy, cxy / 10, 1.0);

    double opened = 1.0;
    double f = std::sin(std::fabs(count2 * 0.1));
    if (f >= 0.8)
    {
        opened = 0.6 + 0.4 * std::fabs(std::sin(f * M_PI));
    }

    double N = 4;
    double delta = (2 * M_PI) / N;
    double radian = factor;
    for (double i = 0; i < N; i += 1)
    {
        double x = qx + cxy * std::cos(radian) * 0.3;
        double y = qy + cxy * std::sin(radian) * 0.3;
        eye(x, y, cxy / 10.0, opened);
        radian += delta;
    }
}

void drawType4_5(INT px, INT py, INT dx, INT dy, INT t)
{
    double qx = px + dx / 2.0;
    double qy = py + dy / 2.0;
    double dxy = (dx + dy) / 2.0;

    glViewport(px, py, dx, dy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + dx, py + dy, py, -1.0, 1.0);

    double count2 = g_counter;
    double factor = count2 * 0.16;

    if (isLargeDisplay())
    {
        qx += 60 * std::cos(factor * 0.8);
        qy += 60 * std::sin(factor * 0.8);
    }
    else
    {
        qx += 30 * std::cos(factor * 0.8);
        qy += 30 * std::sin(factor * 0.8);
    }

    bool isLarge = isLargeDisplay();
    for (double radius = (isLarge ? ((dx + dy) * 0.2) : ((dx + dy) * 0.4)); radius >= 10; radius *= 0.92)
    {
        double r0, g0, b0;
        hsv2rgb(r0, g0, b0, std::fmod(dxy + factor * 0.3 - radius * 0.015, 1.0), 1.0, 1.0);
        glColor3d(r0, g0, b0);

        INT N0 = 10, N1 = 5;
        INT i = 0;
        double oldx = qx, oldy = qy;
        for (double angle = 0; angle <= 360; angle += 360 / N0)
        {
            double radian = (angle + count2 * 2) * (M_PI / 180.0);
            double factor2 = radius * (1.1 + 0.7 * std::fabs(std::sin(N1 * i * M_PI / N0)));
            if (isLarge)
                factor2 *= 2;
            double x = qx + factor2 * std::cos(radian);
            double y = qy + factor2 * std::sin(radian);
            if (oldx == qx && oldy == qy)
            {
                oldx = x;
                oldy = y;
            }
            if (i > 0)
            {
                const int num = 10;
                GLdouble xy[num][2];

                leftExtend(num, xy, oldx, oldy, x, y);

                glBegin(GL_POLYGON);
                INT m = 0;
                glVertex2d(qx, qy);
                for (m = 0; m < num; ++m)
                {
                    glVertex2d(xy[m][0], xy[m][1]);
                }
                glEnd();
            }
            oldx = x;
            oldy = y;
            ++i;
        }
    }

    dxy = (dx >= dy) ? dx : dy;
    whiteOut(qx, qy, dxy);

    if (t == 4)
    {
        glColor4d(1.0, 1.0, std::fmod(factor * (10 / 255.0), 1.0), 0.8);
        INT M = 5;
        for (double radius = std::fmod(factor * 10, 100); radius < dxy; radius += 100)
        {
            for (double angle = 0; angle < 360; angle += 360 / M)
            {
                double radian = angle * (M_PI / 180.0);
                double x0 = qx + radius * std::cos(radian + factor * 0.1 + radius / 100);
                double y0 = qy + radius * std::sin(radian + factor * 0.1 + radius / 100);
                light(x0, y0, std::fmod(radius * 0.1, 30) + 10);
            }
        }
    } 
    else if (t == 5)
    {
        double value = factor * 25 + 10;
        double gb = std::fmod(value / 191, 1.0);
        glColor3d(1.0, gb, gb);
        INT M = 5;
        INT heartSize = 30;
        for (double radius = std::fmod(factor * 10, 100) + 30; radius < dxy; radius += 100)
        {
            for (double angle = 0; angle < 360; angle += 360 / M) 
            {
                double radian = angle * (M_PI / 180.0);
                double x0 = qx + radius * std::cos(radian + factor * 0.1 + radius / 100);
                double y0 = qy + radius * std::sin(radian + factor * 0.1 + radius / 100);
                heart(x0, y0, x0, y0 + heartSize + std::fmod(value, 191) / 12, 1.0, gb, gb);
            }
            heartSize += 5;
        }
    }
}

void drawType(INT px, INT py, INT dx, INT dy)
{
    switch (g_nType)
    {
    case 0:
        break;
    case 1:
        drawType1(px, py, dx, dy);
        break;
    case 2:
        drawType2(px, py, dx, dy, false);
        drawType2(px, py, dx, dy, true);
        break;
    case 3:
        drawType3(px, py, dx, dy);
        break;
    case 4:
    case 5:
        drawType4_5(px, py, dx, dy, g_nType);
        break;
    }
}

void draw(void)
{
    DWORD dwTick0 = GetTickCount();

    INT x = g_width / 2, y = g_height / 2;
    INT cx = g_width, cy = g_height;

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (g_division == 1)
    {
        drawType(0, 0, cx, cy);
        g_stc1deltay = cy / 4;
    }
    else if (g_division == -1)
    {
        if (cx >= cy * 1.75)
        {
            drawType(0, 0, cx / 2, cy);
            drawType(cx / 2, 0, cx / 2, cy);
            g_stc1deltay = 0;
        }
        else if (cy >= cx * 1.75)
        {
            drawType(0, 0, cx, cy / 2);
            drawType(0, cy / 2, cx, cy / 2);
            g_stc1deltay = 0;
        }
        else
        {
            drawType(0, 0, cx, cy);
            g_stc1deltay = cy / 4;
        }
    }
    else if (g_division == 2)
    {
        g_stc1deltay = 0;
        if (cx >= cy)
        {
            drawType(0, 0, cx / 2, cy);
            drawType(cx / 2, 0, cx / 2, cy);
        }
        else
        {
            drawType(0, 0, cx, cy / 2);
            drawType(0, cy / 2, cx, cy / 2);
        }
    }

    // draw stars
    {
        glViewport(0, 0, cx, cy);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, cx, cy, 0, -1.0, 1.0);

        for (INT iStar = 0; iStar < _countof(g_stars); ++iStar)
        {
            STAR& star = g_stars[iStar];
            if (star.size > 0)
            {
                glColor4f(1.0, 1.0, 0.0, 0.8);
                light(star.x, star.y, star.size);
                if (star.size > 1.0) {
                    star.size *= 0.97;
                }
            }
        }

        MoveMemory(&g_stars[0], &g_stars[1], sizeof(g_stars) - sizeof(STAR));
        g_stars[MAX_STARS - 1].size = 0;
    }

    glutSwapBuffers();

    if (g_bPerfCounter)
    {
        LARGE_INTEGER new_tick;
        QueryPerformanceCounter(&new_tick);
        DWORD tick = (DWORD)((new_tick.QuadPart - g_old_tick.QuadPart) * 1000 / g_freq.QuadPart);
        double diff = tick / 1000.0;
        g_counter += diff * g_eSpeed;
        g_old_tick = new_tick;
    }
    else
    {
        DWORD dwNewTick = GetTickCount();
        double diff = (dwNewTick - g_dwOldTick) / 1000.0;
        g_counter += diff * g_eSpeed;
        g_dwOldTick = dwNewTick;
    }

    DWORD dwTick1 = GetTickCount();
    g_dwFPS = dwTick1 - dwTick0;
}

void displayCB()
{
    draw();
}

void reshapeCB(int w, int h)
{
    g_width = w;
    g_height = h;
    glutPostRedisplay();
}

void timerCB(int param)
{
    glutTimerFunc(g_dwFPS, timerCB, 0);
    glutPostRedisplay();
}

void idleCB()
{
    glutPostRedisplay();
}

void keyboardCB(unsigned char key, int x, int y)
{
    ;
}

void mouseCB(int button, int state, int x, int y)
{
    g_nMouseX = x;
    g_nMouseY = y;

    switch (button)
    {
    case GLUT_LEFT_BUTTON:
        if (state == GLUT_DOWN)
        {
            if (GetAsyncKeyState(VK_SHIFT) < 0)
            {
                setType(((g_nType + TYPE_COUNT - 2) % TYPE_COUNT) + 1);
            }
            else
            {
                setType((g_nType % TYPE_COUNT) + 1);
            }

            // play kirakira.mp3
            mciSendF(TEXT("stop file%d"), 0);
            mciSendF(TEXT("seek file%d to start"), 0);
            mciSendF(TEXT("play file%d notify"), 0);

            g_bMouseLeftButton = true;
        }
        else if (state == GLUT_UP)
        {
            g_bMouseLeftButton = false;
        }
        break;

    case GLUT_RIGHT_BUTTON:
        if (state == GLUT_DOWN)
        {
            if (GetAsyncKeyState(VK_SHIFT) < 0)
            {
                switch (g_division)
                {
                case -1:
                    setDivision(2);
                    break;
                case 1:
                default:
                    setDivision(-1);
                    break;
                case 2:
                    setDivision(1);
                    break;
                }
            }
            else
            {
                switch (g_division)
                {
                case -1:
                    setDivision(1);
                    break;
                case 1:
                    setDivision(2);
                    break;
                case 2:
                default:
                    setDivision(-1);
                    break;
                }
            }
            g_bMouseRightButton = true;
        }
        else if (state == GLUT_UP)
        {
            g_bMouseRightButton = false;
        }
        break;

    case 3:
        // mouse wheel up
        if (g_eSpeed < 80.0)
            g_eSpeed += 5.0;
        else
            g_eSpeed = 80.0;
        break;

    case 4:
        // mouse wheel down
        if (g_eSpeed > 0.0)
            g_eSpeed -= 5.0;
        else
            g_eSpeed = 0.0;
        break;
    }
}

void mouseMotionCB(int x, int y)
{
    if (g_bMouseLeftButton)
    {
        ;
    }
    if (g_bMouseRightButton)
    {
        ;
    }
}

void initLights(void)
{
#if 0
    GLfloat lightKa[] = {.2f, .2f, .2f, 1.0f};  // ambient light
    GLfloat lightKd[] = {.7f, .7f, .7f, 1.0f};  // diffuse light
    GLfloat lightKs[] = {1, 1, 1, 1};           // specular light
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightKa);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightKd);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightKs);

    // position the light
    float lightPos[4] = {0, 0, 20, 1}; // positional light
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    glEnable(GL_LIGHT0);
#endif
}

void setCamera(float posX, float posY, float posZ, float targetX, float targetY, float targetZ)
{
    //glMatrixMode(GL_MODELVIEW);
    //glLoadIdentity();
    //gluLookAt(posX, posY, posZ, targetX, targetY, targetZ, 0, 1, 0); // eye(x,y,z), focal(x,y,z), up(x,y,z)
}

inline BOOL stc1_OnEraseBkgnd(HWND hwnd, HDC hdc)
{
    return TRUE;
}

void stc1_CalcSize(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    if (HDC hdc = GetDC(hwnd))
    {
        SelectObject(hdc, g_hMsgFont);
        UINT uFlags = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
        DrawText(hdc, g_strText.c_str(), -1, &rc, uFlags | DT_CALCRECT);
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        g_cxStc1 = rc.right - rc.left;
        g_cyStc1 = tm.tmHeight;
        ReleaseDC(hwnd, hdc);
    }
}

void stc1_OnPaint(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    INT cx = rc.right, cy = rc.bottom;

    BITMAP bm;
    if (g_hbm)
    {
        GetObject(g_hbm, sizeof(bm), &bm);
        if (bm.bmWidth != cx || bm.bmHeight != cy)
        {
            DeleteObject(g_hbm);
            g_hbm = NULL;
        }
    }

    PAINTSTRUCT ps;
    if (HDC hdc = BeginPaint(hwnd, &ps))
    {
        if (HDC hdcMem = CreateCompatibleDC(hdc))
        {
            if (!g_hbm)
            {
                g_hbm = CreateCompatibleBitmap(hdc, cx, cy);
                HGDIOBJ hbmOld = SelectObject(hdcMem, g_hbm);
                {
                    FillRect(hdcMem, &rc, GetStockBrush(DKGRAY_BRUSH));
                    SetTextColor(hdcMem, RGB(191, 255, 255));
                    SetBkMode(hdcMem, TRANSPARENT);
                    SelectObject(hdcMem, g_hMsgFont);
                    UINT uFlags = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
                    DrawText(hdcMem, g_strText.c_str(), -1, &rc, uFlags);
                }
                SelectObject(hdcMem, hbmOld);
            }

            {
                HGDIOBJ hbmOld = SelectObject(hdcMem, g_hbm);
                BitBlt(hdc, 0, 0, cx, cy, hdcMem, 0, 0, SRCCOPY);
                SelectObject(hdcMem, hbmOld);
            }

            DeleteDC(hdcMem);
        }

        EndPaint(hwnd, &ps);
    }
}

LRESULT CALLBACK
stc1WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_ERASEBKGND, stc1_OnEraseBkgnd);
        HANDLE_MSG(hwnd, WM_PAINT, stc1_OnPaint);
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
        return SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
    default:
        return CallWindowProc(g_fnStc1OldWndProc, hwnd, uMsg, wParam, lParam);
    }
}

static DWORD WINAPI messageThreadProc(LPVOID)
{
    while (g_hMessageThread)
    {
        RECT rc;
        GetClientRect(g_hwnd, &rc);

        if (g_strText.empty())
        {
            ShowWindowAsync(g_hStc1, SW_HIDE);
        }
        else
        {
            RECT rc2;
            rc2.left = (rc.left + rc.right - g_cxStc1) / 2;
            rc2.top = (rc.top + rc.bottom - g_cyStc1) / 2;
            rc2.right = rc2.left + g_cxStc1;
            rc2.bottom = rc2.top + g_cyStc1;
            double offset = (rc.bottom - rc.top) * sin(g_counter * 0.08) * 0.05 + g_stc1deltay;
            OffsetRect(&rc2, 0, INT(offset));
            MoveWindow(g_hStc1, rc2.left, rc2.top, g_cxStc1, g_cyStc1, FALSE);
            ShowWindowAsync(g_hStc1, SW_SHOWNOACTIVATE);
            InvalidateRect(g_hStc1, NULL, TRUE);
        }

        Sleep(100);
    }

    return 0;
}

LPTSTR getSoundPath(LPCTSTR pszFileName)
{
    static TCHAR s_szText[MAX_PATH];
    if (pszFileName[0])
    {
        GetModuleFileName(NULL, s_szText, _countof(s_szText));
        PathRemoveFileSpec(s_szText);
        PathAppend(s_szText, TEXT("Sounds"));
        if (!PathIsDirectory(s_szText))
        {
            PathRemoveFileSpec(s_szText);
            PathRemoveFileSpec(s_szText);
            PathAppend(s_szText, TEXT("Sounds"));
            if (!PathIsDirectory(s_szText))
            {
                PathRemoveFileSpec(s_szText);
                PathRemoveFileSpec(s_szText);
                PathAppend(s_szText, TEXT("Sounds"));
            }
        }
        PathAppend(s_szText, pszFileName);
    }
    else
    {
        s_szText[0] = 0;
    }
    return s_szText;
}

BOOL isFileWav(LPCTSTR pszFileName)
{
    return lstrcmpi(PathFindExtension(pszFileName), TEXT(".wav")) == 0;
}
BOOL isFileMp3(LPCTSTR pszFileName)
{
    return lstrcmpi(PathFindExtension(pszFileName), TEXT(".mp3")) == 0;
}
BOOL isFileMid(LPCTSTR pszFileName)
{
    return lstrcmpi(PathFindExtension(pszFileName), TEXT(".mid")) == 0;
}

MCIERROR mciSendF(LPCTSTR fmt, ...)
{
    TCHAR szText[MAX_PATH * 2];
    va_list va;
    va_start(va, fmt);
    StringCchVPrintf(szText, _countof(szText), fmt, va);
    MCIERROR ret = mciSendString(szText, NULL, 0, g_hwnd);
    va_end(va);
    if (ret)
    {
        StringCchPrintf(szText, _countof(szText), TEXT("%d"), ret);
    }
    return ret;
}

VOID doRepeatSound(void)
{
    mciSendF(TEXT("seek file%d to start"), s_iSound);
    mciSendF(TEXT("play file%d notify"), s_iSound);
}

VOID doPlaySound(LPCTSTR file, BOOL bPlay = TRUE)
{
    if (bPlay)
    {
        if (isFileMp3(file))
        {
            mciSendF(TEXT("open \"%s\" type mpegvideo alias file%d"), file, s_iSound);
            mciSendF(TEXT("play file%d notify"), s_iSound);
        }
        else if (isFileMid(file))
        {
            mciSendF(TEXT("open \"%s\" type sequencer alias file%d"), file, s_iSound);
            mciSendF(TEXT("play file%d notify"), s_iSound);
        }
        else if (isFileWav(file))
        {
            mciSendF(TEXT("open \"%s\" alias file%d"), file, s_iSound);
            mciSendF(TEXT("play file%d notify"), s_iSound);
        }
    }
    else
    {
        mciSendF(TEXT("stop file%d"), s_iSound);
        mciSendF(TEXT("close file%d"), s_iSound);
        ++s_iSound;
    }
}

void doListSound(HWND hCmb)
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("Sounds"));
    if (!PathIsDirectory(szPath))
    {
        PathRemoveFileSpec(szPath);
        PathRemoveFileSpec(szPath);
        PathAppend(szPath, TEXT("Sounds"));
        if (!PathIsDirectory(szPath))
        {
            PathRemoveFileSpec(szPath);
            PathRemoveFileSpec(szPath);
            PathAppend(szPath, TEXT("Sounds"));
        }
    }
    PathAppend(szPath, TEXT("*.*"));

    SendMessage(hCmb, CB_ADDSTRING, 0, (LPARAM)TEXT("(None)"));

    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(szPath, &find);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (isFileWav(find.cFileName) ||
                isFileMp3(find.cFileName) ||
                isFileMid(find.cFileName))
            {
                SendMessage(hCmb, CB_ADDSTRING, 0, (LPARAM)find.cFileName);
            }
        } while (FindNextFile(hFind, &find));

        FindClose(hFind);
    }
}

BOOL createControls(HWND hwnd)
{
    DWORD style, exstyle;

    {
        HFONT hFont = GetStockFont(DEFAULT_GUI_FONT);
        LOGFONT lf;
        GetObject(hFont, sizeof(lf), &lf);
        lf.lfHeight = 16;
        g_hUIFont = CreateFontIndirect(&lf);
    }

    // cmb1: The Types
    style = CBS_HASSTRINGS | CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hCmb1 = CreateWindowEx(exstyle, TEXT("COMBOBOX"), NULL, style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)cmb1, g_hInstance, NULL);
    if (!g_hCmb1)
        return FALSE;
    SetWindowFont(g_hCmb1, g_hUIFont, TRUE);
    ComboBox_AddString(g_hCmb1, TEXT("None"));
    ComboBox_AddString(g_hCmb1, TEXT("Type 1"));
    ComboBox_AddString(g_hCmb1, TEXT("Type 2"));
    ComboBox_AddString(g_hCmb1, TEXT("Type 3"));
    ComboBox_AddString(g_hCmb1, TEXT("Type 4"));
    ComboBox_AddString(g_hCmb1, TEXT("Type 5"));
    ComboBox_SetCurSel(g_hCmb1, g_nType);

    // cmb2: The Sounds
    style = CBS_HASSTRINGS | CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hCmb2 = CreateWindowEx(exstyle, TEXT("COMBOBOX"), NULL, style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)cmb2, g_hInstance, NULL);
    if (!g_hCmb2)
        return FALSE;
    SetWindowFont(g_hCmb2, g_hUIFont, TRUE);
    doListSound(g_hCmb2);
    SetWindowText(g_hCmb2, g_strSound.c_str());
    INT iSound = (INT)SendMessage(g_hCmb2, CB_FINDSTRINGEXACT, -1, (LPARAM)g_strSound.c_str());
    ComboBox_SetCurSel(g_hCmb2, iSound);

    if (iSound != CB_ERR && g_strSound != TEXT("(None)"))
    {
        PostMessage(g_hwnd, WM_COMMAND, IDOK, 0);
    }

    // cmb4: Division ComboBox
    style = CBS_HASSTRINGS | CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hCmb4 = CreateWindowEx(exstyle, TEXT("COMBOBOX"), NULL, style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)cmb4, g_hInstance, NULL);
    if (!g_hCmb4)
        return FALSE;
    SetWindowFont(g_hCmb4, g_hUIFont, TRUE);
    for (INT i = 210; i <= 212; ++i)
    {
        ComboBox_AddString(g_hCmb4, doLoadString(i));
    }
    setDivision(g_division);

    // chx1: Speech checkbox
    style = BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hChx1 = CreateWindowEx(exstyle, TEXT("BUTTON"), TEXT("Speech"), style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)chx1, g_hInstance, NULL);
    if (!g_hChx1)
        return FALSE;
    SetWindowFont(g_hChx1, g_hUIFont, TRUE);
    setSpeech(g_bSpeech);

    // chx2: "mic" checkbox
    style = BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hChx2 = CreateWindowEx(exstyle, TEXT("BUTTON"), TEXT("mic"), style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)chx2, g_hInstance, NULL);
    if (!g_hChx2)
        return FALSE;
    SetWindowFont(g_hChx2, g_hUIFont, TRUE);
    if (g_bMic)
    {
        CheckDlgButton(hwnd, chx2, BST_CHECKED);
        micOn();
        micEchoOn();
    }

    // psh1: The "msg" Button
    style = BS_PUSHBUTTON | BS_CENTER | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hPsh1 = CreateWindowEx(exstyle, TEXT("BUTTON"), TEXT("msg"), style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)psh1, g_hInstance, NULL);
    if (!g_hPsh1)
        return FALSE;
    SetWindowFont(g_hPsh1, g_hUIFont, TRUE);

    // psh2: The "?" Button
    g_hPsh2 = CreateWindowEx(exstyle, TEXT("BUTTON"), TEXT("?"), style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)psh2, g_hInstance, NULL);
    if (!g_hPsh2)
        return FALSE;
    SetWindowFont(g_hPsh2, g_hUIFont, TRUE);

    // stc1: The Message
    style = SS_CENTER | SS_NOPREFIX | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hStc1 = CreateWindowEx(exstyle, TEXT("STATIC"), NULL, style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)stc1, g_hInstance, NULL);
    if (!g_hStc1)
        return FALSE;
    g_fnStc1OldWndProc = SubclassWindow(g_hStc1, stc1WindowProc);

    {
        HFONT hFont = GetStockFont(DEFAULT_GUI_FONT);
        LOGFONT lf;
        GetObject(hFont, sizeof(lf), &lf);
        lf.lfHeight = 32;
        if (isLargeDisplay())
            lf.lfHeight *= 2;
        g_hMsgFont = CreateFontIndirect(&lf);
        SetWindowFont(g_hStc1, g_hMsgFont, TRUE);
    }

    SetWindowText(g_hStc1, g_strText.c_str());
    stc1_CalcSize(g_hStc1);
    g_hMessageThread = CreateThread(NULL, 0, messageThreadProc, NULL, 0, NULL);

    // ready kirakira.mp3
    TCHAR szText[MAX_PATH];
    GetModuleFileName(NULL, szText, _countof(szText));
    PathRemoveFileSpec(szText);
    PathAppend(szText, TEXT("kirakira.mp3"));
    mciSendF(TEXT("open \"%s\" type mpegvideo alias file%d"), szText, 0);

    // Reposition
    PostMessage(hwnd, WM_SIZE, 0, 0);
    return TRUE;
}

void destroyControls(HWND hwnd)
{
    CloseHandle(g_hMessageThread);
    g_hMessageThread = NULL;
    CloseHandle(g_hSpeechThread);
    g_hSpeechThread = NULL;
    DestroyWindow(g_hCmb1);
    g_hCmb1 = NULL;
    DestroyWindow(g_hCmb4);
    g_hCmb4 = NULL;
    DestroyWindow(g_hPsh1);
    g_hPsh1 = NULL;
    DestroyWindow(g_hPsh2);
    g_hPsh2 = NULL;
    DestroyWindow(g_hStc1);
    g_hStc1 = NULL;
    DeleteObject(g_hUIFont);
    g_hUIFont = NULL;
    DeleteObject(g_hMsgFont);
    g_hMsgFont = NULL;
    DestroyIcon(g_hIcon);
    g_hIcon = NULL;
    DestroyIcon(g_hIconSm);
    g_hIconSm = NULL;
}

void setSound(LPCTSTR pszText)
{
    if (pszText && pszText[0] && lstrcmpi(pszText, TEXT("(None)")) != 0)
    {
        PostMessage(g_hwnd, WM_COMMAND, IDCANCEL, 0);
        PostMessage(g_hwnd, WM_COMMAND, IDOK, 0);
    }
    else
    {
        PostMessage(g_hwnd, WM_COMMAND, IDCANCEL, 0);
    }
    g_strSound = pszText;
    saveSettings();
}

LRESULT DefWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return CallWindowProc(g_fnOldWndProc, hwnd, uMsg, wParam, lParam);
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    INT iItem;
    TCHAR szText[MAX_PATH];
    switch (id)
    {
    case cmb1:
        iItem = ComboBox_GetCurSel(g_hCmb1);
        if (iItem != CB_ERR)
            setType(iItem);
        break;
    case cmb2:
        iItem = ComboBox_GetCurSel(g_hCmb2);
        if (iItem == CB_ERR)
        {
            g_strSound.clear();
            setSound(TEXT(""));
        }
        else
        {
            ComboBox_GetLBText(g_hCmb2, iItem, szText);
            StrTrim(szText, TEXT(" \t"));
            setSound(szText);
        }
        saveSettings();
        break;
    case cmb4:
        iItem = ComboBox_GetCurSel(g_hCmb4);
        if (iItem != CB_ERR)
        {
            switch (iItem)
            {
            case 0:
                setDivision(-1);
                break;
            case 1:
                setDivision(1);
                break;
            case 2:
                setDivision(2);
                break;
            }
        }
        break;
    case psh1:
        {
            TCHAR szPath[MAX_PATH];
            GetModuleFileName(NULL, szPath, _countof(szPath));
            str_t str = TEXT("--set-message \"");
            str += g_strText;
            str += TEXT("\"");
            ShellExecute(hwnd, NULL, szPath, str.c_str(), NULL, SW_SHOWNORMAL);
        }
        saveSettings();
        break;
    case psh2:
        {
            TCHAR szPath[MAX_PATH];
            GetModuleFileName(NULL, szPath, _countof(szPath));
            PathRemoveFileSpec(szPath);
            PathAppend(szPath, TEXT("ReadMe.txt"));
            ShellExecute(hwnd, NULL, szPath, NULL, NULL, SW_SHOWNORMAL);
        }
        break;
    case chx1:
        setSpeech((IsDlgButtonChecked(hwnd, chx1) == BST_CHECKED));
        break;
    case chx2:
        if (IsDlgButtonChecked(hwnd, chx2) == BST_CHECKED)
        {
            g_bMic = TRUE;
            micOn();
            micEchoOn();
        }
        else
        {
            g_bMic = FALSE;
            micOff();
            micEchoOff();
        }
        saveSettings();
        break;
    case IDYES:
        createControls(hwnd);
        break;
    case IDOK:
        doPlaySound(getSoundPath(g_strSound.c_str()), TRUE);
        break;
    case IDCANCEL:
        doPlaySound(NULL, FALSE);
        break;
    }

    FORWARD_WM_COMMAND(hwnd, id, hwndCtl, codeNotify, DefWndProc);
}

void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    INT nHeight = ComboBox_GetItemHeight(g_hCmb1);
    MoveWindow(g_hCmb1, rc.left, rc.bottom - 24, 80, nHeight * 10, TRUE);
    MoveWindow(g_hChx2, rc.left, rc.bottom - 48, 60, 24, TRUE);
    MoveWindow(g_hCmb2, rc.left + 90, rc.bottom - 24, 150, nHeight * 10, TRUE);
    MoveWindow(g_hChx1, rc.left + 250, rc.bottom - 24, 80, 24, TRUE);
    MoveWindow(g_hCmb4, rc.right - 160 - 40, rc.bottom - 24, 90, nHeight * 10, TRUE);
    MoveWindow(g_hPsh1, rc.right - 100, rc.bottom - 24, 60, 24, TRUE);
    MoveWindow(g_hPsh2, rc.right - 30, rc.bottom - 24, 30, 24, TRUE);

    g_bMaximized = IsZoomed(hwnd);
    saveSettings();

    FORWARD_WM_SIZE(hwnd, state, cx, cy, DefWndProc);
}

void OnDestroy(HWND hwnd)
{
    FORWARD_WM_DESTROY(hwnd, DefWndProc);
}

void OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags)
{
    static INT s_nFlag = 0;
    if (s_nFlag == 0)
    {
        addStar(x, y);
    }
    s_nFlag = (s_nFlag + 1) % 1;

    FORWARD_WM_MOUSEMOVE(hwnd, x, y, keyFlags, DefWndProc);
}

BOOL OnCopyData(HWND hwnd, HWND hwndFrom, PCOPYDATASTRUCT pcds)
{
    if (!pcds || pcds->dwData != 0xDEADBEEF)
        return FALSE;

    std::wstring str((LPWSTR)pcds->lpData, pcds->cbData / sizeof(WCHAR));
    g_strText = str;

    BOOL bFound = str.empty();
    for (INT i = 210; i <= 212; ++i)
    {
        if (lstrcmp(doLoadString(i), str.c_str()) == 0)
        {
            bFound = TRUE;
            break;
        }
    }

    if (!bFound)
        g_texts.push_back(str);

    stc1_CalcSize(g_hStc1);
    if (g_hbm)
    {
        DeleteObject(g_hbm);
        g_hbm = NULL;
    }
    InvalidateRect(g_hStc1, NULL, TRUE);
    setSpeech(g_bSpeech);
    loadSettings();
    return TRUE;
}

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_SIZE, OnSize);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
        HANDLE_MSG(hwnd, WM_MOUSEMOVE, OnMouseMove);
        HANDLE_MSG(hwnd, WM_COPYDATA, OnCopyData);
    case MM_MCINOTIFY:
        switch (wParam)
        {
        case MCI_NOTIFY_SUCCESSFUL:
            doRepeatSound();
            break;
        }
        break;
    default:
        return CallWindowProc(g_fnOldWndProc, hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void initGL(void)
{
    glShadeModel(GL_SMOOTH); // GL_SMOOTH or GL_FLAT

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //glEnable(GL_DEPTH_TEST);
    //glEnable(GL_LIGHTING);
    //glEnable(GL_TEXTURE_2D);
    //glEnable(GL_CULL_FACE);

    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);

    glClearColor(0, 0, 0, 0);                   // background color
    glClearStencil(0);                          // clear stencil buffer
    glClearDepth(1.0);                          // 0 is near, 1 is far
    glDepthFunc(GL_LEQUAL);

    initLights();
    setCamera(0, 0, 5, 0, 0, 0);
}

int initGLUT(int argc, char **argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);

    glutInitWindowPosition(CW_USEDEFAULT, CW_USEDEFAULT);
    glutInitWindowSize(CW_USEDEFAULT, CW_USEDEFAULT);

    int handle;
    BOOL bMaximized = g_bMaximized;
    {
        handle = glutCreateWindow("Saimin");

        g_hwnd = GetActiveWindow();
        assert(g_hwnd != NULL);

        SetWindowText(g_hwnd, doLoadString(106));

        g_hInstance = (HINSTANCE)GetWindowLongPtr(g_hwnd, GWLP_HINSTANCE);
        assert(g_hInstance != NULL);

        g_hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(1));
        g_hIconSm = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(1), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            0
        );
        assert(g_hIcon);
        assert(g_hIconSm);
        SendMessage(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
        SendMessage(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSm);

        g_fnOldWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProc);
        assert(g_fnOldWndProc != NULL);

        glutDisplayFunc(displayCB);
        glutTimerFunc(50, timerCB, 0);

        glutReshapeFunc(reshapeCB);
        glutKeyboardFunc(keyboardCB);
        glutMouseFunc(mouseCB);
        glutMotionFunc(mouseMotionCB);
    }
    g_bMaximized = bMaximized;
    SendMessage(g_hwnd, WM_COMMAND, IDYES, 0);

    if (g_bMaximized)
        ShowWindow(g_hwnd, SW_MAXIMIZE);

    initGL();

    return handle;
}

void drawString(const char *str, int x, int y, float color[4], void *font)
{
    glPushAttrib(GL_LIGHTING_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);

    glColor4fv(color);
    glRasterPos2i(x, y);

    while(*str)
    {
        glutBitmapCharacter(font, *str);
        ++str;
    }

    glEnable(GL_LIGHTING);
    glPopAttrib();
}

void drawString3D(const char *str, float pos[3], float color[4], void *font)
{
    glPushAttrib(GL_LIGHTING_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);

    glColor4fv(color);
    glRasterPos3fv(pos);

    while(*str)
    {
        glutBitmapCharacter(font, *str);
        ++str;
    }

    glEnable(GL_LIGHTING);
    glPopAttrib();
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

void atexit_function(void)
{
    micEchoOff();
    micOff();

    // stop kirakira.mp3
    mciSendF(TEXT("stop file%d"), 0);
    mciSendF(TEXT("close file%d"), 0);

    if (g_hMicThread)
    {
        micEnd(g_hMicThread);
        g_hMicThread = NULL;
    }

    destroyControls(g_hwnd);

    if (g_pWinVoice)
    {
        WinVoice *pWinVoice = g_pWinVoice;
        g_pWinVoice = NULL;
        delete pWinVoice;
    }

    if (g_bCoInit)
    {
        CoUninitialize();
        g_bCoInit = FALSE;
    }
}

INT ShowInputBox(HINSTANCE hInstance, INT myargc, LPWSTR *myargv);

extern "C"
INT WINAPI
_tWinMain(HINSTANCE   hInstance,
          HINSTANCE   hPrevInstance,
          LPTSTR      lpCmdLine,
          INT         nCmdShow)
{
    InitCommonControls();

    loadSettings();

    if (lpCmdLine && lpCmdLine[0])
    {
        INT myargc;
        LPWSTR *myargv = CommandLineToArgvW(GetCommandLineW(), &myargc);
        INT ret = ShowInputBox(hInstance, myargc, myargv);
        LocalFree(myargv);
        return ret;
    }

    g_hMicThread = micStart(FALSE);
    if (g_hMicThread)
    {
        micVolume(2.0);
    }

    if (!doAdultCheck())
    {
        saveSettings();
        return -1;
    }
    saveSettings();

    g_bCoInit = SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));

    g_pWinVoice = new WinVoice();
    if (!g_pWinVoice->IsAvailable())
    {
        delete g_pWinVoice;
        g_pWinVoice = NULL;
    }

    g_bPerfCounter = QueryPerformanceFrequency(&g_freq);

    initGLUT(0, NULL);

    atexit(atexit_function);

    glutMainLoop();

    if (g_bCoInit)
    {
        CoUninitialize();
        g_bCoInit = FALSE;
    }

    saveSettings();

    return 0;
}
