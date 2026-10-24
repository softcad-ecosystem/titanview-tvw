/*
 * libtitangfx — motorul grafic deschis al ecosistemului SoftCAD (TVW/TitanView).
 * Copyright (C) 2026 SoftCAD C+C TOPO S.R.L.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details. You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */
/*
 * ts_titan_ui.h — Cross-Platform Native GUI
 * ============================================
 * Windows: Win32 API (zero deps)
 * Linux:   X11 (-lX11)
 *
 * Creates native windows, handles events, blits TS_Canvas to screen.
 * Integrates with titan_gfx for rendering.
 * Single-header C99.
 *
 * #define TS_TITAN_UI_IMPLEMENTATION in ONE .c file.
 *
 * Usage:
 *   TS_Window *win = ts_window_new("Plan Cadastral", 1280, 720);
 *   TS_Canvas *canvas = ts_canvas_new(1280, 720);
 *   // ... draw on canvas ...
 *   while (ts_window_alive(win)) {
 *       TS_Event ev;
 *       while (ts_window_poll(win, &ev)) {
 *           if (ev.type == TS_EV_KEY && ev.key == TS_KEY_ESC) ts_window_close(win);
 *       }
 *       ts_window_blit(win, canvas);
 *       ts_window_present(win);
 *   }
 *   ts_window_free(win);
 *
 * SoftCAD C+C TOPO © 2026
 */
#ifndef TS_TITAN_UI_H
#define TS_TITAN_UI_H

#include <stdint.h>
#include "ts_gfx_canvas.h"

/* ── Event Types ───────────────────────────────────────────────────────── */

enum {
    TS_EV_NONE = 0,
    TS_EV_CLOSE,           /* window close requested */
    TS_EV_KEY,             /* key press */
    TS_EV_KEYUP,           /* key release */
    TS_EV_MOUSE_MOVE,      /* mouse moved */
    TS_EV_MOUSE_DOWN,      /* mouse button pressed */
    TS_EV_MOUSE_UP,        /* mouse button released */
    TS_EV_MOUSE_WHEEL,     /* scroll wheel */
    TS_EV_RESIZE,          /* window resized */
    TS_EV_FOCUS,           /* window gained focus */
    TS_EV_BLUR,            /* window lost focus */
    TS_EV_DROP,            /* file dropped */
    TS_EV_TEXT,            /* committed Unicode scalar in key (Win32) */
};

/* ── Key Codes ─────────────────────────────────────────────────────────── */

enum {
    TS_KEY_NONE = 0,
    TS_KEY_ESC = 27,
    TS_KEY_ENTER = 13,
    TS_KEY_TAB = 9,
    TS_KEY_BACKSPACE = 8,
    TS_KEY_DELETE = 127,
    TS_KEY_SPACE = 32,
    TS_KEY_LEFT = 256, TS_KEY_RIGHT, TS_KEY_UP, TS_KEY_DOWN,
    TS_KEY_HOME, TS_KEY_END, TS_KEY_PGUP, TS_KEY_PGDN,
    TS_KEY_F1 = 290, TS_KEY_F2, TS_KEY_F3, TS_KEY_F4,
    TS_KEY_F5, TS_KEY_F6, TS_KEY_F7, TS_KEY_F8,
    TS_KEY_F9, TS_KEY_F10, TS_KEY_F11, TS_KEY_F12,
    TS_KEY_SHIFT = 340, TS_KEY_CTRL, TS_KEY_ALT,
};

/* ── Mouse Buttons ─────────────────────────────────────────────────────── */

enum {
    TS_MOUSE_LEFT   = 1,
    TS_MOUSE_RIGHT  = 2,
    TS_MOUSE_MIDDLE = 4,
};

/* ── Event ─────────────────────────────────────────────────────────────── */

typedef struct {
    int type;           /* TS_EV_* */
    int key;            /* TS_KEY_* or ASCII char */
    int button;         /* TS_MOUSE_* */
    int mouse_x;        /* cursor position */
    int mouse_y;
    int scroll;         /* wheel delta (+up, -down) */
    int width, height;  /* for TS_EV_RESIZE */
    int modifiers;      /* bit flags: 1=shift, 2=ctrl, 4=alt */
    char drop_path[512]; /* for TS_EV_DROP */
} TS_Event;

