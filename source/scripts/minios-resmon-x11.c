/* RESOURCE-MON-AP-01: borderless CPU/RAM/SWAP bar. No GPU/VRAM.
 * Hover shows Close and Minimize. Close exits only this process. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

#define BAR_H 20
#define BTN 16
#define GAP 4
#define FULL_W 380
#define MINI_W 28
#define MARGIN 8

static Display *dpy;
static Window win;
static GC gc;
static int scr;
static int hover;
static int minimized;
static int leave_pending;
static int dragging;
static int drag_off_x, drag_off_y;
static int win_x, win_y;
static Cursor move_cur;
static struct timeval leave_at;
static char line[96];
static unsigned long last_cpu_tot, last_cpu_idle;
static int cpu_pct, ram_pct, zram_pct, swap_pct;
static int have_zram, have_swap;

static unsigned long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)tv.tv_sec * 1000UL + (unsigned long)tv.tv_usec / 1000UL;
}

static void read_stats(void)
{
    FILE *f;
    char key[64];
    unsigned long val, mem_total = 0, mem_avail = 0, mem_free = 0;
    unsigned long swap_total = 0, swap_free = 0;
    unsigned long zram_size = 0, zram_used = 0;
    unsigned long disk_size = 0, disk_used = 0;
    unsigned long u = 0, n = 0, s = 0, idle = 0, iw = 0, irq = 0, sirq = 0, st = 0;
    char fname[128], typ[32];
    unsigned long sz = 0, used = 0;
    int pri = 0;

    f = fopen("/proc/meminfo", "r");
    if (f) {
        while (fscanf(f, "%63s %lu %*s", key, &val) == 2) {
            if (strcmp(key, "MemTotal:") == 0) mem_total = val;
            else if (strcmp(key, "MemAvailable:") == 0) mem_avail = val;
            else if (strcmp(key, "MemFree:") == 0) mem_free = val;
            else if (strcmp(key, "SwapTotal:") == 0) swap_total = val;
            else if (strcmp(key, "SwapFree:") == 0) swap_free = val;
        }
        fclose(f);
    }
    if (mem_avail == 0) mem_avail = mem_free;
    if (mem_total > 0) {
        unsigned long used = mem_total - mem_avail;
        ram_pct = (int)((used * 100UL) / mem_total);
    } else {
        ram_pct = 0;
    }
    (void)swap_total;
    (void)swap_free;
    zram_size = zram_used = disk_size = disk_used = 0;
    f = fopen("/proc/swaps", "r");
    if (f) {
        char hdr[128];
        if (fgets(hdr, sizeof(hdr), f)) {
            while (fscanf(f, "%127s %31s %lu %lu %d",
                          fname, typ, &sz, &used, &pri) == 5) {
                if (strstr(fname, "zram") != NULL) {
                    zram_size += sz;
                    zram_used += used;
                } else {
                    disk_size += sz;
                    disk_used += used;
                }
            }
        }
        fclose(f);
    }
    if (zram_size > 0) {
        zram_pct = (int)((zram_used * 100UL) / zram_size);
        have_zram = 1;
    } else {
        zram_pct = 0;
        have_zram = 0;
    }
    if (disk_size > 0) {
        swap_pct = (int)((disk_used * 100UL) / disk_size);
        have_swap = 1;
    } else {
        swap_pct = 0;
        have_swap = 0;
    }

    f = fopen("/proc/stat", "r");
    if (f) {
        if (fscanf(f, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &u, &n, &s, &idle, &iw, &irq, &sirq, &st) >= 4) {
            unsigned long tot = u + n + s + idle + iw + irq + sirq + st;
            unsigned long idl = idle + iw;
            if (last_cpu_tot > 0 && tot > last_cpu_tot) {
                unsigned long dt = tot - last_cpu_tot;
                unsigned long di = idl - last_cpu_idle;
                cpu_pct = (int)((100UL * (dt - di)) / dt);
            }
            last_cpu_tot = tot;
            last_cpu_idle = idl;
        }
        fclose(f);
    }

    if (have_zram && have_swap)
        snprintf(line, sizeof(line),
                 "CPU %d%%  RAM %d%%  ZRAM %d%%  SWAP %d%%",
                 cpu_pct, ram_pct, zram_pct, swap_pct);
    else if (have_zram)
        snprintf(line, sizeof(line),
                 "CPU %d%%  RAM %d%%  ZRAM %d%%  SWAP none",
                 cpu_pct, ram_pct, zram_pct);
    else if (have_swap)
        snprintf(line, sizeof(line),
                 "CPU %d%%  RAM %d%%  ZRAM none  SWAP %d%%",
                 cpu_pct, ram_pct, swap_pct);
    else
        snprintf(line, sizeof(line),
                 "CPU %d%%  RAM %d%%  ZRAM none  SWAP none",
                 cpu_pct, ram_pct);
}

static int win_w(void)
{
    return minimized ? MINI_W : FULL_W;
}

static void paint(void)
{
    int w = win_w();
    XSetForeground(dpy, gc, BlackPixel(dpy, scr));
    XFillRectangle(dpy, win, gc, 0, 0, (unsigned)w, BAR_H);
    XSetForeground(dpy, gc, WhitePixel(dpy, scr));
    if (minimized) {
        XDrawString(dpy, win, gc, 6, 14, "=", 1);
        return;
    }
    XDrawString(dpy, win, gc, 8, 14, line, (int)strlen(line));
    if (hover) {
        int cx = FULL_W - BTN - GAP;
        int mx = cx - BTN - GAP;
        /* minimize */
        XDrawRectangle(dpy, win, gc, mx, 2, BTN - 1, BTN - 1);
        XDrawLine(dpy, win, gc, mx + 3, 10, mx + BTN - 4, 10);
        /* close */
        XDrawRectangle(dpy, win, gc, cx, 2, BTN - 1, BTN - 1);
        XDrawLine(dpy, win, gc, cx + 4, 5, cx + BTN - 5, BTN - 4);
        XDrawLine(dpy, win, gc, cx + BTN - 5, 5, cx + 4, BTN - 4);
    }
}

