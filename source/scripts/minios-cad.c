/* Ctrl+Alt+Delete: Refresh desktop. Close user apps and free RAM. Do not kill X. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static int x_error(Display *dpy, XErrorEvent *ev)
{
    (void)dpy;
    (void)ev;
    return 0;
}

static int take_lock(void)
{
    if (mkdir("/tmp/minios-cad.lock", 0700) == 0)
        return 1;
    if (errno == EEXIST)
        return 0;
    return 1;
}

static void drop_lock(void)
{
    rmdir("/tmp/minios-cad.lock");
}

static void paint_status(Display *dpy, Window win, GC gc, int ww, int wh,
                         unsigned long bg, unsigned long fg, unsigned long accent,
                         const char *title, const char *msg)
{
    if (!title || !title[0])
        title = "Please wait.";
    if (!msg || !msg[0])
        msg = "Freeing memory.";

    XSetForeground(dpy, gc, bg);
    XFillRectangle(dpy, win, gc, 0, 0, ww, wh);
    XSetForeground(dpy, gc, accent);
    XFillRectangle(dpy, win, gc, 0, 0, ww, 4);
    /* 9x15 is 15 pixels tall. Keep the lines far enough apart that the
     * second line cannot sit on the first. A 120-pixel window put them
     * on top of each other on the laptop. */
    XSetForeground(dpy, gc, fg);
    XDrawString(dpy, win, gc, 24, 46, title, (int)strlen(title));
    XDrawString(dpy, win, gc, 24, 92, msg, (int)strlen(msg));
    XFlush(dpy);
}

static int auto_reclaim_on(void)
{
    FILE *f = fopen("/home/tc/.minios/auto-reclaim", "r");
    char buf[16];

    if (!f)
        return 1;
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return 1;
    }
    fclose(f);
    return strncmp(buf, "off", 3) != 0;
}

static void set_auto_reclaim(int on)
{
    FILE *f;

    mkdir("/home/tc/.minios", 0755);
    f = fopen("/home/tc/.minios/auto-reclaim", "w");
    if (!f)
        return;
    fputs(on ? "on\n" : "off\n", f);
    fclose(f);
}

static void do_refresh(void)
{
    const char *cmd = "/opt/minios/minios-refresh-desktop";

    if (access(cmd, X_OK) != 0)
        cmd = "/usr/local/bin/minios-refresh-desktop";
    if (access(cmd, X_OK) != 0)
        cmd = "/opt/minios/minios-refresh-ram";
    system(cmd);
}

static int show_menu(void)
{
    Display *dpy;
    Window win;
    GC gc;
    XEvent ev;
    int scr, dw, dh, ww = 420, wh = 230, hover = -1, reclaim_on;
    unsigned long bg, fg, accent, btn, hi;
    const char *labs[2] = { "Refresh desktop", "Cancel" };
    int by[2], cb_y = 72;

    /* A leftover lock from a crashed menu must not block CAD. */
    rmdir("/tmp/minios-cad.lock");
    if (!take_lock())
        return 0;
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        drop_lock();
        return 1;
    }
    XSetErrorHandler(x_error);
    scr = DefaultScreen(dpy);
    dw = DisplayWidth(dpy, scr);
    dh = DisplayHeight(dpy, scr);
    bg = BlackPixel(dpy, scr);
    fg = WhitePixel(dpy, scr);
    {
        XColor c, e;
        Colormap cm = DefaultColormap(dpy, scr);
        if (XAllocNamedColor(dpy, cm, "SteelBlue", &c, &e))
            accent = c.pixel;
        else
            accent = fg;
        if (XAllocNamedColor(dpy, cm, "gray20", &c, &e))
            btn = c.pixel;
        else
            btn = bg;
        if (XAllocNamedColor(dpy, cm, "gray35", &c, &e))
            hi = c.pixel;
        else
            hi = accent;
    }
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr),
                              (dw - ww) / 2, (dh - wh) / 2, ww, wh, 0, fg, bg);
    XStoreName(dpy, win, "Refresh desktop");
    {
        XSetWindowAttributes swa;
        swa.override_redirect = True;
        swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                         PointerMotionMask | KeyPressMask;
        XChangeWindowAttributes(dpy, win, CWOverrideRedirect | CWEventMask, &swa);
    }
    gc = XCreateGC(dpy, win, 0, NULL);
    XMapRaised(dpy, win);
    reclaim_on = auto_reclaim_on();
    by[0] = 118;
    by[1] = 162;
    for (;;) {
        int i;
        const char *cblab = "Free RAM when an app closes";
        XSetForeground(dpy, gc, bg);
        XFillRectangle(dpy, win, gc, 0, 0, ww, wh);
        XSetForeground(dpy, gc, accent);
        XFillRectangle(dpy, win, gc, 0, 0, ww, 4);
        XSetForeground(dpy, gc, fg);
        XDrawString(dpy, win, gc, 16, 28, "Refresh desktop", 15);
        XDrawString(dpy, win, gc, 16, 52, "Close open apps and free RAM.", 29);
        XSetForeground(dpy, gc, fg);
        XDrawRectangle(dpy, win, gc, 20, cb_y, 16, 16);
        if (reclaim_on) {
            XDrawLine(dpy, win, gc, 23, cb_y + 8, 27, cb_y + 13);
            XDrawLine(dpy, win, gc, 27, cb_y + 13, 34, cb_y + 3);
        }
        XDrawString(dpy, win, gc, 44, cb_y + 13, cblab, (int)strlen(cblab));
        for (i = 0; i < 2; i++) {
            XSetForeground(dpy, gc, (i == hover) ? hi : btn);
            XFillRectangle(dpy, win, gc, 20, by[i], ww - 40, 32);
            XSetForeground(dpy, gc, fg);
            XDrawRectangle(dpy, win, gc, 20, by[i], ww - 40, 32);
            XDrawString(dpy, win, gc, 36, by[i] + 21, labs[i], (int)strlen(labs[i]));
        }
        XFlush(dpy);
        XNextEvent(dpy, &ev);
        if (ev.type == MotionNotify) {
            int y = ev.xmotion.y, n = -1;
            for (i = 0; i < 2; i++)
                if (y >= by[i] && y < by[i] + 32)
                    n = i;
            hover = n;
        } else if (ev.type == ButtonRelease) {
            int x = ev.xbutton.x, y = ev.xbutton.y;
            if (y >= cb_y - 4 && y < cb_y + 22 && x >= 16 && x < ww - 16) {
                reclaim_on = !reclaim_on;
                set_auto_reclaim(reclaim_on);
                continue;
            }
            if (y >= by[0] && y < by[0] + 32) {
                paint_status(dpy, win, gc, ww, wh, bg, fg, accent,
                             "Please wait.",
                             "Closing applications and freeing memory.");
                do_refresh();
                break;
            }
            if (y >= by[1] && y < by[1] + 32)
                break;
        } else if (ev.type == KeyPress) {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            if (ks == XK_Return || ks == XK_KP_Enter || ks == XK_space) {
                paint_status(dpy, win, gc, ww, wh, bg, fg, accent,
                             "Please wait.",
                             "Closing applications and freeing memory.");
                do_refresh();
                break;
            }
            if (ks == XK_Escape || ks == XK_q || ks == XK_Q)
                break;
        }
    }
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    drop_lock();
    return 0;
}

