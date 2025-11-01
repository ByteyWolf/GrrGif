#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "graphics.h"

HDC hdc;
HDC memdc;
HBITMAP membmp;
HBITMAP oldbmp;
HWND hwnd;
HFONT hfont = NULL;
uint32_t wwidth;
uint32_t wheight;
uint32_t *backbuffer = NULL;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const char *CLASS_NAME = "Win9xGraphicsWindow";
static int window_created = 0;
static Event queued_event = {0};
static int has_queued_event = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            if (has_queued_event == 0) {
                queued_event.type = EVENT_QUIT;
                has_queued_event = 1;
            }
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (has_queued_event == 0) {
                queued_event.type = EVENT_KEYDOWN;
                queued_event.key = wParam;
                has_queued_event = 1;
            }
            return 0;
        case WM_KEYUP:
            if (has_queued_event == 0) {
                queued_event.type = EVENT_KEYUP;
                queued_event.key = wParam;
                has_queued_event = 1;
            }
            return 0;
        case WM_MOUSEMOVE:
            if (has_queued_event == 0) {
                queued_event.type = EVENT_MOUSEMOVE;
                queued_event.x = LOWORD(lParam);
                queued_event.y = HIWORD(lParam);
                has_queued_event = 1;
            }
            return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            if (has_queued_event == 0) {
                queued_event.type = EVENT_MOUSEBUTTONDOWN;
                queued_event.x = LOWORD(lParam);
                queued_event.y = HIWORD(lParam);
                has_queued_event = 1;
            }
            return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            if (has_queued_event == 0) {
                queued_event.type = EVENT_MOUSEBUTTONUP;
                queued_event.x = LOWORD(lParam);
                queued_event.y = HIWORD(lParam);
                has_queued_event = 1;
            }
            return 0;
        case WM_SIZE:
            wwidth = LOWORD(lParam);
            wheight = HIWORD(lParam);
            // Need to recreate memdc/membmp here!
            queued_event.type = EVENT_RESIZE;
            queued_event.width = wwidth;
            queued_event.height = wheight;
            has_queued_event = 1;
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int init_graphics(uint32_t width, uint32_t height) {
    WNDCLASSA wc = {0};

    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) return 0;

    wwidth = width;
    wheight = height;

    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd = CreateWindowExA(0, CLASS_NAME, "Graphics Window",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           rect.right - rect.left, rect.bottom - rect.top,
                           NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    hdc = GetDC(hwnd);
    memdc = CreateCompatibleDC(hdc);
    membmp = CreateCompatibleBitmap(hdc, width, height);
    oldbmp = SelectObject(memdc, membmp);

    backbuffer = (uint32_t *)malloc(width * height * sizeof(uint32_t));
    if (!backbuffer) return 0;

    int font_size = (FONT_SIZE_NORMAL > 0) ? FONT_SIZE_NORMAL : 12;
    hfont = CreateFontA(-font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");

    window_created = 1;
    return 1;
}

int poll_event(Event *event) {
    MSG msg;

    if (has_queued_event) {
        *event = queued_event;
        has_queued_event = 0;
        return 1;
    }

    if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);

        if (has_queued_event) {
            *event = queued_event;
            has_queued_event = 0;
            return 1;
        }
    }

    return 0;
}

int set_font_size(int font_size) {
    if (hfont) DeleteObject(hfont);

    hfont = CreateFontA(-font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");

    return hfont != NULL;
}

int draw_rect(Rect *rect, uint32_t color) {
    HBRUSH brush = CreateSolidBrush(RGB((color >> 16) & 0xFF,
                                        (color >> 8) & 0xFF,
                                        color & 0xFF));
    RECT r = {rect->x, rect->y, rect->x + rect->width, rect->y + rect->height};
    FillRect(memdc, &r, brush);
    DeleteObject(brush);
    return 1;
}

int clear_graphics(uint32_t color) {
    HBRUSH brush = CreateSolidBrush(RGB((color >> 16) & 0xFF,
                                        (color >> 8) & 0xFF,
                                        color & 0xFF));
    RECT r = {0, 0, wwidth, wheight};
    FillRect(memdc, &r, brush);
    DeleteObject(brush);
    return 1;
}

int blit_rgb8888_xy(uint32_t *pixels, uint32_t width, uint32_t height, int x, int y) {
    if (!pixels) return 0;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -(int)height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint32_t *converted = (uint32_t *)malloc(width * height * sizeof(uint32_t));
    if (!converted) return 0;

    for (uint32_t i = 0; i < width * height; i++) {
        uint32_t pixel = pixels[i];
        converted[i] = ((pixel & 0xFF) << 16) | (pixel & 0xFF00) | ((pixel >> 16) & 0xFF);
    }

    int result = SetDIBitsToDevice(
        memdc,
        x, y,
        width, height,
        0, 0, 0, height,
        converted,
        &bmi,
        DIB_RGB_COLORS
    );

    free(converted);
    return result != 0;
}


int get_rgb8888(uint32_t *destbuf, uint32_t width, uint32_t height) {
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -(int)height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint32_t *temp = (uint32_t *)malloc(width * height * sizeof(uint32_t));
    if (!temp) return 0;

    int result = GetDIBits(memdc, membmp, 0, height, temp, &bmi, DIB_RGB_COLORS);

    // Convert BGR to ARGB
    for (uint32_t i = 0; i < width * height; i++) {
        uint32_t pixel = temp[i];
        destbuf[i] = ((pixel & 0xFF) << 16) | (pixel & 0xFF00) | ((pixel >> 16) & 0xFF);
    }

    free(temp);
    return result != 0;
}

int flush_graphics() {
    BitBlt(hdc, 0, 0, wwidth, wheight, memdc, 0, 0, SRCCOPY);
    return 1;
}

int close_graphics() {
    if (hfont) DeleteObject(hfont);
    if (backbuffer) free(backbuffer);
    if (memdc) {
        SelectObject(memdc, oldbmp);
        DeleteObject(membmp);
        DeleteDC(memdc);
    }
    if (hdc) ReleaseDC(hwnd, hdc);
    if (hwnd) DestroyWindow(hwnd);
    if (window_created) UnregisterClassA(CLASS_NAME, GetModuleHandle(NULL));
    return 1;
}

int draw_text(const char *text, int x, int y, uint32_t color) {
    if (!hfont) return 0;

    HFONT oldfont = SelectObject(memdc, hfont);
    SetTextColor(memdc, RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
    SetBkMode(memdc, TRANSPARENT);
    TextOutA(memdc, x, y, text, strlen(text));
    SelectObject(memdc, oldfont);

    return 1;
}

int draw_text_bg(const char *text, int x, int y, uint32_t fg_color, uint32_t bg_color) {
    if (!hfont) return 0;

    HFONT oldfont = SelectObject(memdc, hfont);
    SIZE size;
    GetTextExtentPoint32A(memdc, text, strlen(text), &size);

    TEXTMETRICA tm;
    GetTextMetricsA(memdc, &tm);

    Rect bg_rect = {x, y, size.cx, tm.tmHeight};
    draw_rect(&bg_rect, bg_color);

    SetTextColor(memdc, RGB((fg_color >> 16) & 0xFF, (fg_color >> 8) & 0xFF, fg_color & 0xFF));
    SetBkMode(memdc, TRANSPARENT);
    TextOutA(memdc, x, y, text, strlen(text));
    SelectObject(memdc, oldfont);

    return 1;
}
