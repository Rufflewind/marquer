#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#ifndef OEMRESOURCE
#define OEMRESOURCE
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <gdiplus.h>
#include "resource.h"

enum {
    IDM_COORDINATES = 0x9000,
    IDM_PASTECOORDS = 0x9010,
    IDM_PASTETITLE = 0x9020,
    IDM_LOCKMARKER = 0x9030,
    IDM_SETPOSITION = 0x9040,
    IDM_SETTITLE = 0x9050,
    IDM_NUDGELEFT = 0x9060,
    IDM_NUDGERIGHT = 0x9070,
    IDM_NUDGEUP = 0x9080,
    IDM_NUDGEDOWN = 0x9090
};

static const size_t COORDINATES_BUFFER_LENGTH = 256;

namespace strs {

static wchar_t PROGRAM[] =
    L"marquer";

static wchar_t PROGRAM_NAME[] =
    L"Screen Marker";

static wchar_t IMAGE_FILENAME[] =
    L"marquer.png";

static wchar_t COORDS_FORMAT[] =
    L"(%ld, %ld)";

static wchar_t MENU_COORDINATES[] =
    L"C&opy Coordinates - (%ld, %ld)\tCtrl+C";

static wchar_t MENU_PASTECOORDS[] =
    L"&Paste Coordinates\tCtrl+V";

static wchar_t MENU_PASTECOORDS_FORMAT[] =
    L"&Paste Coordinates - (%ld, %ld)\tCtrl+V";

static wchar_t MENU_PASTETITLE[] =
    L"Paste &Title\tCtrl+Shift+V";

static wchar_t MENU_LOCKMARKER[] =
    L"&Lock Marker\tCtrl+L";

static wchar_t ERR_IMAGE_M[] =
    L"Failed to load 'marquer.png'.  Make sure the file is in the working "
    L"directory.";

static wchar_t ERR_IMAGE_T[] =
    L"Image failed to load.";

}

struct {
    HINSTANCE hinstance;
    HWND hwnd;
    HMENU hmenu;
    HCURSOR hcursor_default;
    HCURSOR hcursor_move;
    bool lock;
} window;

// Obtains the string containing the coordinates of the given window and
// stores it into the given `out` buffer.  Note that `out_len` is the number
// of wide characters, NOT the size in bytes, and must be nonzero.  On
// failure, `false` is returned.
bool get_coordinates_str(wchar_t *out, size_t out_len, HWND hwnd,
                         const wchar_t *format = strs::COORDS_FORMAT) {
    WINDOWPLACEMENT p;
    return
        GetWindowPlacement(hwnd, &p) &&
        swprintf(
            out, out_len, format,
            (p.rcNormalPosition.left + p.rcNormalPosition.right) / 2,
            (p.rcNormalPosition.top + p.rcNormalPosition.bottom) / 2) >= 0;
}

// Copies the coordinates of the window onto the clipboard.  On failure,
// `false` is returned.
bool copy_coordinates(HWND hwnd) {
    if (!OpenClipboard(hwnd)) goto err0;
    if (!EmptyClipboard()) goto err1;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE,
                            COORDINATES_BUFFER_LENGTH * sizeof(wchar_t));
    if (!h) goto err1;
    wchar_t *b = static_cast<wchar_t *>(GlobalLock(h));
    if (!b) goto err2;
    if (!get_coordinates_str(b, COORDINATES_BUFFER_LENGTH, hwnd)) goto err3;
    if (!GlobalUnlock(h) && GetLastError() != NO_ERROR) goto err2;
    if (!SetClipboardData(CF_UNICODETEXT, h)) goto err2;
    CloseClipboard();
    return true;
 err3: GlobalUnlock(h);
 err2: GlobalFree(h);
 err1: CloseClipboard();
 err0: return false;
}

