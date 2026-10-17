/* Stretch the Nightly window to the right edge of the screen.
 * Leave the bottom dock clear. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int is_firefox(Display *d, Window w)
{
    XClassHint hint;
    int ok = 0;

    memset(&hint, 0, sizeof(hint));
    if (XGetClassHint(d, w, &hint)) {
        if ((hint.res_class && strstr(hint.res_class, "irefox")) ||
            (hint.res_name && strstr(hint.res_name, "avigator")))
            ok = 1;
        if (hint.res_name)
            XFree(hint.res_name);
        if (hint.res_class)
            XFree(hint.res_class);
    }
    return ok;
}

static Window find_win(Display *d, Window root)
{
    Window rr, p, *ch = NULL, found = 0;
    unsigned n = 0, i;

    if (is_firefox(d, root))
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
    int sw, sh, tries;

    d = XOpenDisplay(NULL);
    if (!d)
        return 1;
    w = 0;
    for (tries = 0; tries < 40 && !w; tries++) {
        w = find_win(d, DefaultRootWindow(d));
        if (!w)
            usleep(250000);
    }
    if (!w) {
        XCloseDisplay(d);
        return 1;
    }
    sw = DisplayWidth(d, DefaultScreen(d));
    sh = DisplayHeight(d, DefaultScreen(d));
    if (sh > 56)
        sh -= 56;
    XMoveResizeWindow(d, w, 0, 0, (unsigned)sw, (unsigned)sh);
    XFlush(d);
    XCloseDisplay(d);
    return 0;
}
