#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <commdlg.h>
#include "graphics.h"

/* Global variables */
static HWND hwnd = NULL;
static HDC hdc = NULL;
static HDC memdc = NULL;
static HBITMAP membmp = NULL;
static HFONT hfont = NULL;
uint32_t wwidth = 0;
uint32_t wheight = 0;
static int font_size = 12;

extern uint8_t pendingRedraw;

Event* evt = 0;

static HMENU menuSlot[256] = {0};
static uint8_t menuSlots = 0;
static HMENU hMenu;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    switch (msg) {
        case WM_KEYDOWN:
            evt->type = EVENT_KEYDOWN;
            evt->key = (int)wParam;
            evt->pending = 1;
            return 0;

        case WM_KEYUP:
            evt->type = EVENT_KEYUP;
            evt->key = (int)wParam;
            evt->pending = 1;
            return 0;

        case WM_MOUSEMOVE:
            evt->type = EVENT_MOUSEMOVE;
            evt->x = LOWORD(lParam);
            evt->y = HIWORD(lParam);
            evt->pending = 1;
            return 0;

        case WM_LBUTTONDOWN:
            evt->type = EVENT_MOUSEBUTTONDOWN;
            evt->x = LOWORD(lParam);
            evt->y = HIWORD(lParam);
            evt->pending = 1;
            return 0;

        case WM_LBUTTONUP:
            evt->type = EVENT_MOUSEBUTTONUP;
            evt->x = LOWORD(lParam);
            evt->y = HIWORD(lParam);
            evt->pending = 1;
            return 0;

        case WM_MOUSEWHEEL:
            evt->type = EVENT_MOUSESCROLL;
            evt->scrollDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            evt->x = LOWORD(lParam);
            evt->y = HIWORD(lParam);
            evt->pending = 1;
            return 0;

        case WM_SIZE:
            evt->pending = 1;
            evt->type = EVENT_RESIZE;
            evt->width = LOWORD(lParam);
            evt->height = HIWORD(lParam);
            wwidth = evt->width;
            wheight = evt->height;

            /* Recreate backing buffer */
            if (memdc) {
                if (membmp) DeleteObject(membmp);
                membmp = CreateCompatibleBitmap(hdc, wwidth, wheight);
                SelectObject(memdc, membmp);
            }
            pendingRedraw = 1;
            return 0;

        case WM_CLOSE:
            evt->type = EVENT_QUIT;
            evt->pending = 1;
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_ERASEBKGND:
            return 1; /* Prevent flicker */

        case WM_COMMAND:
            evt->pending=1;
            evt->type = EVENT_COMMAND;
            evt->command = LOWORD(wParam);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int init_graphics(uint32_t width, uint32_t height, Event* event) {
    WNDCLASS wc;
    HINSTANCE hInstance;
    RECT rect;

    //evt = malloc(sizeof(Event));

    hInstance = GetModuleHandle(NULL);
    evt = event;

    /* Register window class */
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "GrrGifWindow";

    if (!RegisterClass(&wc)) {
        return 0;
    }

    /* Adjust window size for client area */
    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    /* Create window */
    hwnd = CreateWindow(
        "GrrGifWindow",
        "GrrGif - [no project]",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        return 0;
    }

    hMenu = CreateMenu();

    wwidth = width;
    wheight = height;

    /* Get device context */
    hdc = GetDC(hwnd);
    if (!hdc) {
        return 0;
    }

    /* Create memory DC for double buffering */
    memdc = CreateCompatibleDC(hdc);
    if (!memdc) {
        return 0;
    }

    membmp = CreateCompatibleBitmap(hdc, width, height);
    if (!membmp) {
        return 0;
    }

    SelectObject(memdc, membmp);

    /* Create default font */
    font_size = FONT_SIZE_NORMAL > 0 ? FONT_SIZE_NORMAL : 12;
    hfont = CreateFont(
        font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New"
    );

    if (hfont) {
        SelectObject(memdc, hfont);
    }

    /* Show window */
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return 1;
}

uint8_t create_menu() {
    if (menuSlots > 254) return 0;
    menuSlot[menuSlots] = CreateMenu();
    menuSlots++;
    return menuSlots-1;
}

void append_menu(uint8_t handle, char* name, uint32_t code) {
    AppendMenu(menuSlot[handle], MF_STRING, code, name);
}

void append_menu_separator(uint8_t handle) {
    AppendMenu(menuSlot[handle], MF_SEPARATOR, 0, NULL);
}

void finalize_menu(uint8_t handle, char* name) {
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)menuSlot[handle], name);
    SetMenu(hwnd, hMenu);
}

