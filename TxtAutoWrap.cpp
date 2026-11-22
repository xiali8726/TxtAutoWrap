// TxtAutoWrap - Notepad++ 插件
// Copyright (C) 2026 xiali8726
// Licensed under the GNU General Public License v3.0 - see LICENSE.
// 功能：当前显示的文件是 .txt 时自动开启"显示自动换行"(仅影响显示，不改文件内容)；
//       其他文件保持 Notepad++ 全局"视图 > 自动换行"设置不变。
//
// 只依赖 Windows SDK，不需要 Notepad++ 插件模板头文件。

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shlwapi.h>
#include "resource.h"

// ---------------- Notepad++ 插件接口（精简版定义） ----------------
struct NppData {
    HWND _nppHandle;
    HWND _scintillaMainHandle;
    HWND _scintillaSecondHandle;
};

typedef void(__cdecl *PFUNCPLUGINCMD)();

struct ShortcutKey {
    bool _isCtrl;
    bool _isAlt;
    bool _isShift;
    UCHAR _key;
};

struct FuncItem {
    wchar_t _itemName[64];
    PFUNCPLUGINCMD _pFunc;
    int _cmdID;
    bool _init2Check;
    ShortcutKey *_pShKey;
};

// SCNotification 的首个成员就是通知头，这里只用到它
struct SciNotifyHeader {
    void *hwndFrom;
    UINT_PTR idFrom;
    unsigned int code;
};
struct SCNotification {
    SciNotifyHeader nmhdr;
};

#define NPPMSG                    (WM_USER + 1000)
#define NPPM_GETCURRENTSCINTILLA  (NPPMSG + 4)
#define NPPM_GETCURRENTDOCINDEX   (NPPMSG + 23)
#define NPPM_GETMENUHANDLE        (NPPMSG + 25)
#define NPPM_GETFULLPATHFROMBUFFERID (NPPMSG + 58)
#define NPPM_GETBUFFERIDFROMPOS   (NPPMSG + 59)
#define NPPM_GETCURRENTBUFFERID   (NPPMSG + 60)
#define NPPM_SETMENUITEMCHECK     (NPPMSG + 40)
#define NPPM_GETPLUGINSCONFIGDIR  (NPPMSG + 46)
#define NPPMAINMENU 1
#define MAIN_VIEW 0
#define SUB_VIEW  1

#define NPPN_FIRST            1000
#define NPPN_READY            (NPPN_FIRST + 1)
#define NPPN_FILEOPENED       (NPPN_FIRST + 4)
#define NPPN_FILESAVED        (NPPN_FIRST + 8)
#define NPPN_BUFFERACTIVATED  (NPPN_FIRST + 10)
#define NPPN_FILERENAMED      (NPPN_FIRST + 23)

#define IDM_VIEW_WRAP (40000 + 4000 + 22)   // 视图 > 自动换行

#define SCI_SETWRAPMODE 2268
#define SCI_GETWRAPMODE 2269
#define SC_WRAP_NONE 0
#define SC_WRAP_WORD 1

// ---------------- 插件实现 ----------------
static const wchar_t PLUGIN_NAME[] = L"TxtAutoWrap";
static NppData g_npp;
static bool g_enabled = true;
static bool g_ready = false;

static HINSTANCE g_hInst = nullptr;
static wchar_t g_iniPath[MAX_PATH] = L"";

static void toggleEnabled();
static void showHelp();
static void showAbout();

static FuncItem g_funcs[] = {
    { L"对 .txt 文件自动换行", toggleEnabled, 0, true,  nullptr },
    { L"-",                   nullptr,       0, false, nullptr },   // 分隔线
    { L"帮助",                showHelp,      0, false, nullptr },
    { L"关于",                showAbout,     0, false, nullptr },
};
static const int FUNC_COUNT = sizeof(g_funcs) / sizeof(g_funcs[0]);

// 用户在"视图"菜单里设置的全局自动换行状态
static bool globalWrapChecked() {
    HMENU hMenu = (HMENU)::SendMessage(g_npp._nppHandle, NPPM_GETMENUHANDLE, NPPMAINMENU, 0);
    if (!hMenu) return false;
    UINT st = ::GetMenuState(hMenu, IDM_VIEW_WRAP, MF_BYCOMMAND);
    return st != (UINT)-1 && (st & MF_CHECKED);
}

static bool isTxtBuffer(UINT_PTR bufferId) {
    if (!bufferId) return false;
    int len = (int)::SendMessage(g_npp._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferId, 0);
    if (len <= 0) return false;
    wchar_t *path = new wchar_t[len + 1];
    ::SendMessage(g_npp._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferId, (LPARAM)path);
    bool r = ::lstrcmpiW(::PathFindExtensionW(path), L".txt") == 0;
    delete[] path;
    return r;
}

