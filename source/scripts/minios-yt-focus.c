/* Focus the MiniOS YouTube Term/URL field. Does not start or change the player. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* FLTube SearchInput is at (110, 63, 450, 24). Click inside the box. */
#define FIELD_X 180
#define FIELD_Y 75

static int name_is_youtube(Display *dpy, Window w)
{
    char *nm = NULL;
    int ok = 0;
    XClassHint ch;

    if (XFetchName(dpy, w, &nm) && nm) {
        if (strstr(nm, "YouTube") || strstr(nm, "FLTube") || strstr(nm, "fltube"))
            ok = 1;
        XFree(nm);
    }
    if (!ok && XGetClassHint(dpy, w, &ch)) {
        if ((ch.res_name && (strstr(ch.res_name, "fltube") || strstr(ch.res_name, "FLTube") ||
                             strstr(ch.res_name, "YouTube"))) ||
            (ch.res_class && (strstr(ch.res_class, "fltube") || strstr(ch.res_class, "FLTube") ||
                              strstr(ch.res_class, "YouTube"))))
            ok = 1;
        if (ch.res_name)
            XFree(ch.res_name);
        if (ch.res_class)
            XFree(ch.res_class);
    }
    return ok;
}

static Window find_youtube(Display *dpy, Window root)
{
    Window rr, p, *ch = NULL;
    unsigned n = 0, i;
    Window found = 0;

    if (name_is_youtube(dpy, root))
        return root;
    if (!XQueryTree(dpy, root, &rr, &p, &ch, &n) || !ch)
        return 0;
    for (i = 0; i < n; i++) {
        found = find_youtube(dpy, ch[i]);
        if (found)
            break;
    }
    XFree(ch);
    return found;
}

static void click_field(Display *dpy, Window w)
{
    XEvent ev;
    Window root = DefaultRootWindow(dpy);
    int rx = FIELD_X, ry = FIELD_Y;
    Window child = 0;

    XTranslateCoordinates(dpy, w, root, FIELD_X, FIELD_Y, &rx, &ry, &child);
    memset(&ev, 0, sizeof(ev));
    ev.type = ButtonPress;
    ev.xbutton.window = w;
    ev.xbutton.root = root;
    ev.xbutton.subwindow = None;
    ev.xbutton.time = CurrentTime;
    ev.xbutton.x = FIELD_X;
    ev.xbutton.y = FIELD_Y;
    ev.xbutton.x_root = rx;
    ev.xbutton.y_root = ry;
    ev.xbutton.state = 0;
    ev.xbutton.button = 1;
    ev.xbutton.same_screen = True;
    XSendEvent(dpy, w, True, ButtonPressMask, &ev);
    ev.type = ButtonRelease;
    ev.xbutton.state = Button1Mask;
    XSendEvent(dpy, w, True, ButtonReleaseMask, &ev);
    if (child && child != w) {
        ev.type = ButtonPress;
        ev.xbutton.window = child;
        ev.xbutton.state = 0;
        XSendEvent(dpy, child, True, ButtonPressMask, &ev);
        ev.type = ButtonRelease;
        ev.xbutton.state = Button1Mask;
        XSendEvent(dpy, child, True, ButtonReleaseMask, &ev);
    }
}

int main(void)
{
    Display *dpy;
    Window w = 0;
    int tries;
    Atom net_active, net_atom;

    dpy = XOpenDisplay(NULL);
    if (!dpy)
        return 1;
    for (tries = 0; tries < 30 && !w; tries++) {
        w = find_youtube(dpy, DefaultRootWindow(dpy));
        if (!w)
            usleep(200000);
    }
    if (!w) {
        XCloseDisplay(dpy);
        return 1;
    }
    /* Widgets exist a moment after MapNotify. */
    usleep(150000);
    XMapRaised(dpy, w);
    XRaiseWindow(dpy, w);
    net_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    net_atom = XInternAtom(dpy, "ATOM", False);
    if (net_active != None) {
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.xclient.type = ClientMessage;
        ev.xclient.window = w;
        ev.xclient.message_type = net_active;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 1;
        ev.xclient.data.l[1] = CurrentTime;
        ev.xclient.data.l[2] = 0;
        (void)net_atom;
        XSendEvent(dpy, DefaultRootWindow(dpy), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    }
    XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
    click_field(dpy, w);
    XFlush(dpy);
    usleep(80000);
    XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
    click_field(dpy, w);
    XFlush(dpy);
    XCloseDisplay(dpy);
    return 0;
}