int poll_event() {

    // todo: maybe a stack of events would be nice
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            evt->type = EVENT_QUIT;
            evt->pending = 1;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    Sleep(1);
    if (!evt->pending) return 0;
    evt->pending = 0;
    return 1;
}

int set_font_size(int size) {
    if (!memdc) return 0;

    if (hfont) {
        DeleteObject(hfont);
    }

    font_size = size;
    hfont = CreateFont(
        size * 2, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New"
    );

    if (hfont) {
        SelectObject(memdc, hfont);
        return 1;
    }

    return 0;
}

int draw_rect(Rect *rect, uint32_t color) {
    HBRUSH brush;
    RECT r;

    if (!memdc) return 0;

    r.left = rect->x;
    r.top = rect->y;
    r.right = rect->x + rect->width;
    r.bottom = rect->y + rect->height;

    brush = CreateSolidBrush(RGB(
        (color >> 16) & 0xFF,
                                 (color >> 8) & 0xFF,
                                 color & 0xFF
    ));

    FillRect(memdc, &r, brush);
    DeleteObject(brush);

    return 1;
}

int clear_graphics(uint32_t color) {
    Rect r;
    r.x = 0;
    r.y = 0;
    r.width = wwidth;
    r.height = wheight;
    return draw_rect(&r, color);
}

int blit_rgb8888(uint32_t *pixels, uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
    BITMAPINFO bmi;

    if (!memdc) return 0;

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -(int)height; /* Top-down bitmap */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetDIBitsToDevice(
        memdc, x, y, width, height,
        0, 0, 0, height,
        pixels, &bmi, DIB_RGB_COLORS
    );

    return 1;
}

int flush_graphics() {
    if (!hdc || !memdc) return 0;

    /* Copy from memory DC to screen DC */
    BitBlt(hdc, 0, 0, wwidth, wheight, memdc, 0, 0, SRCCOPY);

    return 1;
}

int close_graphics() {
    if (hfont) DeleteObject(hfont);
    if (membmp) DeleteObject(membmp);
    if (memdc) DeleteDC(memdc);
    if (hdc) ReleaseDC(hwnd, hdc);
    if (hwnd) DestroyWindow(hwnd);

    return 1;
}

int draw_text(const char *text, int x, int y, uint32_t color) {
    if (!memdc) return 0;

    SetTextColor(memdc, RGB(
        (color >> 16) & 0xFF,
                            (color >> 8) & 0xFF,
                            color & 0xFF
    ));

    SetBkMode(memdc, TRANSPARENT);
    TextOut(memdc, x, y, text, strlen(text));

    return 1;
}

int draw_text_limited(char *textraw, int x, int y, uint32_t color, uint8_t anchor, uint32_t max_width) {
    SIZE text_size;
    
    if (!memdc) return 0;
    char *text = malloc(strlen(textraw) + 1);
    memcpy(text, textraw, strlen(textraw) + 1);
    
    if (max_width > 0) {
        while (1) {
            GetTextExtentPoint32(memdc, text, strlen(text), &text_size);
            if (text_size.cx <= max_width) break;
            text[strlen(text)-1] = 0;
        }
    }

    GetTextExtentPoint32(memdc, text, strlen(text), &text_size);
    switch (anchor) {
        case ANCHOR_MIDDLE:
            x -= text_size.cx / 2;
            break;
        case ANCHOR_RIGHT:
            x -= text_size.cx;
            break;
    }

    SetTextColor(memdc, RGB(
        (color >> 16) & 0xFF,
                            (color >> 8) & 0xFF,
                            color & 0xFF
    ));

    SetBkMode(memdc, TRANSPARENT);
    TextOut(memdc, x, y, text, strlen(text));

    return 1;
}

