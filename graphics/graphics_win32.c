#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

/* Event queue */
#define MAX_EVENT_QUEUE 32
static Event event_queue[MAX_EVENT_QUEUE];
static int event_queue_head = 0;
static int event_queue_tail = 0;

/* Helper: Add event to queue */
static void push_event(Event *evt) {
    int next = (event_queue_tail + 1) % MAX_EVENT_QUEUE;
    if (next != event_queue_head) {
        event_queue[event_queue_tail] = *evt;
        event_queue_tail = next;
    }
}

/* Helper: Get event from queue */
static int pop_event(Event *evt) {
    if (event_queue_head == event_queue_tail) {
        return 0;
    }
    *evt = event_queue[event_queue_head];
    event_queue_head = (event_queue_head + 1) % MAX_EVENT_QUEUE;
    return 1;
}

/* Window procedure */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Event evt;

    switch (msg) {
        case WM_KEYDOWN:
            evt.type = EVENT_KEYDOWN;
            evt.key = (int)wParam;
            push_event(&evt);
            return 0;

        case WM_KEYUP:
            evt.type = EVENT_KEYUP;
            evt.key = (int)wParam;
            push_event(&evt);
            return 0;

        case WM_MOUSEMOVE:
            evt.type = EVENT_MOUSEMOVE;
            evt.x = LOWORD(lParam);
            evt.y = HIWORD(lParam);
            push_event(&evt);
            return 0;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            evt.type = EVENT_MOUSEBUTTONDOWN;
            evt.x = LOWORD(lParam);
            evt.y = HIWORD(lParam);
            push_event(&evt);
            return 0;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            evt.type = EVENT_MOUSEBUTTONUP;
            evt.x = LOWORD(lParam);
            evt.y = HIWORD(lParam);
            push_event(&evt);
            return 0;

        case WM_SIZE:
            evt.type = EVENT_RESIZE;
            evt.width = LOWORD(lParam);
            evt.height = HIWORD(lParam);
            wwidth = evt.width;
            wheight = evt.height;

            /* Recreate backing buffer */
            if (memdc) {
                if (membmp) DeleteObject(membmp);
                membmp = CreateCompatibleBitmap(hdc, wwidth, wheight);
                SelectObject(memdc, membmp);
            }

            push_event(&evt);
            return 0;

        case WM_CLOSE:
            evt.type = EVENT_QUIT;
            push_event(&evt);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_ERASEBKGND:
            return 1; /* Prevent flicker */
        case WM_PAINT:
            pendingRedraw = 1;
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int init_graphics(uint32_t width, uint32_t height) {
    WNDCLASS wc;
    HINSTANCE hInstance;
    RECT rect;

    hInstance = GetModuleHandle(NULL);

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

int poll_event(Event *event) {
    MSG msg;

    /* Process all pending Windows messages */
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            Event evt;
            evt.type = EVENT_QUIT;
            push_event(&evt);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    /* Return queued event if available */
    return pop_event(event);
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
