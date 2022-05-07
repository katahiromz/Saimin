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

typedef std::complex<double> dcomp_t;
typedef std::complex<float> fcomp_t;

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

#define TYPE_COUNT 7
#define TIMER_ID 999

HINSTANCE g_hInstance = NULL;
HWND g_hwnd = NULL;
HDC g_hDC = NULL;
HGLRC g_hGLRC = 0;
DWORD g_dwCount = 0;
float g_eCount = 0;
DWORD g_dwGriGri = 0;
HWND g_hCmb1 = NULL;
HWND g_hCmb2 = NULL;
HWND g_hChx1 = NULL;
HWND g_hCmb3 = NULL;
HWND g_hPsh1 = NULL;
HWND g_hStc1 = NULL;
INT g_cxStc1 = 100;
INT g_cyStc1 = 100;
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
INT g_nSpeed = 8;
BOOL g_bColor = TRUE;
BOOL g_bMaximized = FALSE;
HFONT g_hFont = NULL;
HBITMAP g_hbm = NULL;


#ifndef _countof
    #define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

inline void rectangle(float x0, float y0, float x1, float y1)
{
    glBegin(GL_POLYGON);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();
}

inline void circle(float x, float y, float r, INT N = 10, BOOL bFill = TRUE)
{
    if (bFill)
        glBegin(GL_POLYGON);
    else
        glBegin(GL_LINE_LOOP);
    for (int i = 0; i < N; i++)
    {
        fcomp_t comp = std::polar(r, float(2 * M_PI) * i / N);
        glVertex2f(x + comp.real(), y + comp.imag());
    }
    glEnd();
}

void line(float x0, float y0, float x1, float y1, float width, INT N = 6)
{
    circle(x0, y0, width, N);
    circle(x1, y1, width, N);

    fcomp_t comp0(std::cos(M_PI / 2), std::sin(M_PI / 2));
    fcomp_t comp1(x1 - x0, y1 - y0);
    float abs = std::abs(comp1);
    if (abs == 0)
        return;

    fcomp_t comp2 = comp1 / abs;
    fcomp_t p0 = width * comp2 * comp0;
    fcomp_t p1 = width * comp2 / comp0;

    glBegin(GL_POLYGON);
    glVertex2f(x0 + p0.real(), y0 + p0.imag());
    glVertex2f(x0 + p1.real(), y0 + p1.imag());
    glVertex2f(x1 + p1.real(), y1 + p1.imag());
    glVertex2f(x1 + p0.real(), y1 + p0.imag());
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

    error = keyApp.QueryDword(TEXT("GriGri"), (DWORD&)g_dwGriGri);
    if (error)
        g_dwGriGri = 0;

    error = keyApp.QueryDword(TEXT("Dual"), (DWORD&)g_bDual);
    if (error)
        g_bDual = TRUE;

    error = keyApp.QueryDword(TEXT("Speed"), (DWORD&)g_nSpeed);
    if (error)
        g_nSpeed = 8;
    if (g_nSpeed < 3)
        g_nSpeed = 8;

    error = keyApp.QueryDword(TEXT("Color"), (DWORD&)g_bColor);
    if (error)
        g_bColor = TRUE;

    error = keyApp.QueryDword(TEXT("Maximized"), (DWORD&)g_bMaximized);
    if (error)
        g_bMaximized = FALSE;

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
    keyApp.SetDword(TEXT("GriGri"), g_dwGriGri);
    keyApp.SetDword(TEXT("Dual"), g_bDual);
    keyApp.SetDword(TEXT("Speed"), g_nSpeed);
    keyApp.SetDword(TEXT("Color"), g_bColor);
    keyApp.SetDword(TEXT("Maximized"), g_bMaximized);

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

inline float getCount(void)
{
    return g_eCount;
}

void drawType1(RECT& rc, BOOL bFlag)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;

    glViewport(px, py, cx, cy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + cx, py + cy, py, -1.0, 1.0);

    INT size = ((rc.right - rc.left) + (rc.bottom - rc.top)) * 2 / 5;
    float qx, qy;
    float count2 = getCount() * 0.65f;
    {
        fcomp_t comp = std::polar(float(g_dwGriGri), float(M_PI * 0.01f) * count2);
        qx = (rc.left + rc.right) / 2 + comp.real();
        qy = (rc.top + rc.bottom) / 2 + comp.imag();
    }

    float factor = (0.99 + fabs(sin(count2 * 0.2f)) * 0.01);

    INT dr0 = 15;
    float dr = dr0 / 2 * factor;
    INT flag2 = bFlag ? g_nDirection : -g_nDirection;
    INT ci = 6;
    for (INT i = 0; i <= ci; ++i)
    {
        INT count = 0;
        float x, y, oldx = qx, oldy = qy, f = 0.5f;
        for (float radius = 0; radius < size; radius += f)
        {
            float theta = dr0 * count * 0.375;
            float value = 0.3f * float(sin(count2 * 0.04f)) + 0.7f;
            if (g_bColor)
                glColor3f(1.0f, 1.0f, value);
            else
                glColor3f(1.0f, 1.0f, 1.0f);

            float radian = theta * float(M_PI / 180.0) + i * (2 * M_PI) / ci;
            fcomp_t comp = std::polar(radius, flag2 * float(radian - count2 * float(M_PI * 0.03f)));
            x = qx + comp.real();
            y = qy + comp.imag();
            if (oldx == MAXLONG && oldy == MAXLONG)
            {
                oldx = x;
                oldy = y;
            }
            line(oldx, oldy, x, y, dr * f * 0.333f, 5.0f * f);

            oldx = x;
            oldy = y;
            ++count;
            f *= 1.02;
        }
    }
}

