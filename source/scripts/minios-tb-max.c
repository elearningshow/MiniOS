/* Move Text Browser to the same screen rectangle YouTube uses.
 * flwm keeps minimize and close on the left when x is 40. */
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int titled(Display *d, Window w)
{
    char *n = NULL;
    int ok = 0;

    if (XFetchName(d, w, &n) && n) {
        if (strstr(n, "MiniOS Text Browser"))
            ok = 1;
        XFree(n);
    }
    return ok;
}

static Window find_win(Display *d, Window root)
{
    Window rr, p, *ch = NULL, found = 0;
    unsigned n = 0, i;

    if (titled(d, root))
        return root;
    if (!XQueryTree(d, root, &rr, &p, &ch, &n) || !ch)
        return 0;
    for (i = 0; i < n && !found; i++)
        found = find_win(d, ch[i]);
    XFree(ch);
    return found;
}

int main(void)
{
    Display *d;
    Window w;
    int sw, sh, ww, hh, tries;

    d = XOpenDisplay(NULL);
    if (!d)
        return 1;
    w = 0;
    for (tries = 0; tries < 20 && !w; tries++) {
        w = find_win(d, DefaultRootWindow(d));
        if (!w)
            usleep(50000);
    }
    if (!w) {
        XCloseDisplay(d);
        return 1;
    }
    sw = DisplayWidth(d, DefaultScreen(d));
    sh = DisplayHeight(d, DefaultScreen(d));
    ww = sw - 40 - 4;
    hh = sh - 56 - 4;
    if (ww < 200)
        ww = 200;
    if (hh < 200)
        hh = 200;
    XMoveResizeWindow(d, w, 40, 4, (unsigned)ww, (unsigned)hh);
    XFlush(d);
    XCloseDisplay(d);
    return 0;
}