static void save_pos(void)
{
    FILE *pf;
    const char *home = getenv("HOME");
    char path[256];
    if (!home)
        home = "/home/tc";
    snprintf(path, sizeof(path), "%s/.minios-monitor-top.pos", home);
    pf = fopen(path, "w");
    if (!pf)
        return;
    fprintf(pf, "%d %d\n", win_x, win_y);
    fclose(pf);
}

static int load_pos(int *x, int *y)
{
    FILE *pf;
    const char *home = getenv("HOME");
    char path[256];
    int px = 0, py = 0;
    int dw, dh;
    if (!home)
        home = "/home/tc";
    snprintf(path, sizeof(path), "%s/.minios-monitor-top.pos", home);
    pf = fopen(path, "r");
    if (!pf)
        return 0;
    if (fscanf(pf, "%d %d", &px, &py) != 2) {
        fclose(pf);
        return 0;
    }
    fclose(pf);
    dw = DisplayWidth(dpy, scr);
    dh = DisplayHeight(dpy, scr);
    if (px < -20 || py < -20 || px > dw - 8 || py > dh - 8)
        return 0;
    *x = px;
    *y = py;
    return 1;
}

static void place_bar(void)
{
    int dw = DisplayWidth(dpy, scr);
    int x = (dw - FULL_W) / 2;
    int y = MARGIN;
    if (x < MARGIN)
        x = MARGIN;
    if (load_pos(&x, &y)) {
        /* saved top-center drag */
    }
    win_x = x;
    win_y = y;
    XMoveResizeWindow(dpy, win, x, y, (unsigned)win_w(), BAR_H);
}

static void raise_no_focus(void)
{
    /* Stay above Nightly. Do not call XSetInputFocus. */
    XRaiseWindow(dpy, win);
}

static void apply_size(void)
{
    XMoveResizeWindow(dpy, win, win_x, win_y, (unsigned)win_w(), BAR_H);
}

static int hit_close(int x, int y)
{
    int cx = FULL_W - BTN - GAP;
    return (!minimized && hover && x >= cx && x < cx + BTN && y >= 2 && y < 2 + BTN);
}

static int hit_min(int x, int y)
{
    int mx = FULL_W - 2 * BTN - 2 * GAP;
    return (!minimized && hover && x >= mx && x < mx + BTN && y >= 2 && y < 2 + BTN);
}