static void grab_cad_keys(Display *dpy, Window root)
{
    unsigned mods[8];
    KeyCode del, kpd, bs;
    int i;

    mods[0] = ControlMask | Mod1Mask;
    mods[1] = ControlMask | Mod1Mask | LockMask;
    mods[2] = ControlMask | Mod1Mask | Mod2Mask;
    mods[3] = ControlMask | Mod1Mask | LockMask | Mod2Mask;
    mods[4] = ControlMask | Mod1Mask | Mod5Mask;
    mods[5] = ControlMask | Mod1Mask | Mod2Mask | Mod5Mask;
    mods[6] = ControlMask | Mod1Mask | Mod4Mask;
    mods[7] = ControlMask | Mod1Mask | LockMask | Mod2Mask | Mod5Mask;
    del = XKeysymToKeycode(dpy, XK_Delete);
    kpd = XKeysymToKeycode(dpy, XK_KP_Delete);
    bs = XKeysymToKeycode(dpy, XK_BackSpace);
    for (i = 0; i < 8; i++) {
        if (del)
            XGrabKey(dpy, del, mods[i], root, True, GrabModeAsync, GrabModeAsync);
        if (kpd)
            XGrabKey(dpy, kpd, mods[i], root, True, GrabModeAsync, GrabModeAsync);
        /* Ctrl+Alt+Backspace otherwise kills Xvesa (looks like a DOS console). */
        if (bs)
            XGrabKey(dpy, bs, mods[i], root, True, GrabModeAsync, GrabModeAsync);
    }
}

static int watch(void)
{
    Display *dpy = NULL;
    Window root;
    XEvent ev;
    int tries;

    for (tries = 0; tries < 40 && dpy == NULL; tries++) {
        dpy = XOpenDisplay(NULL);
        if (dpy == NULL)
            sleep(1);
    }
    if (!dpy)
        return 1;
    root = DefaultRootWindow(dpy);
    grab_cad_keys(dpy, root);
    for (;;) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            if (fork() == 0) {
                execl("/opt/minios/minios-cad", "minios-cad", (char *)0);
                execl("/usr/local/bin/minios-cad", "minios-cad", (char *)0);
                _exit(127);
            }
        }
    }
}