bool parse_coordinates(long *x, long *y, const wchar_t *str) {
    int state = 0;
    std::wstring s[2];
    // Regexp implemented as a finite-state machine:
    //     (-?#+).!(-?#+)
    // where:
    //     ( )   grouping
    //     ?     quantifier: 0 or 1
    //     #     numeric digit (0-9)
    //     +     quantifier: 1 or more
    //     .     any character
    //     !     lazy quantifier: any
    for (const wchar_t *c = str; *c != L'\0'; ++c) {
        switch (state) {
        case 0:
            if (iswdigit(*c)) {
                state = 2;
                s[0] += *c;
            } else if (*c == L'-') {
                state = 1;
                s[0] += *c;
            }
            break;
        case 1:
            if (iswdigit(*c)) {
                state = 2;
                s[0] += *c;
            } else if (*c == L'-') {
            } else {
                state = 0;
                s[0].clear();
            }
            break;
        case 2:
            if (iswdigit(*c)) {
                s[0] += *c;
            } else if (*c == L'-') {
                state = 4;
                s[1] += *c;
            } else {
                state = 3;
            }
            break;
        case 3:
            if (iswdigit(*c)) {
                state = 5;
                s[1] += *c;
            } else if (*c == L'-') {
                state = 4;
                s[1] += *c;
            }
            break;
        case 4:
            if (iswdigit(*c)) {
                state = 5;
                s[1] += *c;
            } else if (*c == L'-') {
            } else {
                state = 3;
                s[1].clear();
            }
            break;
        case 5:
            if (iswdigit(*c))
                s[1] += *c;
            else
                goto done;
        }
    }
 done:
    return
        swscanf(s[0].c_str(), L"%ld", x) == 1 &&
        swscanf(s[1].c_str(), L"%ld", y) == 1;
}

bool set_coordinates(HWND hwnd, const wchar_t *str) {
    WINDOWPLACEMENT p;
    if (!GetWindowPlacement(hwnd, &p))
        return false;

    p.rcNormalPosition.right -= p.rcNormalPosition.left;
    p.rcNormalPosition.bottom -= p.rcNormalPosition.top;

    if (!parse_coordinates(&p.rcNormalPosition.left,
                           &p.rcNormalPosition.top, str))
        return false;

    // Compensate for negative integer division asymmetry
    if (p.rcNormalPosition.left < 0) --p.rcNormalPosition.left;
    if (p.rcNormalPosition.top < 0) --p.rcNormalPosition.top;

    p.rcNormalPosition.left -= p.rcNormalPosition.right / 2;
    p.rcNormalPosition.top -= p.rcNormalPosition.bottom / 2;
    p.rcNormalPosition.right += p.rcNormalPosition.left;
    p.rcNormalPosition.bottom += p.rcNormalPosition.top;

    return SetWindowPlacement(hwnd, &p) != FALSE;
}

// Check if the clipboard data is a valid coordinate
bool get_clipboard_coordinates(long *x, long *y, HWND hwnd) {
    if (!OpenClipboard(hwnd)) goto err0;
    HANDLE hstr = GetClipboardData(CF_UNICODETEXT);
    if (!hstr) goto err1;
    wchar_t *str = static_cast<wchar_t *>(GlobalLock(hstr));
    if (!str) goto err1;
    if (!parse_coordinates(x, y, str)) goto err2;
    GlobalUnlock(hstr);
    CloseClipboard();
    return true;
 err2: GlobalUnlock(hstr);
 err1: CloseClipboard();
 err0: return false;
}

bool paste_coordinates(HWND hwnd) {
    if (!OpenClipboard(hwnd)) goto err0;
    HANDLE hstr = GetClipboardData(CF_UNICODETEXT);
    if (!hstr) goto err1;
    wchar_t *str = static_cast<wchar_t *>(GlobalLock(hstr));
    if (!str) goto err1;
    if (!set_coordinates(hwnd, str)) goto err2;
    GlobalUnlock(hstr);
    CloseClipboard();
    return true;
 err2: GlobalUnlock(hstr);
 err1: CloseClipboard();
 err0: return false;
}