void drawType2_0(RECT& rc, BOOL bFlag, BOOL bFlag2)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;

    glViewport(px, py, cx, cy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + cx, py + cy, py, -1.0, 1.0);
    glColor3f(0, 0, 0);
    rectangle(px, py, px + cx, py + cy);

    float size = (cx + cy) * 0.4;
    float qx, qy;
    float count2 = getCount() * 0.5f;
    {
        fcomp_t comp = std::polar(float(g_dwGriGri), float(M_PI * 0.01f) * count2);
        qx = (rc.left + rc.right) / 2 + comp.real();
        qy = (rc.top + rc.bottom) / 2 + comp.imag();
    }

    float factor = (0.99f + fabs(sin(count2 * 0.2f)) * 0.01f);

    INT dr0 = 20;
    float dr = dr0 / 2 * factor;
    float radius;
    INT flag2 = g_nDirection;
    if (!bFlag2)
    {
        if (flag2 == 1 ^ bFlag2)
            radius = INT(count2 * 2) % dr0;
        else
            radius = INT(dr0 - count2 * 2) % dr0;
    }
    else
    {
        if (flag2 == 1 ^ bFlag2)
            radius = INT(count2 * 2) % dr0;
        else
            radius = INT(dr0 - count2 * 2) % dr0;
    }

    GLfloat old_width;
    glGetFloatv(GL_LINE_WIDTH, &old_width);
    glLineWidth(5);

    GLboolean line_smooth;
    glGetBooleanv(GL_LINE_SMOOTH, &line_smooth);
    glEnable(GL_LINE_SMOOTH);

    float value = 0.4f * sin(count2 * 0.025) + 0.6f;
    if (g_bColor)
        glColor3f(1.0f, value, value);
    else
        glColor3f(1.0f, 1.0f, 1.0f);

    for (; radius < size; radius += dr0)
    {
        float N = radius / 3.5f;
        if (N < 18)
            N = 18;
        circle(qx, qy, radius, N, FALSE);
        circle(qx, qy, radius + 2, N, FALSE);
    }

    glLineWidth(old_width);

    if (line_smooth)
        glEnable(GL_LINE_SMOOTH);
    else
        glDisable(GL_LINE_SMOOTH);
}

