/* Paints the video name on its own strip, over the left of the player title.
   The player draws a fixed sentence. This window is not that program.
   The right side stays clear for the window buttons. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int xerr(Display *dpy, XErrorEvent *ev)
{
    (void)dpy;
    (void)ev;
    return 0;
}

static void read_title(char *dst, int n)
{
    FILE *f;
    int i = 0, c;

    dst[0] = 0;
    if (n < 2)
        return;
    f = fopen("/tmp/minios-tt-title", "r");
    if (!f)
        return;
    while (i < n - 1 && (c = fgetc(f)) != EOF) {
        if (c == '\n' || c == '\r' || c == '\t')
            break;
        if (c == '"')
            c = '\'';
        dst[i++] = (char)c;
    }
    dst[i] = 0;
    fclose(f);
}

static int text_px(XFontSet fs, XFontStruct *font, const char *s, int len)
{
    if (fs) {
        XRectangle ink, logical;

        Xutf8TextExtents(fs, s, len, &ink, &logical);
        return logical.width;
    }
    if (font)
        return XTextWidth(font, s, len);
    return len * 6;
}

static void fit_line(char *s, XFontSet fs, XFontStruct *font, int maxw)
{
    int full = (int)strlen(s);
    int len = full;

    if (maxw < 24)
        maxw = 24;
    while (len > 0 && text_px(fs, font, s, len) > maxw) {
        len--;
        while (len > 0 && ((unsigned char)s[len] & 0xc0) == 0x80)
            len--;
    }
    if (len < full && len >= 3) {
        s[len - 3] = '.';
        s[len - 2] = '.';
        s[len - 1] = '.';
    }
    if (len < 0)
        len = 0;
    s[len] = 0;
}

static int name_has_tiktok(Display *dpy, Window w)
{
    char *name = NULL;
    Atom net, type;
    int fmt, ok = 0;
    unsigned long nitems, after;
    unsigned char *prop = NULL;

    if (XFetchName(dpy, w, &name) && name) {
        if (strstr(name, "TikTok Player"))
            ok = 1;
        XFree(name);
        if (ok)
            return 1;
    }
    net = XInternAtom(dpy, "_NET_WM_NAME", False);
    if (!net)
        return 0;
    if (XGetWindowProperty(dpy, w, net, 0, 64, False, AnyPropertyType,
                           &type, &fmt, &nitems, &after, &prop) == Success &&
        prop) {
        if (strstr((char *)prop, "TikTok Player"))
            ok = 1;
        XFree(prop);
    }
    return ok;
}

static int find_title(Display *dpy, Window *out, int *x, int *y, int *w, int *h)
{
    Window root, root_r, parent, *kids = NULL, best = 0;
    unsigned nk = 0, i;
    int best_y = 100000;

    *out = 0;
    root = DefaultRootWindow(dpy);
    if (!XQueryTree(dpy, root, &root_r, &parent, &kids, &nk))
        return 0;
    for (i = 0; i < nk; i++) {
        XWindowAttributes a;

        if (!XGetWindowAttributes(dpy, kids[i], &a))
            continue;
        if (a.map_state != IsViewable || a.class != InputOutput)
            continue;
        if (a.height < 20 || a.height > 50 || a.width < 300)
            continue;
        if (!name_has_tiktok(dpy, kids[i]))
            continue;
        if (a.y < best_y) {
            best = kids[i];
            best_y = a.y;
            *x = a.x;
            *y = a.y;
            *w = a.width;
            *h = a.height;
        }
    }
    if (kids)
        XFree(kids);
    *out = best;
    return best != 0;
}

static void paint_bar(Display *dpy, Window win, GC gc, XFontSet fs, XFontStruct *font,
                      int width, int height, unsigned long navy, unsigned long white)
{
    char title[180];
    char line[360];
    int len, baseline, maxw;

    read_title(title, (int)sizeof title);
    if (title[0])
        snprintf(line, sizeof line,
                 "MiniOS TikTok Player - Now playing \"%s\"", title);
    else
        snprintf(line, sizeof line, "MiniOS TikTok Player - Now playing");
    maxw = width - 16;
    if (maxw < 40)
        maxw = 40;
    fit_line(line, fs, font, maxw);
    len = (int)strlen(line);
    baseline = (height + (font ? font->ascent : 11) - (font ? font->descent : 2)) / 2;
    if (baseline < 12)
        baseline = 12;
    XSetForeground(dpy, gc, navy);
    XFillRectangle(dpy, win, gc, 0, 0, (unsigned)width, (unsigned)height);
    XSetForeground(dpy, gc, white);
    if (fs)
        Xutf8DrawString(dpy, win, fs, gc, 12, baseline, line, len);
    else if (font)
        XDrawString(dpy, win, gc, 12, baseline, line, len);
}

int main(void)
{
    Display *dpy;
    GC gc;
    Window bar = 0;
    XFontSet fs = NULL;
    XFontStruct *font = NULL;
    XSetWindowAttributes at;
    XColor c;
    Colormap cm;
    unsigned long navy, white;
    char **missing = NULL;
    int nmiss = 0;
    char *def = NULL;
    int scr, bw = 400, bh = 36;
    int logged = 0;
    int announced = 0;
    int px = -9999, py = -9999;
    Window parent_win = 0;
    char shown[180];

    shown[0] = 0;
    setsid();
    signal(SIGPIPE, SIG_IGN);
    dpy = XOpenDisplay(NULL);
    if (!dpy)
        return 0;
    XSetErrorHandler(xerr);
    scr = DefaultScreen(dpy);
    cm = DefaultColormap(dpy, scr);
    c.flags = DoRed | DoGreen | DoBlue;
    c.red = 22 * 257;
    c.green = 30 * 257;
    c.blue = 48 * 257;
    if (!XAllocColor(dpy, cm, &c))
        navy = BlackPixel(dpy, scr);
    else
        navy = c.pixel;
    c.red = c.green = c.blue = 255 * 257;
    if (!XAllocColor(dpy, cm, &c))
        white = WhitePixel(dpy, scr);
    else
        white = c.pixel;
    fs = XCreateFontSet(dpy,
                        "-*-helvetica-bold-r-normal--13-*-*-*-*-*-*-*,fixed",
                        &missing, &nmiss, &def);
    if (missing)
        XFreeStringList(missing);
    font = XLoadQueryFont(dpy, "-*-helvetica-bold-r-normal--13-*-*-*-*-*-*-*");
    if (!font)
        font = XLoadQueryFont(dpy, "fixed");
    at.override_redirect = True;
    at.background_pixel = navy;
    at.event_mask = ExposureMask;
    bar = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, 400, 36, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWOverrideRedirect | CWBackPixel | CWEventMask, &at);
    gc = XCreateGC(dpy, bar, 0, NULL);
    if (font)
        XSetFont(dpy, gc, font->fid);
    for (;;) {
        Window found = 0;
        int x = 0, y = 0, w = 0, h = 0;
        char clipname[180];
        struct timespec ts;
        int nw;

        if (access("/tmp/minios-tt-mp.quit", F_OK) == 0)
            break;
        if (getppid() == 1)
            break;
        while (XPending(dpy)) {
            XEvent ev;

            XNextEvent(dpy, &ev);
            if (ev.type == Expose && ev.xexpose.count == 0)
                shown[0] = 0;
        }
        if (find_title(dpy, &found, &x, &y, &w, &h) && bar) {
            nw = w - 168;
            if (nw < 180)
                nw = w > 80 ? w - 40 : 180;
            if (h < 24)
                h = 36;
            if (found != parent_win) {
                XReparentWindow(dpy, bar, found, 0, 0);
                parent_win = found;
                px = -9999;
                shown[0] = 0;
            }
            if (nw != bw || h != bh) {
                px = x;
                py = y;
                bw = nw;
                bh = h;
                XMoveResizeWindow(dpy, bar, 0, 0, (unsigned)bw, (unsigned)bh);
                shown[0] = 0;
            }
            if (!logged) {
                fprintf(stderr, "tt-now on %d,%d %dx%d\n", x, y, bw, bh);
                logged = 1;
            }
            XMapRaised(dpy, bar);
            read_title(clipname, (int)sizeof clipname);
            if (strcmp(shown, clipname) != 0) {
                snprintf(shown, sizeof shown, "%s", clipname);
                paint_bar(dpy, bar, gc, fs, font, bw, bh, navy, white);
            }
        } else if (!announced) {
            fprintf(stderr, "tt-now waiting for the player title\n");
            announced = 1;
        }
        XFlush(dpy);
        ts.tv_sec = 1;
        ts.tv_nsec = 0;
        nanosleep(&ts, NULL);
    }
    if (bar)
        XDestroyWindow(dpy, bar);
    XFreeGC(dpy, gc);
    if (fs)
        XFreeFontSet(dpy, fs);
    if (font)
        XFreeFont(dpy, font);
    XCloseDisplay(dpy);
    return 0;
}
