/* MiniOS AI chat window. Messages on the left and right, input at the bottom. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>

#define MAXM 24
#define MAXT 900
#define MAXIN 240
#define MAXLINES 14

static Display *dpy;
static Window win;
static GC gc;
static XFontStruct *fs;
static int scr, dw, dh;
static unsigned long bg, header, user_bg, ai_bg, field, fg, dim, send_bg;
static char input[MAXIN + 1];
static int inlen;
static char msg[MAXM][MAXT];
static int from_user[MAXM];
static int nmsg;
static int thinking;
static pid_t ask_pid;
static Atom wm_delete;
static int panel; /* 0 chat, 1 about, 2 model list */
static int svc;

struct svcinfo {
    const char *id;
    const char *label;
    const char *model;
    const char *about;
};

static const struct svcinfo services[] = {
    {
        "pollinations-openai",
        "Pollinations",
        "openai",
        "This chat sends your question to Pollinations and asks for the openai model."
    },
    {
        "pollinations-fast",
        "Pollinations Fast",
        "openai-fast",
        "This chat sends your question to Pollinations and asks for the openai-fast model."
    },
};
#define NSVC ((int)(sizeof(services) / sizeof(services[0])))
#define CHOICE "/home/tc/.minios/ai-service"

static int x_error(Display *d, XErrorEvent *e)
{
    (void)d;
    (void)e;
    return 0;
}

static int text_w(const char *s)
{
    if (!s || !s[0])
        return 0;
    if (fs)
        return XTextWidth(fs, s, (int)strlen(s));
    return (int)strlen(s) * 6;
}

static int font_h(void)
{
    if (fs)
        return fs->ascent + fs->descent;
    return 13;
}

static void fill(unsigned long c, int x, int y, int w, int h)
{
    XSetForeground(dpy, gc, c);
    XFillRectangle(dpy, win, gc, x, y, w, h);
}

static void box(unsigned long c, int x, int y, int w, int h)
{
    XSetForeground(dpy, gc, c);
    XDrawRectangle(dpy, win, gc, x, y, w, h);
}

static void draw_text(unsigned long c, int x, int y, const char *s)
{
    if (!s || !s[0])
        return;
    XSetForeground(dpy, gc, c);
    XDrawString(dpy, win, gc, x, y, s, (int)strlen(s));
}

static void add_msg(int user, const char *s)
{
    if (!s || !s[0])
        return;
    if (nmsg >= MAXM) {
        memmove(msg, msg + 1, (MAXM - 1) * MAXT);
        memmove(from_user, from_user + 1, (MAXM - 1) * sizeof(int));
        nmsg = MAXM - 1;
    }
    snprintf(msg[nmsg], MAXT, "%s", s);
    from_user[nmsg] = user;
    nmsg++;
}

static int in_box(int x, int y, int bx, int by, int bw, int bh)
{
    return x >= bx && x <= bx + bw && y >= by && y <= by + bh;
}

static const char *ask_bin(void)
{
    if (access("/opt/minios/minios-ai-ask", X_OK) == 0)
        return "/opt/minios/minios-ai-ask";
    if (access("/usr/local/bin/minios-ai-ask", X_OK) == 0)
        return "/usr/local/bin/minios-ai-ask";
    return NULL;
}

static void send_now(void)
{
    const char *bin;
    char saved[MAXIN + 1];

    if (thinking || inlen < 1)
        return;
    bin = ask_bin();
    if (!bin) {
        add_msg(0, "The chat helper is not available.");
        return;
    }
    snprintf(saved, sizeof(saved), "%s", input);
    add_msg(1, saved);
    input[0] = 0;
    inlen = 0;
    unlink("/tmp/minios-ai-out.txt");
    ask_pid = fork();
    if (ask_pid == 0) {
        execl(bin, "minios-ai-ask", saved, (char *)0);
        _exit(127);
    }
    if (ask_pid < 0) {
        thinking = 0;
        add_msg(0, "The question could not be sent.");
        return;
    }
    thinking = 1;
}