int draw_text_bg(const char *text, int x, int y, uint32_t fg_color, uint32_t bg_color) {
    SIZE text_size;
    Rect r;

    if (!memdc) return 0;

    /* Get text dimensions */
    GetTextExtentPoint32(memdc, text, strlen(text), &text_size);

    /* Draw background rectangle */
    r.x = x;
    r.y = y;
    r.width = text_size.cx;
    r.height = text_size.cy;
    draw_rect(&r, bg_color);

    /* Draw text */
    SetTextColor(memdc, RGB(
        (fg_color >> 16) & 0xFF,
                            (fg_color >> 8) & 0xFF,
                            fg_color & 0xFF
    ));

    SetBkMode(memdc, TRANSPARENT);
    TextOut(memdc, x, y, text, strlen(text));

    return 1;
}

int draw_text_anchor(char *text, int x, int y, uint32_t color, uint8_t anchor) {
    return draw_text_limited(text, x, y, color, anchor, 0);
}

void set_cursor(int type) {
    HCURSOR cursor = NULL;
    switch(type) {
        case CURSOR_NORMAL: cursor = LoadCursor(NULL, IDC_ARROW); break;
        case CURSOR_BUSY:   cursor = LoadCursor(NULL, IDC_WAIT); break;
        case CURSOR_SIZEH:  cursor = LoadCursor(NULL, IDC_SIZEWE); break;
        case CURSOR_SIZEV:  cursor = LoadCursor(NULL, IDC_SIZENS); break;
        case CURSOR_MOVE:   cursor = LoadCursor(NULL, IDC_SIZEALL); break;
    }
    if (cursor) SetCursor(cursor);
}

void set_window_title(char *title) {
    SetWindowTextA(hwnd, title);
}

char* choose_file(uint8_t type) {
    char* szFileName = malloc(512);
    if (!szFileName) {printf("Failed to alloc filename\n"); return 0; }
    
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ZeroMemory(szFileName, 512);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = type ? "GrrGif projects\0*.grrproj\0All Files\0*.*\0\0" : "GIF animations\0*.GIF\0All Files\0*.*\0\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = 511;
    ofn.lpstrTitle = "Select a file";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileName(&ofn)) {
        return szFileName; }
    else
        return NULL;
}

char* choose_save_file(uint8_t type) {
    char* szFileName = malloc(512);
    if (!szFileName) {printf("Failed to alloc filename\n"); return 0; }

    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ZeroMemory(szFileName, 512);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = type ? "GrrGif projects\0*.grrproj\0All Files\0*.*\0\0" : "GIF animations\0*.GIF\0All Files\0*.*\0\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = 511;
    ofn.lpstrTitle = "Save Project As";
    ofn.lpstrDefExt = "grrproj";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    if (GetSaveFileName(&ofn)) {
        printf("Saving to %s\n", szFileName);
        return szFileName;
    }
    else {
        free(szFileName);
        return NULL;
    }
}

int messagebox(char* title, char* body, int type) {
    int icon = 0;
    switch (type) {
        case MSGBOX_INFO: icon = MB_ICONINFORMATION | MB_OK; break;
        case MSGBOX_WARNING: icon = MB_ICONWARNING | MB_OK; break;
        case MSGBOX_ERROR: icon = MB_ICONERROR | MB_OK; break;
        case MSGBOX_QUESTION: icon = MB_ICONQUESTION | MB_YESNO; break;
    }
    return MessageBox(NULL, body, title, icon);
}