int main(void)
{
    XSetWindowAttributes swa;
    XEvent ev;
    int xfd;
    unsigned long last_stat = 0;
    int running = 1;
    int dw;

    dpy = XOpenDisplay(NULL);
    if (!dpy)
        return 1;
    scr = DefaultScreen(dpy);
    dw = DisplayWidth(dpy, scr);
    (void)dw;
    swa.override_redirect = True;
    swa.background_pixel = BlackPixel(dpy, scr);
    swa.event_mask = ExposureMask | EnterWindowMask | LeaveWindowMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     StructureNotifyMask;
    /* Middle of the top edge. */
    win = XCreateWindow(dpy, RootWindow(dpy, scr),
                        (dw - FULL_W) / 2 > MARGIN ? (dw - FULL_W) / 2 : MARGIN,
                        MARGIN,
                        FULL_W, BAR_H, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, WhitePixel(dpy, scr));
    move_cur = XCreateFontCursor(dpy, XC_fleur);
    XDefineCursor(dpy, win, move_cur);
    XStoreName(dpy, win, "MiniOS resources");
    XMapRaised(dpy, win);
    place_bar();
    raise_no_focus();
    read_stats();
    last_stat = now_ms();
    paint();
    XFlush(dpy);

    xfd = ConnectionNumber(dpy);
    while (running) {
        unsigned long t = now_ms();
        struct timeval tv;
        fd_set rfds;
        int timeout_ms = 250;

        if (leave_pending) {
            long remain = (long)((leave_at.tv_sec * 1000L + leave_at.tv_usec / 1000L) - (long)t);
            if (remain <= 0) {
                hover = 0;
                leave_pending = 0;
                paint();
                XFlush(dpy);
            } else if (remain < timeout_ms) {
                timeout_ms = (int)remain;
            }
        }
        if (t - last_stat >= 1500) {
            read_stats();
            last_stat = t;
            paint();
            XFlush(dpy);
        }

        FD_ZERO(&rfds);
        FD_SET(xfd, &rfds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        select(xfd + 1, &rfds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            switch (ev.type) {
            case Expose:
                if (ev.xexpose.count == 0)
                    paint();
                break;
            case EnterNotify:
                hover = 1;
                leave_pending = 0;
                paint();
                break;
            case LeaveNotify:
                if (ev.xcrossing.detail != NotifyInferior) {
                    leave_pending = 1;
                    gettimeofday(&leave_at, NULL);
                    leave_at.tv_usec += 400000;
                    if (leave_at.tv_usec >= 1000000) {
                        leave_at.tv_sec += 1;
                        leave_at.tv_usec -= 1000000;
                    }
                }
                break;
            case ButtonPress:
                if (ev.xbutton.button != Button1)
                    break;
                if (minimized) {
                    minimized = 0;
                    apply_size();
                    paint();
                } else if (hit_close(ev.xbutton.x, ev.xbutton.y)) {
                    running = 0;
                } else if (hit_min(ev.xbutton.x, ev.xbutton.y)) {
                    minimized = 1;
                    hover = 0;
                    apply_size();
                    paint();
                } else {
                    dragging = 1;
                    drag_off_x = ev.xbutton.x;
                    drag_off_y = ev.xbutton.y;
                    XGrabPointer(dpy, win, False,
                                 ButtonReleaseMask | PointerMotionMask,
                                 GrabModeAsync, GrabModeAsync, None,
                                 move_cur, CurrentTime);
                }
                break;
            case MotionNotify:
                if (dragging) {
                    win_x = ev.xmotion.x_root - drag_off_x;
                    win_y = ev.xmotion.y_root - drag_off_y;
                    XMoveWindow(dpy, win, win_x, win_y);
                }
                break;
            case ButtonRelease:
                if (dragging && ev.xbutton.button == Button1) {
                    dragging = 0;
                    XUngrabPointer(dpy, CurrentTime);
                    win_x = ev.xbutton.x_root - drag_off_x;
                    win_y = ev.xbutton.y_root - drag_off_y;
                    XMoveWindow(dpy, win, win_x, win_y);
                    save_pos();
                }
                break;
            default:
                break;
            }
        }
    }
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
