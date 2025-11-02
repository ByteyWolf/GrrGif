#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/cursorfont.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "graphics.h"

GC gc;
Window win;
Display *dpy;
uint32_t wwidth;
uint32_t wheight;

XftDraw *xft_draw = NULL;
XftFont *xft_font = NULL;

int init_graphics(uint32_t width, uint32_t height) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) return 0;

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    win = XCreateSimpleWindow(dpy, root, 100, 100, width, height, 1, BlackPixel(dpy, screen), WhitePixel(dpy, screen));
    XSelectInput(dpy, win,
    StructureNotifyMask | KeyPressMask | KeyReleaseMask |
    ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XSetWindowAttributes attrs;
    attrs.background_pixmap = None;
    XChangeWindowAttributes(dpy, win, CWBackPixmap, &attrs);
    XSizeHints hints;
    hints.flags = PMinSize;
    hints.min_width = 400;
    hints.min_height = 300;
    XSetWMNormalHints(dpy, win, &hints);
    XStoreName(dpy, win, "GrrGif - [no project]");


    wwidth = width;
    wheight = height;
    XMapWindow(dpy, win);
    XFlush(dpy);
    
    gc = XCreateGC(dpy, win, 0, NULL);
    xft_draw = XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
    if (!xft_draw) return 0;

    char font_str[128];
    snprintf(font_str, sizeof(font_str), "monospace-%d", FONT_SIZE_NORMAL > 0 ? FONT_SIZE_NORMAL : 12);

    xft_font = XftFontOpenName(dpy, screen, font_str);
    if (!xft_font) xft_font = XftFontOpenName(dpy, screen, "monospace-12");
    
    return 1;
}

int poll_event(Event *event) {
    XEvent xevent;

    if (XPending(dpy) > 0) {
        XNextEvent(dpy, &xevent);
        switch (xevent.type) {
            case KeyPress:
                event->type = EVENT_KEYDOWN;
                event->key = XLookupKeysym(&xevent.xkey, 0);
                break;
            case KeyRelease:
                event->type = EVENT_KEYUP;
                event->key = XLookupKeysym(&xevent.xkey, 0);
                break;
            case MotionNotify:
                event->type = EVENT_MOUSEMOVE;
                event->x = xevent.xmotion.x;
                event->y = xevent.xmotion.y;
                break;
            case ButtonPress:
                event->type = EVENT_MOUSEBUTTONDOWN;
                event->x = xevent.xbutton.x;
                event->y = xevent.xbutton.y;
                break;
            case ButtonRelease:
                event->type = EVENT_MOUSEBUTTONUP;
                event->x = xevent.xbutton.x;
                event->y = xevent.xbutton.y;
                break;
            case ClientMessage:
                event->type = EVENT_QUIT;
                break;
            case ConfigureNotify:
                if (xevent.xconfigure.width != wwidth ||
                    xevent.xconfigure.height != wheight) {
                    event->type = EVENT_RESIZE; // Add this to your Event enum
                    event->width = xevent.xconfigure.width;
                event->height = xevent.xconfigure.height;
                wwidth = event->width;
                wheight = event->height;
                    }
                    break;
        }
    } else {
        return 0;
    }

    return 1;
}

int set_font_size(int font_size) {
    if (!dpy || !xft_draw) return 0;
    if (xft_font) XftFontClose(dpy, xft_font);
    int screen = DefaultScreen(dpy);
    char font_str[128];
    snprintf(font_str, sizeof(font_str), "monospace-%d", font_size);
    xft_font = XftFontOpenName(dpy, screen, font_str);
    if (!xft_font) xft_font = XftFontOpenName(dpy, screen, "monospace-12");
    return 1;
}

int draw_rect(Rect *rect, uint32_t color) {
    XSetForeground(dpy, gc, color);
    XFillRectangle(dpy, win, gc, rect->x, rect->y, rect->width, rect->height);
    return 1;
}

int clear_graphics(uint32_t color) {
    XSetForeground(dpy, gc, color);
    XFillRectangle(dpy, win, gc, 0, 0, wwidth, wheight);
    return 1;
}

int blit_rgb8888(uint32_t *pixels, uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
    XImage *img = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultDepth(dpy, DefaultScreen(dpy)), ZPixmap, 0, (char *)pixels, width, height, 32, 0);
    if (!img) return 0;
    XPutImage(dpy, win, gc, img, 0, 0, x, y, width, height);
    img->data = NULL;
    XDestroyImage(img);
    return 1;
}

/*int get_rgb8888(uint32_t *destbuf, uint32_t width, uint32_t height) {
    XImage *img = XGetImage(dpy, win, 0, 0, width, height, AllPlanes, ZPixmap);
    if (!img) return 0;
    if (img->bits_per_pixel != 32) {
        XDestroyImage(img);
        return 0;
    }
    memcpy(destbuf, img->data, width * height * 4);
    XDestroyImage(img);
    return 1;
}*/

int flush_graphics() {
    XFlush(dpy);
    return 1;
}

int close_graphics() {
    if (xft_draw) XftDrawDestroy(xft_draw);
    if (xft_font) XftFontClose(dpy, xft_font);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 1;
}

int draw_text(const char *text, int x, int y, uint32_t color) {
    if (!xft_draw || !xft_font) return 0;
    XRenderColor xrcolor = {(color >> 16 & 0xFF) * 257, (color >> 8 & 0xFF) * 257, (color & 0xFF) * 257, 0xFFFF};
    XftColor xft_color;
    XftColorAllocValue(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)), &xrcolor, &xft_color);
    XftDrawStringUtf8(xft_draw, &xft_color, xft_font, x, y, (const FcChar8 *)text, strlen(text));
    XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)), &xft_color);
    return 1;
}

int draw_text_bg(const char *text, int x, int y, uint32_t fg_color, uint32_t bg_color) {
    if (!xft_draw || !xft_font) return 0;
    XGlyphInfo extents;
    XftTextExtentsUtf8(dpy, xft_font, (const FcChar8 *)text, strlen(text), &extents);
    int text_width = extents.width;
    int text_height = xft_font->ascent + xft_font->descent;
    draw_rect(&(Rect){x, y - xft_font->ascent, text_width, text_height}, bg_color);
    draw_text(text, x, y, fg_color);
    return 1;
}


void set_cursor(int type) {
    if (!dpy || !win) return;

    unsigned int shape = XC_left_ptr;
    switch(type) {
        case CURSOR_NORMAL: shape = XC_left_ptr; break;
        case CURSOR_BUSY:   shape = XC_watch; break;
        case CURSOR_SIZEH:  shape = XC_sb_h_double_arrow; break;
        case CURSOR_SIZEV:  shape = XC_sb_v_double_arrow; break;
    }

    Cursor cur = XCreateFontCursor(dpy, shape);
    XDefineCursor(dpy, win, cur);
    XFlush(dpy);
    XFreeCursor(dpy, cur);
}

void set_window_title(char *title) {
    XStoreName(dpy, win, title);
    XFlush(dpy);
}