static void take_reply(void)
{
    FILE *f;
    char line[MAXT];
    size_t n = 0;
    int c;

    f = fopen("/tmp/minios-ai-out.txt", "r");
    if (!f) {
        add_msg(0, "No reply was returned.");
        return;
    }
    while (n + 1 < sizeof(line) && (c = fgetc(f)) != EOF) {
        if (c == '\n' || c == '\r')
            c = ' ';
        line[n++] = (char)c;
    }
    line[n] = 0;
    fclose(f);
    unlink("/tmp/minios-ai-out.txt");
    while (n > 0 && line[n - 1] == ' ')
        line[--n] = 0;
    add_msg(0, line[0] ? line : "No reply was returned.");
}

static int check_child(void)
{
    int st;
    if (!thinking || ask_pid <= 0)
        return 0;
    if (waitpid(ask_pid, &st, WNOHANG) == ask_pid) {
        thinking = 0;
        ask_pid = 0;
        take_reply();
        return 1;
    }
    return 0;
}

static void stop_ask(void)
{
    if (ask_pid > 0) {
        kill(ask_pid, SIGTERM);
        waitpid(ask_pid, NULL, WNOHANG);
        ask_pid = 0;
    }
    thinking = 0;
}

/* How many source bytes fit on one row. Prefer a space near the end. */
static int row_take(const char *s, int len, int maxw)
{
    int take = len;
    int sp;
    char line[MAXT];

    if (take >= (int)sizeof(line))
        take = (int)sizeof(line) - 1;
    if (take < 1)
        return 1;
    memcpy(line, s, take);
    line[take] = 0;
    while (take > 1 && text_w(line) > maxw)
        line[--take] = 0;
    if (take < len) {
        sp = take;
        while (sp > take / 2 && line[sp - 1] != ' ')
            sp--;
        if (sp > 0 && line[sp - 1] == ' ' && sp > take / 2)
            take = sp;
    }
    return take;
}

static int wrap_count(const char *s, int maxw)
{
    int n = 0, i = 0, len, take;
    if (!s || !s[0] || maxw < 20)
        return 1;
    len = (int)strlen(s);
    while (i < len && n < MAXLINES) {
        while (s[i] == ' ')
            i++;
        if (i >= len)
            break;
        take = row_take(s + i, len - i, maxw);
        i += take;
        n++;
    }
    return n < 1 ? 1 : n;
}

static void draw_wrap(const char *s, int x, int y, int maxw, unsigned long color)
{
    int i = 0, row = 0, len, take;
    char line[MAXT];
    int fh = font_h();
    if (!s)
        return;
    len = (int)strlen(s);
    while (i < len && row < MAXLINES) {
        while (s[i] == ' ')
            i++;
        if (i >= len)
            break;
        take = row_take(s + i, len - i, maxw);
        if (take >= (int)sizeof(line))
            take = (int)sizeof(line) - 1;
        memcpy(line, s + i, take);
        line[take] = 0;
        while (take > 0 && line[take - 1] == ' ')
            line[--take] = 0;
        draw_text(color, x, y + row * (fh + 2) + fh, line);
        i += take;
        row++;
    }
}

static char set_model[80] = "openai";
static char set_url[180] = "https://text.pollinations.ai/openai";
static char set_key[80];
static int set_focus;

static void trim_nl(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = 0;
}

static void load_choice(void)
{
    FILE *f;
    snprintf(set_model, sizeof(set_model), "openai");
    snprintf(set_url, sizeof(set_url), "https://text.pollinations.ai/openai");
    set_key[0] = 0;
    f = fopen(CHOICE, "r");
    if (!f)
        return;
    if (fgets(set_model, sizeof(set_model), f))
        trim_nl(set_model);
    if (fgets(set_url, sizeof(set_url), f))
        trim_nl(set_url);
    if (fgets(set_key, sizeof(set_key), f))
        trim_nl(set_key);
    fclose(f);
    if (!set_model[0])
        snprintf(set_model, sizeof(set_model), "openai");
    if (!set_url[0])
        snprintf(set_url, sizeof(set_url), "https://text.pollinations.ai/openai");
}

