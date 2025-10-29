#include <X11/Xlib.h>
#include <stdint.h>
#include <stdio.h>
#include "graphics.h"

GC gc;
Window win;
Display *dpy;
uint32_t wwidth;
uint32_t wheight;


int init_graphics(uint32_t width, uint32_t height) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        perror("Failed to open display\n");
        return 0;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    win = XCreateSimpleWindow(dpy, root,
                                     100, 100, width, height, 1,
                                     BlackPixel(dpy, screen),
                                     WhitePixel(dpy, screen));
    wwidth = width;
    wheight = height;

    XMapWindow(dpy, win);
    XFlush(dpy);

    gc = XCreateGC(dpy, win, 0, NULL);
    return 1;
}

int draw_rect(Rect *rect, uint32_t color) {
    XSetForeground(dpy, gc, color);
    XFillRectangle(dpy, win, gc, rect->x, rect->y, rect->width, rect->height);
    return 1;
}

int blit_rgb8888(uint32_t *pixels, uint32_t width, uint32_t height) {
    XImage *img = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
                               DefaultDepth(dpy, DefaultScreen(dpy)),
                               ZPixmap, 0, (char *)pixels,
                               width, height, 32, 0);
    if (!img) {
        perror("Failed to create XImage\n");
        return 0;
    }

    XPutImage(dpy, win, gc, img, 0, 0, 0, 0, width, height);
    img->data = NULL; // Prevent XDestroyImage from freeing pixels
    XDestroyImage(img);
    return 1;
}

int get_rgb8888(uint32_t *destbuf, uint32_t width, uint32_t height) {
    XImage *img = XGetImage(dpy, win, 0, 0, width, height, AllPlanes, ZPixmap);
    if (!img) {
        perror("Failed to get XImage\n");
        return 0;
    }

    if (img->bits_per_pixel != 32) {
        fprintf(stderr, "Unexpected image depth: %d bpp\n", img->bits_per_pixel);
        XDestroyImage(img);
        return 0;
    }

    memcpy(destbuf, img->data, width * height * 4);

    XDestroyImage(img);
    return 1;
}


int flush_graphics() {
    XFlush(dpy);
    return 1;
}

int close_graphics() {
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 1;
}

Event poll_event() {
    XEvent xevent;
    Event event = {0, 0, 0, 0};

    if (XPending(dpy) > 0) {
        XNextEvent(dpy, &xevent);
        switch (xevent.type) {
            case KeyPress:
                event.type = EVENT_KEYDOWN;
                event.key = XLookupKeysym(&xevent.xkey, 0);
                break;
            case KeyRelease:
                event.type = EVENT_KEYUP;
                event.key = XLookupKeysym(&xevent.xkey, 0);
                break;
            case MotionNotify:
                event.type = EVENT_MOUSEMOVE;
                event.x = xevent.xmotion.x;
                event.y = xevent.xmotion.y;
                break;
            case ButtonPress:
                event.type = EVENT_MOUSEBUTTONDOWN;
                event.x = xevent.xbutton.x;
                event.y = xevent.xbutton.y;
                break;
            case ButtonRelease:
                event.type = EVENT_MOUSEBUTTONUP;
                event.x = xevent.xbutton.x;
                event.y = xevent.xbutton.y;
                break;
            case ClientMessage:
                event.type = EVENT_QUIT;
                break;
        }
    }

    return event;
}

int clear_graphics(uint32_t color) {
    XSetForeground(dpy, gc, color);
    XFillRectangle(dpy, win, gc, 0, 0, wwidth, wheight);
    return 1;
}