static void applyToView(HWND hSci, UINT_PTR bufferId) {
    if (!hSci || !bufferId) return;
    int want = (g_enabled && isTxtBuffer(bufferId)) || globalWrapChecked() ? SC_WRAP_WORD : SC_WRAP_NONE;
    if ((int)::SendMessage(hSci, SCI_GETWRAPMODE, 0, 0) != want)
        ::SendMessage(hSci, SCI_SETWRAPMODE, want, 0);
}

static UINT_PTR bufferInView(int view) {
    int idx = (int)::SendMessage(g_npp._nppHandle, NPPM_GETCURRENTDOCINDEX, 0, view);
    if (idx < 0) return 0;
    return (UINT_PTR)::SendMessage(g_npp._nppHandle, NPPM_GETBUFFERIDFROMPOS, idx, view);
}

static void applyToBothViews() {
    applyToView(g_npp._scintillaMainHandle, bufferInView(MAIN_VIEW));
    applyToView(g_npp._scintillaSecondHandle, bufferInView(SUB_VIEW));
}

static void applyToCurrentView() {
    int which = -1;
    ::SendMessage(g_npp._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    HWND hSci = (which == 1) ? g_npp._scintillaSecondHandle : g_npp._scintillaMainHandle;
    UINT_PTR buf = (UINT_PTR)::SendMessage(g_npp._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);
    applyToView(hSci, buf);
}

// ---- 设置持久化：<Notepad++ 插件配置目录>\TxtAutoWrap.ini ----
static void loadSettings() {
    wchar_t dir[MAX_PATH] = L"";
    ::SendMessage(g_npp._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)dir);
    if (!dir[0]) return;
    ::CreateDirectoryW(dir, nullptr);
    ::PathCombineW(g_iniPath, dir, L"TxtAutoWrap.ini");
    g_enabled = ::GetPrivateProfileIntW(L"Settings", L"Enabled", 1, g_iniPath) != 0;
}

static void saveSettings() {
    if (g_iniPath[0])
        ::WritePrivateProfileStringW(L"Settings", L"Enabled", g_enabled ? L"1" : L"0", g_iniPath);
}

static void toggleEnabled() {
    g_enabled = !g_enabled;
    ::SendMessage(g_npp._nppHandle, NPPM_SETMENUITEMCHECK, g_funcs[0]._cmdID, g_enabled);
    saveSettings();
    applyToBothViews();
}

static void showHelp() {
    ::MessageBoxW(g_npp._nppHandle,
        L"功能：\n"
        L"打开/切换到 .txt 文件时自动启用显示换行（不修改文件内容）。\n"
        L"其他类型文件沿用“视图 > 自动换行”的全局设置。\n"
        L"可通过菜单“对 .txt 文件自动换行”开启或关闭，设置会自动保存。\n\n"
        L"注意事项：\n"
        L"1. 按扩展名判断（不区分大小写），未保存的“新文件”不会自动换行。\n"
        L"2. “另存为”或重命名改变扩展名后会重新判断。\n"
        L"3. 分屏时，若在非 txt 文件上手动切换了“视图 > 自动换行”，\n"
        L"    另一侧的 txt 可能暂时不换行，点一下该侧窗口即可恢复。",
        L"TxtAutoWrap 帮助", MB_OK | MB_ICONINFORMATION);
}

static INT_PTR CALLBACK aboutProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        // 居中于 Notepad++ 主窗口
        RECT rp, rd;
        ::GetWindowRect(g_npp._nppHandle, &rp);
        ::GetWindowRect(hDlg, &rd);
        int w = rd.right - rd.left, h = rd.bottom - rd.top;
        ::SetWindowPos(hDlg, nullptr, rp.left + ((rp.right - rp.left) - w) / 2,
                       rp.top + ((rp.bottom - rp.top) - h) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            ::EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void showAbout() {
    ::DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_ABOUT), g_npp._nppHandle, aboutProc, 0);
}

// ---------------- 导出函数 ----------------
extern "C" __declspec(dllexport) void setInfo(NppData data) {
    g_npp = data;
    loadSettings();
    g_funcs[0]._init2Check = g_enabled;
}

extern "C" __declspec(dllexport) const wchar_t *getName() { return PLUGIN_NAME; }

extern "C" __declspec(dllexport) FuncItem *getFuncsArray(int *n) {
    *n = FUNC_COUNT;
    return g_funcs;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *n) {
    if (!n || n->nmhdr.hwndFrom != g_npp._nppHandle) return;
    switch (n->nmhdr.code) {
    case NPPN_READY:
        g_ready = true;
        applyToBothViews();
        break;
    case NPPN_BUFFERACTIVATED:
    case NPPN_FILEOPENED:
    case NPPN_FILESAVED:     // "另存为"可能改变扩展名
    case NPPN_FILERENAMED:
        if (g_ready) applyToCurrentView();
        break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }

extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }

BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) g_hInst = hInst;
    return TRUE;
}