// Paste the title from the clipboard.  On failure, `false` is returned.
bool paste_title(HWND hwnd) {
    if (!OpenClipboard(hwnd)) goto err0;
    HANDLE hstr = GetClipboardData(CF_UNICODETEXT);
    if (!hstr) goto err1;
    wchar_t *str = static_cast<wchar_t *>(GlobalLock(hstr));
    if (!str) goto err1;
    if (SetWindowText(hwnd, str)) goto err2;
    GlobalUnlock(hstr);
    CloseClipboard();
    return true;
 err2: GlobalUnlock(hstr);
 err1: CloseClipboard();
 err0: return false;
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    static int x0, y0;

    switch (msg) {

    case WM_CLOSE:
        if (window.lock) return 0; else break;

    case WM_COMMAND:
        SendMessage(hwnd, WM_SYSCOMMAND, LOWORD(wparam), 0);
        return 0;

    case WM_CONTEXTMENU: {
        EnableMenuItem(window.hmenu, SC_RESTORE,
                       MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
        EnableMenuItem(window.hmenu, SC_MINIMIZE, MF_BYCOMMAND | MF_ENABLED);
        SetMenuDefaultItem(window.hmenu, static_cast<UINT>(-1), 0);
        int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
        if (x == -1 && y == -1) {
            WINDOWPLACEMENT p;
            if (GetWindowPlacement(hwnd, &p)) {
                x = (p.rcNormalPosition.left + p.rcNormalPosition.right) / 2;
                y = (p.rcNormalPosition.top + p.rcNormalPosition.bottom) / 2;
            }
        }
        TrackPopupMenuEx(window.hmenu, 0, x, y, hwnd, 0);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(ERROR_SUCCESS);
        return 0;

    case WM_INITMENU: {
        static const size_t BUFLEN = 256;
        static wchar_t buf[BUFLEN];
        MENUITEMINFO m = {sizeof(m), MIIM_TYPE};
        m.dwTypeData = buf;
        get_coordinates_str(buf, BUFLEN, hwnd, strs::MENU_COORDINATES);
        SetMenuItemInfo(window.hmenu, IDM_COORDINATES, 0, &m);
        long x, y;
        bool valid_clipboard_coords = get_clipboard_coordinates(&x, &y, hwnd);
        if (valid_clipboard_coords) {
            swprintf(buf, BUFLEN, strs::MENU_PASTECOORDS_FORMAT, x, y);
            EnableMenuItem(window.hmenu, IDM_PASTECOORDS, MF_BYCOMMAND |
                           !window.lock ? MF_ENABLED :
                           MF_DISABLED | MF_GRAYED);
        } else {
            m.dwTypeData = strs::MENU_PASTECOORDS;
            EnableMenuItem(window.hmenu, IDM_PASTECOORDS, MF_BYCOMMAND |
                           MF_DISABLED | MF_GRAYED);
        }
        SetMenuItemInfo(window.hmenu, IDM_PASTECOORDS, 0, &m);
        return 0;
    }

    case WM_LBUTTONDOWN:
        if (!window.lock) {
            x0 = GET_X_LPARAM(lparam);
            y0 = GET_Y_LPARAM(lparam);
            SetCapture(hwnd);
            SetCursor(window.hcursor_move);
            return 0;
        }

    case WM_LBUTTONUP:
        if (!window.lock) {
            SetCursor(window.hcursor_default);
            ReleaseCapture();
            return 0;
        }

    case WM_MOUSEMOVE:
        if (!window.lock && GetCapture() == hwnd) {
            RECT rect;
            if (GetWindowRect(hwnd, &rect)) {
                SetWindowPos(hwnd, HWND_TOPMOST,
                             rect.left + GET_X_LPARAM(lparam) - x0,
                             rect.top + GET_Y_LPARAM(lparam) - y0,
                             0, 0, SWP_NOSIZE);
            }
            return 0;
        }

    case WM_SYSCOMMAND:
        switch (wparam & 0xfff0) {
        case IDM_COORDINATES:
            copy_coordinates(hwnd);
            return 0;
        case IDM_PASTECOORDS:
            paste_coordinates(hwnd);
            return 0;
        case IDM_PASTETITLE:
            paste_title(hwnd);
            return 0;
        case IDM_LOCKMARKER: {
            window.lock = !window.lock;
            MENUITEMINFO m = {sizeof(m), MIIM_STATE};
            m.fState = window.lock ? MFS_CHECKED : MFS_UNCHECKED;
            SetMenuItemInfo(window.hmenu, IDM_LOCKMARKER, 0, &m);
            UINT state = MF_BYCOMMAND |
                window.lock ? MF_DISABLED | MF_GRAYED : MF_ENABLED;
            EnableMenuItem(window.hmenu, IDM_PASTETITLE, state);
            EnableMenuItem(window.hmenu, SC_CLOSE, state);
            return 0;
        }
        case IDM_NUDGELEFT:
            if (!window.lock) {
                WINDOWPLACEMENT p;
                GetWindowPlacement(hwnd, &p);
                --p.rcNormalPosition.left;
                --p.rcNormalPosition.right;
                SetWindowPlacement(hwnd, &p);
                return 0;
            }
        case IDM_NUDGERIGHT:
            if (!window.lock)  {
                WINDOWPLACEMENT p;
                GetWindowPlacement(hwnd, &p);
                ++p.rcNormalPosition.left;
                ++p.rcNormalPosition.right;
                SetWindowPlacement(hwnd, &p);
                return 0;
            }
        case IDM_NUDGEUP:
            if (!window.lock)  {
                WINDOWPLACEMENT p;
                GetWindowPlacement(hwnd, &p);
                --p.rcNormalPosition.top;
                --p.rcNormalPosition.bottom;
                SetWindowPlacement(hwnd, &p);
                return 0;
            }
        case IDM_NUDGEDOWN:
            if (!window.lock)  {
                WINDOWPLACEMENT p;
                GetWindowPlacement(hwnd, &p);
                ++p.rcNormalPosition.top;
                ++p.rcNormalPosition.bottom;
                SetWindowPlacement(hwnd, &p);
                return 0;
            }
        }
        break;

    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int CALLBACK wWinMain(HINSTANCE hinstance, HINSTANCE, LPWSTR, int ncmdshow) {

    window.hinstance = hinstance;
    window.hcursor_default = static_cast<HCURSOR>(
        LoadImage(0, MAKEINTRESOURCE(OCR_NORMAL), IMAGE_CURSOR,
                  0, 0, LR_DEFAULTSIZE | LR_SHARED));
    window.hcursor_move = static_cast<HCURSOR>(
        LoadImage(0, MAKEINTRESOURCE(OCR_SIZEALL), IMAGE_CURSOR,
                  0, 0, LR_DEFAULTSIZE | LR_SHARED));

    INITCOMMONCONTROLSEX i = {sizeof(i), ICC_STANDARD_CLASSES};
    if (!InitCommonControlsEx(&i)) return GetLastError();

    WNDCLASSEX w = {sizeof(w)};
    w.lpszClassName = L"MAIN";
    w.lpfnWndProc = wndproc;
    w.hInstance = hinstance;
    w.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    w.hCursor = window.hcursor_default;
    LPCWSTR mainIcon = MAKEINTRESOURCE(IDI_MAIN);
    w.hIcon = (HICON) LoadImage(hinstance, mainIcon, IMAGE_ICON,
                                0, 0, LR_DEFAULTSIZE);
    w.hIconSm = (HICON) LoadImage(hinstance, mainIcon, IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), 0);
    RegisterClassEx(&w);

    window.hwnd = CreateWindowEx(WS_EX_LAYERED, w.lpszClassName,
                                 strs::PROGRAM_NAME, 0, 0, 0, 0, 0, 0, 0,
                                 hinstance, 0);
    if (!window.hwnd) return EXIT_FAILURE;
    SetWindowLongPtr(window.hwnd, GWL_STYLE, WS_SYSMENU | WS_MINIMIZEBOX);
    SetWindowPos(window.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOREPOSITION);
    if (ncmdshow == SW_MAXIMIZE) ncmdshow = SW_SHOW;

    window.hmenu = GetSystemMenu(window.hwnd, 0);
    RemoveMenu(window.hmenu, SC_MOVE, MF_BYCOMMAND);
    RemoveMenu(window.hmenu, SC_SIZE, MF_BYCOMMAND);
    RemoveMenu(window.hmenu, SC_MAXIMIZE, MF_BYCOMMAND);

    // (C, M, N, & R are reserved access keys)
    ACCEL accels[] = {
        {FVIRTKEY | FCONTROL, 'C', IDM_COORDINATES},
        {FVIRTKEY | FCONTROL, 'V', IDM_PASTECOORDS},
        {FVIRTKEY | FCONTROL | FSHIFT, 'V', IDM_PASTETITLE},
        {FVIRTKEY | FCONTROL, 'L', IDM_LOCKMARKER},
        {FVIRTKEY | FCONTROL, 'P', IDM_SETPOSITION},
        {FVIRTKEY | FCONTROL, 'T', IDM_SETTITLE},
        {FVIRTKEY, VK_LEFT, IDM_NUDGELEFT},
        {FVIRTKEY, VK_RIGHT, IDM_NUDGERIGHT},
        {FVIRTKEY, VK_UP, IDM_NUDGEUP},
        {FVIRTKEY, VK_DOWN, IDM_NUDGEDOWN}
    };
    MENUITEMINFO m = {sizeof(m),
                      MIIM_ID | MIIM_FTYPE | MIIM_STRING | MIIM_STATE,
                      MFT_STRING};
    m.wID = IDM_COORDINATES;
    m.dwTypeData = strs::MENU_COORDINATES;
    InsertMenuItem(window.hmenu, SC_RESTORE, 0, &m);
    m.wID = IDM_PASTECOORDS;
    m.dwTypeData = strs::MENU_PASTECOORDS;
    InsertMenuItem(window.hmenu, SC_RESTORE, 0, &m);
    m.wID = IDM_PASTETITLE;
    m.dwTypeData = strs::MENU_PASTETITLE;
    InsertMenuItem(window.hmenu, SC_RESTORE, 0, &m);
    m.wID = IDM_LOCKMARKER;
    m.dwTypeData = strs::MENU_LOCKMARKER;
    InsertMenuItem(window.hmenu, SC_RESTORE, 0, &m);
    m.fMask = MIIM_FTYPE;
    m.fType = MFT_SEPARATOR;
    InsertMenuItem(window.hmenu, SC_RESTORE, 0, &m);

    ULONG_PTR gdiplus;
    Gdiplus::GdiplusStartupInput gdiplusParams;
    Gdiplus::GdiplusStartup(&gdiplus, &gdiplusParams, 0);
    {
        HDC hscreen = GetDC(0);
        HDC hdc = CreateCompatibleDC(hscreen);
        Gdiplus::Bitmap image(strs::IMAGE_FILENAME);
        if (image.GetLastStatus() != Gdiplus::Ok) {
            MessageBox(0, strs::ERR_IMAGE_M, strs::ERR_IMAGE_T, MB_ICONERROR);
            return image.GetLastStatus();
        }
        SIZE size = {image.GetWidth(), image.GetHeight()};
        HBITMAP hbitmap;
        if (image.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbitmap)
            != Gdiplus::Ok)
            return image.GetLastStatus();
        HGDIOBJ old = SelectObject(hdc, hbitmap);
        BLENDFUNCTION b = {AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA};
        if (!GdiAlphaBlend(hscreen, 0, 0, size.cx, size.cy, hdc,
                           0, 0, size.cx, size.cy, b))
            return GetLastError();
        POINT loc = {0};
        UpdateLayeredWindow(window.hwnd, hscreen, &loc, &size,
                            hdc, &loc, 0, &b, ULW_ALPHA);
        SelectObject(hdc, old);
        DeleteDC(hdc);
        ReleaseDC(0, hscreen);
    }
    Gdiplus::GdiplusShutdown(gdiplus);

    ShowWindow(window.hwnd, ncmdshow);
    MSG msg;
    HACCEL haccel = CreateAcceleratorTable(accels, sizeof(accels));
    while (GetMessage(&msg, 0, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, haccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DestroyAcceleratorTable(haccel);

    DestroyIcon(w.hIcon);
    DestroyIcon(w.hIconSm);
    return (int) msg.wParam;
}