static int show_notice(const char *title, const char *msg)
{
    Display *dpy;
    Window win;
    GC gc;
    int scr, dw, dh, ww = 520, wh = 150;
    unsigned long bg, fg, accent;
    FILE *pf;
    int i;

    if (!title || !title[0])
        title = "Please wait.";
    if (!msg || !msg[0])
        msg = "Freeing memory.";
    unlink("/tmp/minios-notice.done");
    dpy = XOpenDisplay(NULL);
    if (!dpy)
        return 1;
    XSetErrorHandler(x_error);
    scr = DefaultScreen(dpy);
    dw = DisplayWidth(dpy, scr);
    dh = DisplayHeight(dpy, scr);
    bg = BlackPixel(dpy, scr);
    fg = WhitePixel(dpy, scr);
    {
        XColor c, e;
        Colormap cm = DefaultColormap(dpy, scr);
        if (XAllocNamedColor(dpy, cm, "SteelBlue", &c, &e))
            accent = c.pixel;
        else
            accent = fg;
    }
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr),
                              (dw - ww) / 2, (dh - wh) / 2, ww, wh, 0, fg, bg);
    XStoreName(dpy, win, title);
    {
        XSetWindowAttributes swa;
        swa.override_redirect = True;
        swa.event_mask = ExposureMask | KeyPressMask;
        XChangeWindowAttributes(dpy, win, CWOverrideRedirect | CWEventMask, &swa);
    }
    gc = XCreateGC(dpy, win, 0, NULL);
    {
        XFontStruct *fs = XLoadQueryFont(dpy, "9x15");
        if (!fs)
            fs = XLoadQueryFont(dpy, "fixed");
        if (fs)
            XSetFont(dpy, gc, fs->fid);
    }
    XMapRaised(dpy, win);
    /* Do not focus before the window is on screen. That BadMatch used to
     * kill this notice, same as the TikTok window. */
    pf = fopen("/tmp/minios-notice.pid", "w");
    if (pf) {
        fprintf(pf, "%d\n", (int)getpid());
        fclose(pf);
    }
    paint_status(dpy, win, gc, ww, wh, bg, fg, accent, title, msg);
    for (i = 0; i < 900; i++) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                if (access("/tmp/minios-notice.done", F_OK) == 0)
                    paint_status(dpy, win, gc, ww, wh, bg, fg, accent,
                                 title, "Memory freed.");
                else
                    paint_status(dpy, win, gc, ww, wh, bg, fg, accent,
                                 title, msg);
            }
        }
        if (access("/tmp/minios-notice.done", F_OK) == 0) {
            char done[80];
            FILE *rf = fopen("/tmp/minios-notice.result", "r");
            snprintf(done, sizeof(done), "Memory freed.");
            if (rf) {
                if (fgets(done, sizeof(done), rf)) {
                    size_t L = strlen(done);
                    while (L > 0 && (done[L - 1] == '\n' || done[L - 1] == '\r'))
                        done[--L] = 0;
                }
                fclose(rf);
            }
            paint_status(dpy, win, gc, ww, wh, bg, fg, accent,
                         title, done[0] ? done : "Memory freed.");
            usleep(600000);
            break;
        }
        usleep(100000);
    }
    unlink("/tmp/minios-notice.pid");
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

static int run_refresh_now(void)
{
    Display *dpy;
    Window win;
    GC gc;
    int scr, dw, dh, ww = 520, wh = 160;
    unsigned long bg, fg, accent;

    rmdir("/tmp/minios-cad.lock");
    if (!take_lock())
        return 0;
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        drop_lock();
        do_refresh();
        return 0;
    }
    XSetErrorHandler(x_error);
    scr = DefaultScreen(dpy);
    dw = DisplayWidth(dpy, scr);
    dh = DisplayHeight(dpy, scr);
    bg = BlackPixel(dpy, scr);
    fg = WhitePixel(dpy, scr);
    {
        XColor c, e;
        Colormap cm = DefaultColormap(dpy, scr);
        if (XAllocNamedColor(dpy, cm, "SteelBlue", &c, &e))
            accent = c.pixel;
        else
            accent = fg;
    }
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr),
                              (dw - ww) / 2, (dh - wh) / 2, ww, wh, 0, fg, bg);
    XStoreName(dpy, win, "Please wait.");
    {
        XSetWindowAttributes swa;
        swa.override_redirect = True;
        swa.event_mask = ExposureMask;
        XChangeWindowAttributes(dpy, win, CWOverrideRedirect | CWEventMask, &swa);
    }
    gc = XCreateGC(dpy, win, 0, NULL);
    {
        XFontStruct *fs = XLoadQueryFont(dpy, "9x15");
        if (!fs)
            fs = XLoadQueryFont(dpy, "fixed");
        if (fs)
            XSetFont(dpy, gc, fs->fid);
    }
    XMapRaised(dpy, win);
    paint_status(dpy, win, gc, ww, wh, bg, fg, accent,
                 "Please wait.", "Closing applications and freeing memory.");
    do_refresh();
    paint_status(dpy, win, gc, ww, wh, bg, fg, accent,
                 "Please wait.", "Memory freed.");
    usleep(600000);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    drop_lock();
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--watch") == 0)
        return watch();
    if (argc > 1 && strcmp(argv[1], "--refresh") == 0)
        return run_refresh_now();
    if (argc > 1 && strcmp(argv[1], "--notice") == 0) {
        const char *title = (argc > 2 && argv[2][0]) ? argv[2] : "Please wait.";
        const char *msg = (argc > 3 && argv[3][0]) ? argv[3]
            : "Freeing memory.";
        return show_notice(title, msg);
    }
    return show_menu();
}