static void save_choice(void)
{
    FILE *f;
    mkdir("/home/tc/.minios", 0755);
    f = fopen(CHOICE, "w");
    if (!f)
        return;
    fprintf(f, "%s\n%s\n%s\n", set_model, set_url, set_key);
    fclose(f);
}

static int net_up(void)
{
    DIR *d;
    struct dirent *e;
    d = opendir("/sys/class/net");
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL) {
        char path[160], state[16];
        FILE *f;
        if (e->d_name[0] == '.' || strcmp(e->d_name, "lo") == 0)
            continue;
        snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", e->d_name);
        f = fopen(path, "r");
        if (!f)
            continue;
        if (fgets(state, sizeof(state), f) && strncmp(state, "up", 2) == 0) {
            fclose(f);
            closedir(d);
            return 1;
        }
        fclose(f);
    }
    closedir(d);
    return 0;
}

static void welcome(void)
{
    char line[MAXT];
    snprintf(line, sizeof(line),
             "You are now connected to the Pollinations.ai/openai.  Type your message and press Enter.");
    nmsg = 0;
    add_msg(0, line);
}

static void paint(void)
{
    int fh = font_h();
    int top = 44;
    int bottom = dh - 56;
    int area = bottom - top;
    int bubble_w = dw / 2 - 24;
    int i, y, used, start;
    int send_x = dw - 86;
    char shown[MAXIN + 2];

    if (bubble_w < 120)
        bubble_w = 120;
    fill(bg, 0, 0, dw, dh);
    fill(header, 0, 0, dw, 36);
    draw_text(fg, 16, 24,
              (net_up() && access("/tmp/minios-ai.helper", R_OK) == 0)
                  ? "AI Chat - Active" : "AI Chat - Offline");
    fill(0x5a6578, dw - 310, 8, 72, 20);
    draw_text(fg, dw - 302, 22, "Refresh");
    fill(0x5a6578, dw - 230, 8, 64, 20);
    draw_text(fg, dw - 220, 22, "About");
    fill(0x5a6578, dw - 156, 8, 70, 20);
    draw_text(fg, dw - 150, 22, "Settings");
    fill(0x5a6578, dw - 78, 8, 64, 20);
    draw_text(fg, dw - 66, 22, "Close");
    if (panel == 1) {
        char note[MAXT];
        fill(ai_bg, 24, 48, dw - 48, 160);
        snprintf(note, sizeof(note),
                 "Model %s. Address %s.",
                 set_model, set_url);
        draw_wrap(note, 36, 56, dw - 80, fg);
        draw_text(dim, 36, 190, "Click the chat to close this note.");
    } else if (panel == 2) {
        fill(ai_bg, 24, 48, dw - 48, 210);
        draw_text(fg, 36, 70, "Model");
        fill(set_focus == 0 ? user_bg : field, 140, 54, dw - 180, 22);
        draw_text(fg, 148, 70, set_model[0] ? set_model : "openai");
        draw_text(fg, 36, 104, "Address");
        fill(set_focus == 1 ? user_bg : field, 140, 88, dw - 180, 22);
        draw_text(fg, 148, 104, set_url);
        draw_text(fg, 36, 138, "Key");
        fill(set_focus == 2 ? user_bg : field, 140, 122, dw - 180, 22);
        draw_text(fg, 148, 138, set_key[0] ? set_key : "(none)");
        fill(send_bg, 140, 160, 72, 24);
        draw_text(0x061018, 156, 176, "Save");
        draw_text(dim, 224, 176, "Click a box, type, then Save.");
    }

    used = 0;
    start = nmsg;
    for (i = nmsg - 1; i >= 0; i--) {
        int lines = wrap_count(msg[i], bubble_w - 16);
        int h = lines * (fh + 2) + 14;
        if (thinking && i == nmsg - 1)
            h += 0;
        if (used + h > area && start != nmsg)
            break;
        used += h + 8;
        start = i;
    }
    y = bottom - 8;
    if (thinking) {
        y -= fh + 18;
        fill(ai_bg, 16, y, 90, fh + 10);
        draw_text(fg, 26, y + fh + 1, "...");
    }
    for (i = nmsg - 1; i >= start; i--) {
        int lines = wrap_count(msg[i], bubble_w - 16);
        int h = lines * (fh + 2) + 14;
        int x;
        y -= h;
        if (y < top)
            break;
        if (from_user[i])
            x = dw - bubble_w - 16;
        else
            x = 16;
        fill(from_user[i] ? user_bg : ai_bg, x, y, bubble_w, h);
        draw_wrap(msg[i], x + 8, y + 4, bubble_w - 16, fg);
        y -= 8;
    }

    fill(field, 12, dh - 44, send_x - 20, 32);
    box(dim, 12, dh - 44, send_x - 20, 32);
    snprintf(shown, sizeof(shown), "%s", input);
    if (inlen < MAXIN)
        strcat(shown, "_");
    while (shown[0] && text_w(shown) > send_x - 36)
        memmove(shown, shown + 1, strlen(shown));
    if (shown[0])
        draw_text(fg, 20, dh - 44 + 8 + fh, shown);
    else
        draw_text(dim, 20, dh - 44 + 8 + fh, "Type a message");
    fill(send_bg, send_x, dh - 44, 72, 32);
    draw_text(0x061018, send_x + 18, dh - 44 + 8 + fh, "Send");
    XFlush(dpy);
}