/* ── Window (opaque) ───────────────────────────────────────────────────── */

typedef struct TS_Window TS_Window;

/* ── API ───────────────────────────────────────────────────────────────── */

TS_Window *ts_window_new(const char *title, int width, int height);
void       ts_window_free(TS_Window *win);
int        ts_window_alive(const TS_Window *win);
void       ts_window_close(TS_Window *win);

/* Event polling */
int  ts_window_poll(TS_Window *win, TS_Event *ev);
void ts_window_wait(TS_Window *win, TS_Event *ev);  /* blocks until event */

/* Display */
void ts_window_blit(TS_Window *win, const TS_Canvas *canvas);
void ts_window_present(TS_Window *win);
void ts_window_set_title(TS_Window *win, const char *title);

/* Query */
int  ts_window_width(const TS_Window *win);
int  ts_window_height(const TS_Window *win);
int  ts_window_mouse_x(const TS_Window *win);
int  ts_window_mouse_y(const TS_Window *win);
int  ts_window_key_down(const TS_Window *win, int key);

/* Timing */
double ts_window_time(void);   /* seconds since first call */
void   ts_window_sleep(int ms);

/* ══════════════════════════════════════════════════════════════════════ */
/*  IMPLEMENTATION                                                       */
/* ══════════════════════════════════════════════════════════════════════ */
#ifdef TS_TITAN_UI_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════════════════ */
/*  WINDOWS (Win32)                                                      */
/* ══════════════════════════════════════════════════════════════════════ */
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

struct TS_Window {
    HWND     hwnd;
    HDC      hdc;
    int      width, height;
    int      alive;
    int      mouse_x, mouse_y;
    uint8_t  keys[512];
    /* Blit buffer */
    BITMAPINFO bmi;
    uint32_t  *blit_buf;
    int        blit_w, blit_h;
    /* Event queue */
    TS_Event   eq[64];
    int        eq_head, eq_tail;
    unsigned int pending_surrogate;
};

static void _win_push_event(TS_Window *w, TS_Event ev) {
    int next = (w->eq_head + 1) % 64;
    if (next == w->eq_tail) return; /* full */
    w->eq[w->eq_head] = ev;
    w->eq_head = next;
}

static int _win_key(int key) {
    if (key >= VK_F1 && key <= VK_F12) return TS_KEY_F1 + key - VK_F1;
    switch (key) {
    case VK_LEFT: return TS_KEY_LEFT; case VK_RIGHT: return TS_KEY_RIGHT;
    case VK_UP: return TS_KEY_UP; case VK_DOWN: return TS_KEY_DOWN;
    case VK_HOME: return TS_KEY_HOME; case VK_END: return TS_KEY_END;
    case VK_PRIOR: return TS_KEY_PGUP; case VK_NEXT: return TS_KEY_PGDN;
    case VK_DELETE: return TS_KEY_DELETE;
    case VK_SHIFT: return TS_KEY_SHIFT; case VK_CONTROL: return TS_KEY_CTRL;
    case VK_MENU: return TS_KEY_ALT;
    default: return key;
    }
}

static void _win_commit_char(TS_Window *win, unsigned int cp) {
    TS_Event ev; memset(&ev, 0, sizeof(ev));
    if (cp >= 0xD800 && cp <= 0xDBFF) { win->pending_surrogate = cp; return; }
    if (cp >= 0xDC00 && cp <= 0xDFFF) {
        if (!win->pending_surrogate) return;
        cp = 0x10000 + ((win->pending_surrogate - 0xD800) << 10) + cp - 0xDC00;
    }
    win->pending_surrogate = 0;
    if (cp < 32 || cp == 127 || cp > 0x10FFFF) return;
    ev.type = TS_EV_TEXT; ev.key = (int)cp;
    _win_push_event(win, ev);
}

int ts_window_text_input_version(void) { return 1; }