void drawType2(RECT& rc, BOOL bFlag)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;
    INT cxy = min(cx, cy);

    INT centerx = px + cx / 2;
    INT centery = py + cy / 2;

    RECT rc0, rc1;
    SetRect(&rc0, px, py, px + cx, py + cy);
    INT size = cxy / 5 + cxy * fabs(2 - sin(g_eCount * 0.01f)) / 16;
    SetRect(&rc1, centerx - size, centery - size, centerx + size, centery + size);

    drawType2_0(rc0, bFlag, FALSE);
    drawType2_0(rc1, bFlag, TRUE);
}

void hsv2rgb(float h, float s, float v, float& r, float& g, float& b)
{
    r = g = b = v;
    if (s > 0)
    {
        h *= 6;
        int i = (int)h;
        float f = h - (float)i;
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

void drawType3(RECT& rc, BOOL bFlag)
{
    INT px = rc.left, py = rc.top;
    INT dx = rc.right - rc.left, dy = rc.bottom - rc.top;
	INT qx = px + dx / 2;
	INT qy = py + dy / 2;
	INT dxy = (dx + dy) / 2;

    glViewport(px, py, dx, dy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + dx, py + dy, py, -1.0, 1.0);

    float count2 = getCount() * 0.5f;
	float factor = count2 * 0.16;

	qx += 30 * std::cos(factor * 0.8);
	qy += 30 * std::sin(factor * 0.8);

	for (float radius = (dx + dy) * 0.4f; radius >= 10; radius *= 0.92)
	{
		float h;
		if (bFlag)
			h = std::fmod(dxy + factor * 0.3f + radius * 0.015f, 1.0f);
		else
			h = std::fmod(dxy + factor * 0.3f - radius * 0.015f, 1.0f);
		float r0, g0, b0;
		hsv2rgb(h, 1.0f, 1.0f, r0, g0, b0);
        glColor3f(r0, g0, b0);

		INT N0 = 40, N1 = 5;
		INT i = 0;
		for (float angle = 0; angle <= 360; angle += 360 / N0)
		{
			float radian = (angle + count2 * 2) * (M_PI / 180.0f);
			float factor2 = radius * (1 + 0.7f * std::fabs(std::sin(N1 * i * M_PI / N0)));
			float x = qx + factor2 * std::cos(radian);
			float y = qy + factor2 * std::sin(radian);
			if (angle == 0)
			{
			    glBegin(GL_POLYGON);
			}
		    glVertex2f(x, y);
		    ++i;
		}
	    glEnd();
	}
}

void drawType4(RECT& rc, BOOL bFlag)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;

    glViewport(px, py, cx, cy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + cx, py + cy, py, -1.0, 1.0);

    float size = ((rc.right - rc.left) + (rc.bottom - rc.top)) * 0.4f;
    float qx, qy;
    float count2 = getCount() * 0.5f;
    {
        fcomp_t comp = std::polar(float(g_dwGriGri), g_eCount * float(M_PI * 0.01f));
        qx = (rc.left + rc.right) / 2 + comp.real();
        qy = (rc.top + rc.bottom) / 2 + comp.imag();
    }

    float factor = (0.99 + fabs(sin(count2 * 0.2f)) * 0.01f);

    INT dr0 = 20;
    float dr = dr0 / 4 * factor;
    INT flag2 = g_nDirection * (bFlag ? -1 : 1);
    INT ci = 10;
    for (INT i = 0; i < ci; ++i)
    {
        INT count = 0;
        float oldx = MAXLONG, oldy = MAXLONG;
        for (float radius = -size; radius < 0; radius += 16.0f)
        {
            float theta = count * dr0 * 0.4f;
            float value = 0.6f + 0.4f * sin(count2 * 0.1f + count * 0.1f);
            if (g_bColor)
                glColor3f(value, 1.0f, 1.0f);
            else
                glColor3f(1.0f, 1.0f, 1.0f);

            float radian = theta * float(M_PI / 90) + i * float(2 * M_PI) / ci;
            fcomp_t comp = std::polar(radius, flag2 * (-radian + count2 * float(M_PI * 0.03f)));
            float x = qx + comp.real() * (2 + sin(count2 * 0.04f));
            float y = qy + comp.imag() * (2 + sin(count2 * 0.04f));
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
        for (float radius = 0; radius < size; radius += 16.0f)
        {
            float theta = count * dr0 * 0.4f;
            float value = 0.6f + 0.4f * sin(count2 * 0.1f + count * 0.1f);
            if (g_bColor)
                glColor3f(value, 1.0f, 1.0f);
            else
                glColor3f(1.0f, 1.0f, 1.0f);

            float radian = theta * (-flag2 * M_PI / 90.0f) + i * float(2 * M_PI) / ci;
            fcomp_t comp = std::polar(radius, radian + flag2 * float(M_PI * 0.03f) * count2);
            float x = qx + comp.real() * (2 + sin(count2 * 0.02f));
            float y = qy + comp.imag() * (2 + sin(count2 * 0.02f));
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

    if (g_bColor)
        glColor3f(1.0f, 0.4f, 0.4f);
    else
        glColor3f(1.0f, 1.0f, 1.0f);
    circle(qx, qy, dr * 2);
    if (g_bColor)
        glColor3f(1.0f, 0.6f, 0.6f);
    else
        glColor3f(1.0f, 1.0f, 1.0f);
    circle(qx, qy, dr);
}

void drawType5_0(RECT& rc, BOOL bFlag, float size)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;
    INT qx = px + cx / 2, qy = py + cy / 2;

    glViewport(px, py, cx, cy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(px, px + cx, py + cy, py, -1.0, 1.0);

    float eCount = getCount();
    float factor = eCount * 0.02f;

    float count2 = getCount() * 0.5;
    {
        fcomp_t comp = std::polar(float(g_dwGriGri) * 0.7f, float(M_PI * 0.012f) * g_eCount);
        qx += comp.real();
        qy += comp.imag();
    }

    INT nCount2 = 0;
    float cxy = (cx + cy) * 0.35f;
    float xy0 = (cxy + size) - std::fmod((cxy + size), size);
    for (float y = -xy0; y < cxy + size; y += size)
    {
        INT nCount = nCount2 % 3;
        for (float x = -xy0; x < cxy + size; x += size)
        {
            float h;
            float s = 1.0f;
            float v = 1.0f;
            float r, g, b;
            if (g_bColor)
            {
                switch (nCount % 3)
                {
                case 0:
                    h = std::fmod(0.2f + factor * 1.2, 1.0f);
                    break;
                case 1:
                    h = std::fmod(0.4f + factor * 1.2, 1.0f);
                    break;
                case 2:
                    h = std::fmod(0.8f + factor * 1.2, 1.0f);
                    break;
                }
                hsv2rgb(h, s, v, r, g, b);

                if (r < 0.2f)
                    r = 0.2f;
                if (g < 0.2f)
                    g = 0.2f;
                if (b < 0.2f)
                    b = 0.2f;
                glColor3f(r, g, b);
            }
            else
            {
                if ((nCount + (g_dwCount * g_nSpeed / 40)) % 3 == 0)
                    glColor3f(1.0f, 1.0f, 1.0f);
                else
                    glColor3f(0.0f, 0.0f, 0.0f);
            }

            fcomp_t comp, comp0;
            if (bFlag ^ (g_nDirection < 0))
                comp0 = std::polar(1.0f, factor * 0.65f);
            else
                comp0 = std::polar(1.0f, (1.0f - factor) * 0.65f);

            float x0 = x - size / 2;
            float y0 = y - size / 2;
            float x1 = x0 + size;
            float y1 = y0 + size;
            glBegin(GL_POLYGON);
            comp = fcomp_t(x0, y0);
            comp *= comp0;
            glVertex2f(qx + comp.real(), qy + comp.imag());
            comp = fcomp_t(x0, y1);
            comp *= comp0;
            glVertex2f(qx + comp.real(), qy + comp.imag());
            comp = fcomp_t(x1, y1);
            comp *= comp0;
            glVertex2f(qx + comp.real(), qy + comp.imag());
            comp = fcomp_t(x1, y0);
            comp *= comp0;
            glVertex2f(qx + comp.real(), qy + comp.imag());
            glEnd();

            if (x >= 0)
                nCount = (nCount + 2) % 3;
            else
                ++nCount;
        }
        if (y >= 0)
            nCount2 = (nCount2 + 2) % 3;
        else
            ++nCount2;
    }

    GLboolean line_smooth;
    glGetBooleanv(GL_LINE_SMOOTH, &line_smooth);
    glEnable(GL_LINE_SMOOTH);

    GLfloat old_width;
    glGetFloatv(GL_LINE_WIDTH, &old_width);
    glLineWidth(5);

    float r;
    if (g_nDirection < 0)
        r = std::fmod(eCount, 100);
    else
        r = 100 - std::fmod(eCount, 100);

    if (!g_bColor)
        glColor3f(1.0f, 1.0f, 1.0f);
    else if (size == 14)
        glColor3f(1.0f, 0.2f, 0.2f);
    else
        glColor3f(0.2f, 0.2f, 1.0f);

    for (; r < cxy; r += 100)
    {
        INT N = INT(r * 0.0667f);
        if (N < 12)
            N = 12;

        circle(qx, qy, r, N, FALSE);
        circle(qx, qy, r + 2, N, FALSE);
    }

    glLineWidth(old_width);

    if (line_smooth)
        glEnable(GL_LINE_SMOOTH);
    else
        glDisable(GL_LINE_SMOOTH);
}

void drawType5(RECT& rc, BOOL bFlag)
{
    INT px = rc.left, py = rc.top;
    INT cx = rc.right - rc.left, cy = rc.bottom - rc.top;

    INT centerx = px + cx / 2;
    INT centery = py + cy / 2;

    RECT rc0, rc1;
    SetRect(&rc0, px, py, px + cx, py + cy);
    INT size = (cx + cy) / 8;
    SetRect(&rc1, centerx - size, centery - size, centerx + size, centery + size);

    drawType5_0(rc0, bFlag, 16);
    drawType5_0(rc1, !bFlag, 14);
}

void updateRandom(INT nOldType)
{
    INT nNewType;
    do
    {
        nNewType = rand() % (TYPE_COUNT - 2) + 1;
    } while (nNewType == nOldType || nNewType == 0 || nNewType == TYPE_COUNT - 1);

    g_nRandomType = nNewType;
    g_bDual = (rand() % 3) < 2;
}

void drawRandom(RECT& rc, BOOL bFlag)
{
    if ((g_dwCount % 500) == 0)
        updateRandom(g_nRandomType);

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
    case 5:
        drawType5(rc, bFlag);
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
    case 5:
        drawType5(rc, bFlag);
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
        SelectObject(hdc, g_hFont);
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
                    SelectObject(hdcMem, g_hFont);
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
    ComboBox_AddString(g_hCmb1, TEXT("Type 5"));
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

    // chx1: Color checkbox
    style = BS_AUTOCHECKBOX | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hChx1 = CreateWindowEx(exstyle, TEXT("BUTTON"), TEXT("Color"), style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)chx1, g_hInstance, NULL);
    if (!g_hChx1)
        return FALSE;
    SetWindowFont(g_hChx1, GetStockFont(DEFAULT_GUI_FONT), TRUE);
    if (g_bColor)
        CheckDlgButton(hwnd, chx1, BST_CHECKED);
    else
        CheckDlgButton(hwnd, chx1, BST_UNCHECKED);

    // cmb3: Messages ComboBox
    style = CBS_HASSTRINGS | CBS_AUTOHSCROLL | CBS_DROPDOWN | WS_CHILD | WS_VISIBLE;
    exstyle = 0;
    g_hCmb3 = CreateWindowEx(exstyle, TEXT("COMBOBOX"), NULL, style, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)cmb3, g_hInstance, NULL);
    if (!g_hCmb3)
        return FALSE;
    SetWindowFont(g_hCmb3, GetStockFont(DEFAULT_GUI_FONT), TRUE);
    ComboBox_AddString(g_hCmb3, TEXT(""));
    for (INT i = 200; i <= 209; ++i)
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

    {
        HFONT hFont = GetStockFont(DEFAULT_GUI_FONT);
        LOGFONT lf;
        GetObject(hFont, sizeof(lf), &lf);
        lf.lfHeight = 32;
        g_hFont = CreateFontIndirect(&lf);
        SetWindowFont(g_hStc1, g_hFont, TRUE);
    }

    SetWindowText(g_hStc1, g_strText.c_str());
    stc1_CalcSize(g_hStc1);

    PostMessage(hwnd, WM_SIZE, 0, 0);
    return TRUE;
}

void OnTimer(HWND hwnd, UINT id)
{
    if (id == TIMER_ID)
    {
        InvalidateRect(hwnd, NULL, TRUE);
        g_dwCount++;
        g_eCount += g_nSpeed * 0.25;
    }
}

DWORD WINAPI threadProc(LPVOID)
{
    while (g_hThread)
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
            INT offset = INT((rc.bottom - rc.top) * (sin(g_dwCount * 0.05) * 0.2 + 1) / 4);
            OffsetRect(&rc2, 0, offset);
            MoveWindow(g_hStc1, rc2.left, rc2.top, g_cxStc1, g_cyStc1, FALSE);
            ShowWindowAsync(g_hStc1, SW_SHOWNOACTIVATE);
            InvalidateRect(g_hStc1, NULL, TRUE);
        }

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
    DeleteObject(g_hFont);
    DeleteObject(g_hbm);
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
    MoveWindow(g_hChx1, rc.left + 250, rc.bottom - 24, 60, 20, TRUE);
    MoveWindow(g_hCmb3, rc.right - 150 - 40, rc.bottom - 24, 150, nHeight * 10, TRUE);
    MoveWindow(g_hPsh1, rc.right - 40, rc.bottom - 24, 40, 24, TRUE);

    g_bMaximized = IsZoomed(hwnd);
}

inline void OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    g_bDual = !g_bDual;
}