static void on_key(XKeyEvent *ev)
{
    KeySym ks;
    char buf[16];
    int n;

    n = XLookupString(ev, buf, sizeof(buf) - 1, &ks, NULL);
    if (panel == 2) {
        char *dst = set_model;
        int cap = (int)sizeof(set_model) - 1;
        int len;
        if (set_focus == 1) {
            dst = set_url;
            cap = (int)sizeof(set_url) - 1;
        } else if (set_focus == 2) {
            dst = set_key;
            cap = (int)sizeof(set_key) - 1;
        }
        len = (int)strlen(dst);
        if (ks == XK_Return || ks == XK_KP_Enter) {
            save_choice();
            unlink("/tmp/minios-ai-hist.json");
            welcome();
            panel = 0;
            return;
        }
        if (ks == XK_BackSpace) {
            if (len > 0)
                dst[len - 1] = 0;
            return;
        }
        if (n == 1 && isprint((unsigned char)buf[0]) && len < cap) {
            dst[len] = buf[0];
            dst[len + 1] = 0;
        }
        return;
    }
    if (ks == XK_Return || ks == XK_KP_Enter) {
        send_now();
        return;
    }
    if (ks == XK_BackSpace) {
        if (inlen > 0)
            input[--inlen] = 0;
        return;
    }
    if (n == 1 && isprint((unsigned char)buf[0]) && inlen < MAXIN) {
        input[inlen++] = buf[0];
        input[inlen] = 0;
    }
}

static void reload_helper(void)
{
    pid_t p;
    add_msg(0, "Reloading the answer helper.");
    p = fork();
    if (p < 0)
        return;
    if (p == 0) {
        execl("/opt/minios/minios-ai-reload", "minios-ai-reload", (char *)0);
        execl("/usr/local/bin/minios-ai-reload", "minios-ai-reload", (char *)0);
        _exit(127);
    }
}

static void on_click(int x, int y)
{
    int send_x = dw - 86;
    if (in_box(x, y, dw - 78, 8, 64, 20)) {
        stop_ask();
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        exit(0);
    }
    if (in_box(x, y, dw - 310, 8, 72, 20)) {
        reload_helper();
        return;
    }
    if (in_box(x, y, dw - 230, 8, 64, 20)) {
        panel = panel == 1 ? 0 : 1;
        return;
    }
    if (in_box(x, y, dw - 156, 8, 70, 20)) {
        panel = panel == 2 ? 0 : 2;
        return;
    }
    if (panel == 2) {
        if (in_box(x, y, 140, 54, dw - 180, 22)) {
            set_focus = 0;
            return;
        }
        if (in_box(x, y, 140, 88, dw - 180, 22)) {
            set_focus = 1;
            return;
        }
        if (in_box(x, y, 140, 122, dw - 180, 22)) {
            set_focus = 2;
            return;
        }
        if (in_box(x, y, 140, 160, 72, 24)) {
            save_choice();
            unlink("/tmp/minios-ai-hist.json");
            welcome();
            panel = 0;
            return;
        }
        return;
    }
    if (panel) {
        panel = 0;
        return;
    }
    if (in_box(x, y, send_x, dh - 44, 72, 32))
        send_now();
}