static LRESULT CALLBACK _wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TS_Window *win = (TS_Window *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!win) return DefWindowProcW(hwnd, msg, wp, lp);

    TS_Event ev; memset(&ev, 0, sizeof(ev));
    ev.modifiers = ((GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0)
                 | ((GetKeyState(VK_CONTROL) & 0x8000) ? 2 : 0)
                 | ((GetKeyState(VK_MENU) & 0x8000) ? 4 : 0);

    switch (msg) {
    case WM_CLOSE:
        ev.type = TS_EV_CLOSE; _win_push_event(win, ev);
        win->alive = 0; return 0;
    case WM_SIZE:
        win->width = LOWORD(lp); win->height = HIWORD(lp);
        ev.type = TS_EV_RESIZE; ev.width = win->width; ev.height = win->height;
        _win_push_event(win, ev); return 0;
    case WM_CHAR:
        _win_commit_char(win, (unsigned int)wp); return 0;
    case WM_UNICHAR:
        if (wp == UNICODE_NOCHAR) return TRUE;
        _win_commit_char(win, (unsigned int)wp); return 0;
    case WM_KEYDOWN:
        ev.type = TS_EV_KEY; ev.key = _win_key((int)wp);
        if (ev.key >= 0 && ev.key < 512) win->keys[ev.key] = 1;
        _win_push_event(win, ev); return 0;
    case WM_KEYUP:
        ev.type = TS_EV_KEYUP; ev.key = _win_key((int)wp);
        if (ev.key >= 0 && ev.key < 512) win->keys[ev.key] = 0;
        _win_push_event(win, ev); return 0;
    case WM_MOUSEMOVE:
        win->mouse_x = LOWORD(lp); win->mouse_y = HIWORD(lp);
        ev.type = TS_EV_MOUSE_MOVE; ev.mouse_x = win->mouse_x; ev.mouse_y = win->mouse_y;
        _win_push_event(win, ev); return 0;
    case WM_LBUTTONDOWN:
        ev.type = TS_EV_MOUSE_DOWN; ev.button = TS_MOUSE_LEFT;
        ev.mouse_x = LOWORD(lp); ev.mouse_y = HIWORD(lp);
        _win_push_event(win, ev); return 0;
    case WM_LBUTTONUP:
        ev.type = TS_EV_MOUSE_UP; ev.button = TS_MOUSE_LEFT;
        _win_push_event(win, ev); return 0;
    case WM_RBUTTONDOWN:
        ev.type = TS_EV_MOUSE_DOWN; ev.button = TS_MOUSE_RIGHT;
        _win_push_event(win, ev); return 0;
    case WM_RBUTTONUP:
        ev.type = TS_EV_MOUSE_UP; ev.button = TS_MOUSE_RIGHT;
        _win_push_event(win, ev); return 0;
    case WM_MOUSEWHEEL:
        ev.type = TS_EV_MOUSE_WHEEL;
        ev.scroll = GET_WHEEL_DELTA_WPARAM(wp) / 120;
        _win_push_event(win, ev); return 0;
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wp;
        /* 4.66: numele în UTF-8 (ca pe Linux și ca restul API-ului). Înainte
           DragQueryFileA dădea cod ANSI → diacriticele se pierdeau. */
        wchar_t wpath[1024];
        ev.type = TS_EV_DROP;
        ev.drop_path[0] = 0;
        if (DragQueryFileW(hDrop, 0, wpath, 1024) > 0 &&
            WideCharToMultiByte(CP_UTF8, 0, wpath, -1, ev.drop_path,
                                (int)sizeof(ev.drop_path), NULL, NULL) <= 0)
            ev.drop_path[0] = 0;
        _win_push_event(win, ev);
        DragFinish(hDrop);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static TS_Window *_ts_window_create(const char *title, int width, int height, int visible) {
    TS_Window *win = (TS_Window *)calloc(1, sizeof(TS_Window));
    if (!win) return NULL;
    win->width = width; win->height = height; win->alive = 1;

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = _wndproc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"TitanUI";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_OWNDC;
    RegisterClassW(&wc);

    RECT r = {0, 0, width, height};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    wchar_t wide_title[1024];
    if (!MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, 1024)) wide_title[0] = 0;
    win->hwnd = CreateWindowW(L"TitanUI", wide_title,
        WS_OVERLAPPEDWINDOW | (visible ? WS_VISIBLE : 0),
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, wc.hInstance, NULL);

    if (!win->hwnd) { free(win); return NULL; }
    SetWindowLongPtr(win->hwnd, GWLP_USERDATA, (LONG_PTR)win);
    DragAcceptFiles(win->hwnd, TRUE);
    win->hdc = GetDC(win->hwnd);

    /* Setup BITMAPINFO for StretchDIBits */
    memset(&win->bmi, 0, sizeof(win->bmi));
    win->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    win->bmi.bmiHeader.biPlanes = 1;
    win->bmi.bmiHeader.biBitCount = 32;
    win->bmi.bmiHeader.biCompression = BI_RGB;

    return win;
}

TS_Window *ts_window_new(const char *title, int width, int height) {
    return _ts_window_create(title, width, height, 1);
}

void ts_window_free(TS_Window *win) {
    if (!win) return;
    ReleaseDC(win->hwnd, win->hdc);
    DestroyWindow(win->hwnd);
    free(win->blit_buf);
    free(win);
}

int ts_window_alive(const TS_Window *win) { return win && win->alive; }
void ts_window_close(TS_Window *win) { if (win) win->alive = 0; }

int ts_window_poll(TS_Window *win, TS_Event *ev) {
    MSG msg;
    while (PeekMessageW(&msg, win->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (win->eq_tail != win->eq_head) {
        *ev = win->eq[win->eq_tail];
        win->eq_tail = (win->eq_tail + 1) % 64;
        return 1;
    }
    memset(ev, 0, sizeof(*ev));
    return 0;
}

void ts_window_wait(TS_Window *win, TS_Event *ev) {
    while (win->alive) {
        if (ts_window_poll(win, ev)) return;
        Sleep(1);
    }
}

void ts_window_blit(TS_Window *win, const TS_Canvas *canvas) {
    win->bmi.bmiHeader.biWidth = canvas->width;
    win->bmi.bmiHeader.biHeight = -canvas->height; /* top-down */
    StretchDIBits(win->hdc, 0, 0, win->width, win->height,
                  0, 0, canvas->width, canvas->height,
                  canvas->pixels, &win->bmi, DIB_RGB_COLORS, SRCCOPY);
}

void ts_window_present(TS_Window *win) {
    /* Win32 StretchDIBits is immediate — no extra present needed */
    (void)win;
}

void ts_window_set_title(TS_Window *win, const char *title) {
    wchar_t wide_title[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, 1024))
        SetWindowTextW(win->hwnd, wide_title);
}

int ts_window_width(const TS_Window *win)  { return win->width; }
int ts_window_height(const TS_Window *win) { return win->height; }
int ts_window_mouse_x(const TS_Window *win) { return win->mouse_x; }
int ts_window_mouse_y(const TS_Window *win) { return win->mouse_y; }
int ts_window_key_down(const TS_Window *win, int key) {
    return (key >= 0 && key < 512) ? win->keys[key] : 0;
}

double ts_window_time(void) {
    static LARGE_INTEGER freq = {0}, start = {0};
    if (freq.QuadPart == 0) { QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&start); }
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
}

void ts_window_sleep(int ms) { Sleep((DWORD)ms); }

/* ══════════════════════════════════════════════════════════════════════ */
/*  LINUX (X11)                                                          */
/* ══════════════════════════════════════════════════════════════════════ */
#elif defined(__linux__) || defined(__unix__)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <sys/time.h>

struct TS_Window {
    Display    *dpy;
    Window      xwin;
    GC          gc;
    XImage     *ximg;
    Atom        wm_delete;
    int         width, height;
    int         alive;
    int         mouse_x, mouse_y;
    uint8_t     keys[512];
    /* Event queue */
    TS_Event    eq[64];
    int         eq_head, eq_tail;
};

static void _x11_push_event(TS_Window *w, TS_Event ev) {
    int next = (w->eq_head + 1) % 64;
    if (next == w->eq_tail) return;
    w->eq[w->eq_head] = ev;
    w->eq_head = next;
}

static int _x11_translate_key(KeySym ks) {
    if (ks >= XK_a && ks <= XK_z) return (int)('A' + (ks - XK_a));
    if (ks >= XK_0 && ks <= XK_9) return (int)('0' + (ks - XK_0));
    switch (ks) {
    case XK_Escape:    return TS_KEY_ESC;
    case XK_Return:    return TS_KEY_ENTER;
    case XK_Tab:       return TS_KEY_TAB;
    case XK_BackSpace: return TS_KEY_BACKSPACE;
    case XK_Delete:    return TS_KEY_DELETE;
    case XK_space:     return TS_KEY_SPACE;
    case XK_Left:      return TS_KEY_LEFT;
    case XK_Right:     return TS_KEY_RIGHT;
    case XK_Up:        return TS_KEY_UP;
    case XK_Down:      return TS_KEY_DOWN;
    case XK_Home:      return TS_KEY_HOME;
    case XK_End:       return TS_KEY_END;
    case XK_Page_Up:   return TS_KEY_PGUP;
    case XK_Page_Down: return TS_KEY_PGDN;
    case XK_F1: return TS_KEY_F1; case XK_F2: return TS_KEY_F2;
    case XK_F3: return TS_KEY_F3; case XK_F4: return TS_KEY_F4;
    case XK_F5: return TS_KEY_F5; case XK_F6: return TS_KEY_F6;
    case XK_F7: return TS_KEY_F7; case XK_F8: return TS_KEY_F8;
    case XK_F9: return TS_KEY_F9; case XK_F10: return TS_KEY_F10;
    case XK_F11: return TS_KEY_F11; case XK_F12: return TS_KEY_F12;
    case XK_Shift_L: case XK_Shift_R:   return TS_KEY_SHIFT;
    case XK_Control_L: case XK_Control_R: return TS_KEY_CTRL;
    case XK_Alt_L: case XK_Alt_R:       return TS_KEY_ALT;
    default: return (int)(ks & 0xFF);
    }
}

TS_Window *ts_window_new(const char *title, int width, int height) {
    TS_Window *win = (TS_Window *)calloc(1, sizeof(TS_Window));
    if (!win) return NULL;
    win->width = width; win->height = height; win->alive = 1;

    win->dpy = XOpenDisplay(NULL);
    if (!win->dpy) { free(win); return NULL; }

    int screen = DefaultScreen(win->dpy);
    win->xwin = XCreateSimpleWindow(win->dpy, RootWindow(win->dpy, screen),
                                     0, 0, (unsigned)width, (unsigned)height,
                                     0, 0, BlackPixel(win->dpy, screen));

    XStoreName(win->dpy, win->xwin, title);
    XSelectInput(win->dpy, win->xwin,
        ExposureMask | KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
        StructureNotifyMask);

    win->wm_delete = XInternAtom(win->dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(win->dpy, win->xwin, &win->wm_delete, 1);

    win->gc = XCreateGC(win->dpy, win->xwin, 0, NULL);
    XMapWindow(win->dpy, win->xwin);
    XFlush(win->dpy);

    return win;
}

void ts_window_free(TS_Window *win) {
    if (!win) return;
    if (win->ximg) { win->ximg->data = NULL; XDestroyImage(win->ximg); }
    XFreeGC(win->dpy, win->gc);
    XDestroyWindow(win->dpy, win->xwin);
    XCloseDisplay(win->dpy);
    free(win);
}

int ts_window_alive(const TS_Window *win) { return win && win->alive; }
void ts_window_close(TS_Window *win) { if (win) win->alive = 0; }

int ts_window_poll(TS_Window *win, TS_Event *ev) {
    /* Process X11 events */
    while (XPending(win->dpy)) {
        XEvent xev;
        XNextEvent(win->dpy, &xev);
        TS_Event te; memset(&te, 0, sizeof(te));

        switch (xev.type) {
        case ClientMessage:
            if ((Atom)xev.xclient.data.l[0] == win->wm_delete) {
                te.type = TS_EV_CLOSE; _x11_push_event(win, te);
                win->alive = 0;
            }
            break;
        case KeyPress:
            te.type = TS_EV_KEY;
            te.key = _x11_translate_key(XLookupKeysym(&xev.xkey, 0));
            if (te.key >= 0 && te.key < 512) win->keys[te.key] = 1;
            _x11_push_event(win, te);
            break;
        case KeyRelease:
            te.type = TS_EV_KEYUP;
            te.key = _x11_translate_key(XLookupKeysym(&xev.xkey, 0));
            if (te.key >= 0 && te.key < 512) win->keys[te.key] = 0;
            _x11_push_event(win, te);
            break;
        case MotionNotify:
            win->mouse_x = xev.xmotion.x;
            win->mouse_y = xev.xmotion.y;
            te.type = TS_EV_MOUSE_MOVE;
            te.mouse_x = win->mouse_x;
            te.mouse_y = win->mouse_y;
            _x11_push_event(win, te);
            break;
        case ButtonPress:
            te.type = TS_EV_MOUSE_DOWN;
            te.mouse_x = xev.xbutton.x;
            te.mouse_y = xev.xbutton.y;
            if (xev.xbutton.button == 1) te.button = TS_MOUSE_LEFT;
            else if (xev.xbutton.button == 3) te.button = TS_MOUSE_RIGHT;
            else if (xev.xbutton.button == 2) te.button = TS_MOUSE_MIDDLE;
            else if (xev.xbutton.button == 4) { te.type = TS_EV_MOUSE_WHEEL; te.scroll = 1; }
            else if (xev.xbutton.button == 5) { te.type = TS_EV_MOUSE_WHEEL; te.scroll = -1; }
            _x11_push_event(win, te);
            break;
        case ButtonRelease:
            te.type = TS_EV_MOUSE_UP;
            if (xev.xbutton.button == 1) te.button = TS_MOUSE_LEFT;
            else if (xev.xbutton.button == 3) te.button = TS_MOUSE_RIGHT;
            _x11_push_event(win, te);
            break;
        case ConfigureNotify:
            if (xev.xconfigure.width != win->width || xev.xconfigure.height != win->height) {
                win->width = xev.xconfigure.width;
                win->height = xev.xconfigure.height;
                te.type = TS_EV_RESIZE;
                te.width = win->width; te.height = win->height;
                _x11_push_event(win, te);
            }
            break;
        }
    }

    /* Pop from queue */
    if (win->eq_tail != win->eq_head) {
        *ev = win->eq[win->eq_tail];
        win->eq_tail = (win->eq_tail + 1) % 64;
        return 1;
    }
    memset(ev, 0, sizeof(*ev));
    return 0;
}

void ts_window_wait(TS_Window *win, TS_Event *ev) {
    while (win->alive) {
        if (ts_window_poll(win, ev)) return;
        usleep(1000);
    }
}

void ts_window_blit(TS_Window *win, const TS_Canvas *canvas) {
    int screen = DefaultScreen(win->dpy);
    Visual *vis = DefaultVisual(win->dpy, screen);
    int depth = DefaultDepth(win->dpy, screen);

    /* Create or recreate XImage if size changed */
    if (win->ximg && (win->ximg->width != canvas->width || win->ximg->height != canvas->height)) {
        win->ximg->data = NULL;
        XDestroyImage(win->ximg);
        win->ximg = NULL;
    }

    if (!win->ximg) {
        win->ximg = XCreateImage(win->dpy, vis, (unsigned)depth, ZPixmap, 0,
                                  (char *)canvas->pixels,
                                  (unsigned)canvas->width, (unsigned)canvas->height,
                                  32, canvas->width * 4);
        if (!win->ximg) return;
    } else {
        win->ximg->data = (char *)canvas->pixels;
    }

    XPutImage(win->dpy, win->xwin, win->gc, win->ximg,
              0, 0, 0, 0, (unsigned)canvas->width, (unsigned)canvas->height);
}

void ts_window_present(TS_Window *win) {
    XFlush(win->dpy);
}

void ts_window_set_title(TS_Window *win, const char *title) {
    XStoreName(win->dpy, win->xwin, title);
}

int ts_window_width(const TS_Window *win)   { return win->width; }
int ts_window_height(const TS_Window *win)  { return win->height; }
int ts_window_mouse_x(const TS_Window *win) { return win->mouse_x; }
int ts_window_mouse_y(const TS_Window *win) { return win->mouse_y; }
int ts_window_key_down(const TS_Window *win, int key) {
    return (key >= 0 && key < 512) ? win->keys[key] : 0;
}

double ts_window_time(void) {
    static struct timeval start = {0, 0};
    if (start.tv_sec == 0) gettimeofday(&start, NULL);
    struct timeval now;
    gettimeofday(&now, NULL);
    return (double)(now.tv_sec - start.tv_sec) + (double)(now.tv_usec - start.tv_usec) / 1e6;
}

void ts_window_sleep(int ms) { usleep((unsigned)(ms * 1000)); }

#else
#error "titan_ui: unsupported platform (need _WIN32 or __linux__)"
#endif

#endif /* TS_TITAN_UI_IMPLEMENTATION */
#endif /* TS_TITAN_UI_H */