void setType(INT nType)
{
    INT nOldType = g_nType;
    g_nType = nType;

    if (g_nType == TYPE_COUNT - 1)
        updateRandom(nOldType);

    if (ComboBox_GetCurSel(g_hCmb1) != g_nType)
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
    case chx1:
        if (IsDlgButtonChecked(hwnd, chx1) == BST_CHECKED)
            g_bColor = TRUE;
        else
            g_bColor = FALSE;
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
            stc1_CalcSize(g_hStc1);
            if (g_hbm)
            {
                DeleteObject(g_hbm);
                g_hbm = NULL;
            }
            InvalidateRect(g_hStc1, NULL, TRUE);
        }
        break;
    }
}

void OnMButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    if (GetAsyncKeyState(VK_SHIFT) < 0)
        ;
    else if (GetAsyncKeyState(VK_CONTROL) < 0)
        ;
    else
        g_nDirection = -g_nDirection;
}

int OnMouseWheel(HWND hwnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
    if (GetAsyncKeyState(VK_CONTROL) < 0)
    {
        if (zDelta < 0)
        {
            if (g_dwGriGri <= 5)
                g_dwGriGri = 0;
            else if (g_dwGriGri < 10)
                g_dwGriGri -= 3;
            else
                g_dwGriGri = g_dwGriGri * 10 / 12;
        }
        else
        {
            if (g_dwGriGri <= 10)
                g_dwGriGri += 5;
            else if (g_dwGriGri > 50)
                g_dwGriGri = 50;
            else
                g_dwGriGri = g_dwGriGri * 12 / 10;
        }
    }
    else
    {
        if (zDelta < 0)
        {
            if (g_nSpeed > 3)
                g_nSpeed = g_nSpeed - 1;
        }
        else
        {
            if (g_nSpeed < 13)
                g_nSpeed = g_nSpeed + 1;
        }
    }
    return 0;
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
        HANDLE_MSG(hwnd, WM_MOUSEWHEEL, OnMouseWheel);
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

    srand(GetTickCount());

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

    if (g_bMaximized)
        ShowWindow(g_hwnd, SW_SHOWMAXIMIZED);
    else
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