int main(void)
{
    XEvent ev;
    int xfd;
    Colormap cmap;
    XColor col, dummy;

    dpy = XOpenDisplay(NULL);
    if (!dpy)
        return 1;
    XSetErrorHandler(x_error);
    scr = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, scr);
    bg = 0x1b2430;
    header = 0x121820;
    user_bg = 0x0e7490;
    ai_bg = 0x2c3548;
    field = 0x0f141c;
    fg = 0xf2f5f8;
    dim = 0x8b97a8;
    send_bg = 0x22d3ee;
    if (XAllocNamedColor(dpy, cmap, "#1b2430", &col, &dummy))
        bg = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#121820", &col, &dummy))
        header = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#0e7490", &col, &dummy))
        user_bg = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#2c3548", &col, &dummy))
        ai_bg = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#0f141c", &col, &dummy))
        field = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#f2f5f8", &col, &dummy))
        fg = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#8b97a8", &col, &dummy))
        dim = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#22d3ee", &col, &dummy))
        send_bg = col.pixel;

    dw = DisplayWidth(dpy, scr);
    dh = DisplayHeight(dpy, scr);
    if (dw < 480)
        dw = 480;
    if (dh < 320)
        dh = 320;

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, dw, dh, 0,
                              fg, bg);
    XStoreName(dpy, win, "AI Chat");
    gc = XCreateGC(dpy, win, 0, NULL);
    fs = XLoadQueryFont(dpy, "6x13");
    if (!fs)
        fs = XLoadQueryFont(dpy, "fixed");
    if (fs)
        XSetFont(dpy, gc, fs->fid);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask |
                           StructureNotifyMask);
    XMapRaised(dpy, win);
    {
        FILE *f = fopen("/tmp/minios-ai.ready", "w");
        if (f)
            fclose(f);
        f = fopen("/tmp/minios-ai.pid", "w");
        if (f) {
            fprintf(f, "%d\n", (int)getpid());
            fclose(f);
        }
    }
    unlink("/tmp/minios-ai-hist.json");
    load_choice();
    welcome();
    paint();
    xfd = ConnectionNumber(dpy);

    for (;;) {
        fd_set fds;
        struct timeval tv;
        int got = check_child();
        int helper_now = access("/tmp/minios-ai.helper", R_OK) == 0;
        static int helper_was = -1;
        if (helper_now != helper_was) {
            helper_was = helper_now;
            paint();
        }
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = thinking ? 200000 : 400000;
        if (select(xfd + 1, &fds, NULL, NULL, &tv) > 0) {
            while (XPending(dpy)) {
                XNextEvent(dpy, &ev);
                if (ev.type == Expose && ev.xexpose.count == 0)
                    paint();
                else if (ev.type == MapNotify)
                    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
                else if (ev.type == ConfigureNotify) {
                    if (ev.xconfigure.width > 200 && ev.xconfigure.height > 160) {
                        dw = ev.xconfigure.width;
                        dh = ev.xconfigure.height;
                    }
                    paint();
                } else if (ev.type == KeyPress)
                    on_key(&ev.xkey);
                else if (ev.type == ButtonPress)
                    on_click(ev.xbutton.x, ev.xbutton.y);
                else if (ev.type == ClientMessage &&
                         (Atom)ev.xclient.data.l[0] == wm_delete) {
                    stop_ask();
                    XCloseDisplay(dpy);
                    return 0;
                }
                if (ev.type == KeyPress || ev.type == ButtonPress)
                    paint();
            }
            if (got)
                paint();
        } else if (thinking || got) {
            paint();
        }
    }
}
