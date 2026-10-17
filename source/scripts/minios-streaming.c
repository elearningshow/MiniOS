/* MiniOS Streaming Player. Its own window. It does not call YouTube or TikTok. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>

#define MAXR 40
#define MAXL 700
#define PAGE 6

static Display *dpy;
static Window win;
static GC gc;
static int ww = 920, wh = 560, scr;
static unsigned long fg, bg, btn, field, hi;
static XFontStruct *fs;
static Atom wm_delete;
static char query[96];
static int qlen;
static char titles[MAXR][80];
static char urls[MAXR][MAXL];
static char kinds[MAXR][48];
static int nres, page, searching;
static char status[180];
static pid_t search_pid;
static pid_t play_pid;
static int search_fd = -1;
static char acc[MAXL * 2];
static int acc_n;
static int go_x, go_w, stop_x, close_x, btn_y, btn_h, field_x, field_w;
static int list_y, row_h, page_rows;
static int prev_x, next_x, pagebtn_y, pagebtn_w, pagebtn_h;
static time_t play_started, pic_lift;
static Window pop, vid;
static void (*volatile st_keep_pop)(void);
static int pop_on, vid_on, play_i = -1, paused, vol = 80, vol_sent, want_pause;
static int vid_w, vid_h;
static char now_title[80];
static void start_play(const char *url, const char *title);
static void paint(void);
static void paint_pop(void);
static void paint_video(void);
static void hide_video(void);
static void st_show_controls(void);
static void st_stop_station(void);
static void st_step(int dir);
static void layout(void);
static int hit(int x, int y, int bx, int by, int bw, int bh);

static int x_error(Display *d, XErrorEvent *e)
{
    (void)d;
    (void)e;
    return 0;
}

static void text_at(unsigned long c, int x, int y, const char *s)
{
    XSetForeground(dpy, gc, c);
    XDrawString(dpy, win, gc, x, y, s, (int)strlen(s));
}

static void fill(unsigned long c, int x, int y, int w, int h)
{
    XSetForeground(dpy, gc, c);
    XFillRectangle(dpy, win, gc, x, y, (unsigned)w, (unsigned)h);
}

static void frame(unsigned long c, int x, int y, int w, int h)
{
    XSetForeground(dpy, gc, c);
    XDrawRectangle(dpy, win, gc, x, y, (unsigned)w, (unsigned)h);
}

static void fit_copy(char *dst, int n, const char *src)
{
    int i = 0;

    if (n < 2) {
        if (n > 0)
            dst[0] = 0;
        return;
    }
    while (src[i] && i < n - 1) {
        unsigned char ch = (unsigned char)src[i];
        dst[i] = (ch >= 32 && ch < 127) ? (char)ch : '?';
        i++;
    }
    dst[i] = 0;
}

static const char *script_path(const char *name)
{
    static char path[128];

    snprintf(path, sizeof path, "/opt/minios/%s", name);
    if (access(path, X_OK) == 0)
        return path;
    snprintf(path, sizeof path, "/usr/local/bin/%s", name);
    if (access(path, X_OK) == 0)
        return path;
    return NULL;
}

static void stop_play(void)
{
    char buf[32];
    FILE *f;
    int n;
    pid_t extra = 0;

    if (play_pid > 0) {
        kill(-play_pid, SIGTERM);
        kill(play_pid, SIGTERM);
        waitpid(play_pid, NULL, WNOHANG);
        play_pid = 0;
    }
    f = fopen("/tmp/minios-st-play.pid", "r");
    if (f) {
        n = (int)fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        if (n > 0) {
            buf[n] = 0;
            extra = (pid_t)atoi(buf);
        }
    }
    if (extra > 1) {
        kill(extra, SIGTERM);
        kill(extra, SIGKILL);
    }
    unlink("/tmp/minios-st-play.pid");
    unlink("/tmp/minios-st-cmd");
    unlink("/tmp/minios-st-geom");
}

/* Talk to this station's player only. Not the YouTube or TikTok command pipe. */
static void st_cmd(const char *line)
{
    int fd;

    if (!line || !line[0])
        return;
    fd = open("/tmp/minios-st-cmd", O_WRONLY | O_NONBLOCK);
    if (fd < 0)
        return;
    if (write(fd, line, strlen(line)) < 0)
        ;
    close(fd);
}

static void st_send_vol(void)
{
    char buf[64];

    snprintf(buf, sizeof buf, "pausing_keep_force volume %d 1\n", vol);
    st_cmd(buf);
}

static void st_toggle_pause(void)
{
    want_pause = !want_pause;
    paused = want_pause;
    if (vol_sent)
        st_cmd("pause\n");
    if (paused)
        snprintf(status, sizeof status, "Paused");
    else if (play_pid > 0 && time(NULL) - play_started >= 2)
        snprintf(status, sizeof status, "Playing");
    else
        snprintf(status, sizeof status, "Loading the station. Please wait.");
    st_show_controls();
}

static void hide_pop(void)
{
    if (!pop)
        return;
    XUnmapWindow(dpy, pop);
    pop_on = 0;
}

static void paint_pop(void)
{
    char vbuf[8];
    const char *line;
    const char *pause_lab;

    if (!pop || !pop_on)
        return;
    XSetForeground(dpy, gc, bg);
    XFillRectangle(dpy, pop, gc, 0, 0, 560, 188);
    XSetForeground(dpy, gc, fg);
    XDrawRectangle(dpy, pop, gc, 0, 0, 559, 187);
    XDrawString(dpy, pop, gc, 16, 32, now_title, (int)strlen(now_title));
    if (play_i >= 0 && play_i < nres) {
        int video = strncmp(kinds[play_i], "Video", 5) == 0;
        XSetForeground(dpy, gc, video ? 0xD8E8FF : 0xE8F0E8);
        XFillRectangle(dpy, pop, gc, 456, 12, 84, 22);
        XSetForeground(dpy, gc, fg);
        XDrawRectangle(dpy, pop, gc, 456, 12, 84, 22);
        XDrawString(dpy, pop, gc, 470, 28, video ? "Video" : "Audio", 5);
    }
    if (play_pid > 0 && !paused && time(NULL) - play_started < 2)
        line = "Loading the station. Please wait.";
    else if (paused)
        line = "Paused";
    else if (play_pid > 0)
        line = "Playing";
    else
        line = status;
    XDrawString(dpy, pop, gc, 16, 64, line, (int)strlen(line));
    pause_lab = paused ? "Play" : "Pause";
    XSetForeground(dpy, gc, btn);
    XFillRectangle(dpy, pop, gc, 16, 96, 80, 36);
    XFillRectangle(dpy, pop, gc, 108, 96, 110, 36);
    XFillRectangle(dpy, pop, gc, 228, 96, 80, 36);
    XFillRectangle(dpy, pop, gc, 324, 96, 40, 36);
    XFillRectangle(dpy, pop, gc, 412, 96, 40, 36);
    XFillRectangle(dpy, pop, gc, 464, 96, 76, 36);
    XSetForeground(dpy, gc, fg);
    XDrawRectangle(dpy, pop, gc, 16, 96, 80, 36);
    XDrawRectangle(dpy, pop, gc, 108, 96, 110, 36);
    XDrawRectangle(dpy, pop, gc, 228, 96, 80, 36);
    XDrawRectangle(dpy, pop, gc, 324, 96, 40, 36);
    XDrawRectangle(dpy, pop, gc, 412, 96, 40, 36);
    XDrawRectangle(dpy, pop, gc, 464, 96, 76, 36);
    XDrawString(dpy, pop, gc, 28, 118, pause_lab, (int)strlen(pause_lab));
    XDrawString(dpy, pop, gc, 120, 118, "<Previous", 9);
    XDrawString(dpy, pop, gc, 246, 118, "Next>", 5);
    XDrawString(dpy, pop, gc, 338, 118, "-", 1);
    XDrawString(dpy, pop, gc, 426, 118, "+", 1);
    XDrawString(dpy, pop, gc, 480, 118, "Close", 5);
    snprintf(vbuf, sizeof vbuf, "%d", vol);
    XDrawString(dpy, pop, gc, 372, 118, vbuf, (int)strlen(vbuf));
    XFlush(dpy);
}

static void show_pop(void)
{
    int sw, sh;

    if (!dpy)
        return;
    sw = DisplayWidth(dpy, scr);
    sh = DisplayHeight(dpy, scr);
    if (!pop) {
        pop = XCreateSimpleWindow(dpy, RootWindow(dpy, scr),
                                   (sw - 560) / 2, (sh - 188) / 2,
                                   560, 188, 1, fg, bg);
        XStoreName(dpy, pop, "MiniOS Streaming");
        XSetWMProtocols(dpy, pop, &wm_delete, 1);
        XSelectInput(dpy, pop, ExposureMask | KeyPressMask | ButtonPressMask);
        fprintf(stderr, "stream-v253 pop\n");
    }
    XMapRaised(dpy, pop);
    pop_on = 1;
    paint_pop();
}

static void stop_search(void)
{
    if (search_pid > 0) {
        kill(-search_pid, SIGTERM);
        kill(search_pid, SIGTERM);
        waitpid(search_pid, NULL, WNOHANG);
        search_pid = 0;
    }
    if (search_fd >= 0) {
        close(search_fd);
        search_fd = -1;
    }
    searching = 0;
    acc_n = 0;
}

#define ST_W_PAUSE 64
#define ST_W_PREV 90
#define ST_W_NEXT 58
#define ST_W_PM 28
#define ST_W_CLOSE 64

/* Right side of one row: Pause, Previous, Next, volume, Close. */
static void st_ctrl_xs(int width, int *pause, int *prevb, int *nextb,
                       int *minus, int *plus, int *closeb)
{
    int x = width - 12 - ST_W_CLOSE;

    *closeb = x;
    x -= 4 + ST_W_PM;
    *plus = x;
    x -= 32 + ST_W_PM;
    *minus = x;
    x -= 4 + ST_W_NEXT;
    *nextb = x;
    x -= 4 + ST_W_PREV;
    *prevb = x;
    x -= 4 + ST_W_PAUSE;
    *pause = x;
}

static void draw_text(Window w, unsigned long c, int x, int y, const char *s)
{
    if (!w || !s)
        return;
    XSetForeground(dpy, gc, c);
    XDrawString(dpy, w, gc, x, y, s, (int)strlen(s));
}

static void draw_fill(Window w, unsigned long c, int x, int y, int rw, int rh)
{
    if (!w || rw < 1 || rh < 1)
        return;
    XSetForeground(dpy, gc, c);
    XFillRectangle(dpy, w, gc, x, y, (unsigned)rw, (unsigned)rh);
}

static void draw_frame(Window w, unsigned long c, int x, int y, int rw, int rh)
{
    if (!w || rw < 1 || rh < 1)
        return;
    XSetForeground(dpy, gc, c);
    XDrawRectangle(dpy, w, gc, x, y, (unsigned)rw, (unsigned)rh);
}

static void st_paint_ctrls(Window w, int width, int y, int bh)
{
    int pause, prevb, nextb, minus, plus, closeb;
    char vbuf[8];
    const char *pause_lab = paused ? "Play" : "Pause";
    int ty = y + 17;

    st_ctrl_xs(width, &pause, &prevb, &nextb, &minus, &plus, &closeb);
    draw_fill(w, btn, pause, y, ST_W_PAUSE, bh);
    draw_fill(w, btn, prevb, y, ST_W_PREV, bh);
    draw_fill(w, btn, nextb, y, ST_W_NEXT, bh);
    draw_fill(w, btn, minus, y, ST_W_PM, bh);
    draw_fill(w, btn, plus, y, ST_W_PM, bh);
    draw_fill(w, btn, closeb, y, ST_W_CLOSE, bh);
    draw_frame(w, fg, pause, y, ST_W_PAUSE, bh);
    draw_frame(w, fg, prevb, y, ST_W_PREV, bh);
    draw_frame(w, fg, nextb, y, ST_W_NEXT, bh);
    draw_frame(w, fg, minus, y, ST_W_PM, bh);
    draw_frame(w, fg, plus, y, ST_W_PM, bh);
    draw_frame(w, fg, closeb, y, ST_W_CLOSE, bh);
    draw_text(w, fg, pause + 8, ty, pause_lab);
    draw_text(w, fg, prevb + 4, ty, "<Previous");
    draw_text(w, fg, nextb + 6, ty, "Next>");
    draw_text(w, fg, minus + 10, ty, "-");
    draw_text(w, fg, plus + 10, ty, "+");
    draw_text(w, fg, closeb + 10, ty, "Close");
    snprintf(vbuf, sizeof vbuf, "%d", vol);
    draw_text(w, fg, minus + ST_W_PM + 6, ty, vbuf);
}

static int st_play_is_video(const char *url)
{
    if (play_i >= 0 && play_i < nres)
        return strncmp(kinds[play_i], "Video", 5) == 0;
    if (!url)
        return 0;
    return strstr(url, ".m3u8") || strstr(url, ".smil") || strstr(url, "/hls/");
}

static int st_audio_row_on(void)
{
    return play_pid > 0 && play_i >= 0 && play_i < nres
        && strncmp(kinds[play_i], "Video", 5) != 0;
}

static void st_paint_audio_row(int y, const char *title, const char *kind)
{
    int pause_x, prevb, nextb, minus, plus, closeb, maxc;
    char line[80];

    st_ctrl_xs(ww, &pause_x, &prevb, &nextb, &minus, &plus, &closeb);
    fill(0xFFF3C4, 12, y, ww - 24, row_h - 4);
    frame(fg, 12, y, ww - 24, row_h - 4);
    maxc = (pause_x - 24) / 9;
    if (maxc > 42)
        maxc = 42;
    if (maxc < 8)
        maxc = 8;
    fit_copy(line, maxc, title ? title : "");
    text_at(fg, 20, y + 15, line);
    fit_copy(line, 18, kind ? kind : "");
    text_at(hi, 20, y + 30, line);
    st_paint_ctrls(win, ww, y + 4, row_h - 10);
}

static void st_raise_picture(void)
{
    Window root, parent, *kids = NULL;
    unsigned n = 0, i;

    if (!dpy || !vid_on)
        return;
    if (!XQueryTree(dpy, RootWindow(dpy, scr), &root, &parent, &kids, &n))
        return;
    for (i = 0; i < n; i++) {
        XWindowAttributes a;

        if (kids[i] == win || kids[i] == vid || kids[i] == pop)
            continue;
        if (!XGetWindowAttributes(dpy, kids[i], &a))
            continue;
        if (a.map_state != IsViewable)
            continue;
        if (a.width >= 450 && a.width <= 530 && a.height >= 240 && a.height <= 320)
            XRaiseWindow(dpy, kids[i]);
    }
    if (kids)
        XFree(kids);
}

static void paint_video(void)
{
    char line[80];
    const char *msg;
    int maxc, pic_x, pause_x, skip;

    if (!vid || !vid_on)
        return;
    draw_fill(vid, 0x202020, 0, 0, vid_w, vid_h);
    draw_text(vid, 0xFFFFFF, 16, 24, "MiniOS Streaming Player");
    maxc = (vid_w - 360) / 9;
    if (maxc > 40)
        maxc = 40;
    if (maxc < 8)
        maxc = 8;
    fit_copy(line, maxc, now_title);
    draw_text(vid, 0xFFFFFF, 240, 24, line);
    draw_fill(vid, 0xD8E8FF, vid_w - 100, 8, 84, 20);
    draw_frame(vid, 0xFFFFFF, vid_w - 100, 8, 84, 20);
    draw_text(vid, fg, vid_w - 80, 23, "Video");
    pic_x = (vid_w - 480) / 2;
    if (pic_x < 0)
        pic_x = 0;
    draw_fill(vid, 0x000000, pic_x, 36, 480, 270);
    if (play_pid > 0 && !paused && time(NULL) - play_started < 2)
        msg = "Loading the station. Please wait.";
    else if (paused)
        msg = "Paused";
    else if (play_pid > 0)
        msg = "Playing";
    else
        msg = status;
    st_ctrl_xs(vid_w, &pause_x, &skip, &skip, &skip, &skip, &skip);
    (void)skip;
    maxc = (pause_x - 20) / 9;
    if (maxc > 42)
        maxc = 42;
    if (maxc < 8)
        maxc = 8;
    fit_copy(line, maxc, msg);
    draw_text(vid, 0xFFFFFF, 16, vid_h - 14, line);
    st_paint_ctrls(vid, vid_w, vid_h - 30, 26);
    XFlush(dpy);
}

static void hide_video(void)
{
    if (!vid)
        return;
    XUnmapWindow(dpy, vid);
    vid_on = 0;
}

static void show_video(void)
{
    int sw, sh, vx, vy, pic_x, rx, ry, tries;
    Window child;
    FILE *gf;

    if (!dpy)
        return;
    sw = DisplayWidth(dpy, scr);
    sh = DisplayHeight(dpy, scr);
    vid_w = sw - 80;
    if (vid_w > 960)
        vid_w = 960;
    if (vid_w < 720)
        vid_w = 720;
    vid_h = 36 + 270 + 52;
    vx = (sw - vid_w) / 2;
    if (vx < 8)
        vx = 8;
    vy = 16;
    if (vy + vid_h > sh - 60)
        vy = 8;
    pic_x = (vid_w - 480) / 2;
    if (pic_x < 0)
        pic_x = 0;
    if (!vid) {
        vid = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), vx, vy,
                                   (unsigned)vid_w, (unsigned)vid_h, 1, fg, 0x202020);
        XStoreName(dpy, vid, "MiniOS Streaming Player");
        XSetWMProtocols(dpy, vid, &wm_delete, 1);
        XSelectInput(dpy, vid, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
        fprintf(stderr, "stream-v256 view\n");
    }
    XMoveResizeWindow(dpy, vid, vx, vy, (unsigned)vid_w, (unsigned)vid_h);
    XMapRaised(dpy, vid);
    vid_on = 1;
    for (tries = 0; tries < 5; tries++) {
        XEvent ev;

        XSync(dpy, False);
        while (XCheckTypedWindowEvent(dpy, vid, ConfigureNotify, &ev))
            ;
        usleep(20000);
    }
    rx = vx + pic_x;
    ry = vy + 36;
    if (!XTranslateCoordinates(dpy, vid, RootWindow(dpy, scr), pic_x, 36, &rx, &ry, &child)) {
        rx = vx + pic_x;
        ry = vy + 36;
    }
    gf = fopen("/tmp/minios-st-geom", "w");
    if (gf) {
        fprintf(gf, "480x270+%d+%d\n", rx, ry);
        fclose(gf);
    }
    paint_video();
}

static void st_show_controls(void)
{
    if (vid_on)
        paint_video();
    else if (pop_on)
        paint_pop();
    else
        paint();
}

static void st_stop_station(void)
{
    hide_pop();
    hide_video();
    stop_play();
    play_i = -1;
    paused = 0;
    want_pause = 0;
    snprintf(status, sizeof status, "Stopped.");
    paint();
}

/* 1 = a control was used. 2 = Close. 0 = the click missed the buttons. */
static int st_ctrl_hit(int width, int y, int bh, int x, int yy)
{
    int pause, prevb, nextb, minus, plus, closeb;

    st_ctrl_xs(width, &pause, &prevb, &nextb, &minus, &plus, &closeb);
    if (hit(x, yy, pause, y, ST_W_PAUSE, bh)) {
        st_toggle_pause();
        return 1;
    }
    if (hit(x, yy, prevb, y, ST_W_PREV, bh)) {
        st_step(-1);
        return 1;
    }
    if (hit(x, yy, nextb, y, ST_W_NEXT, bh)) {
        st_step(1);
        return 1;
    }
    if (hit(x, yy, minus, y, ST_W_PM, bh)) {
        if (vol > 0)
            vol -= 10;
        st_send_vol();
        st_show_controls();
        return 1;
    }
    if (hit(x, yy, plus, y, ST_W_PM, bh)) {
        if (vol < 100)
            vol += 10;
        st_send_vol();
        st_show_controls();
        return 1;
    }
    if (hit(x, yy, closeb, y, ST_W_CLOSE, bh))
        return 2;
    return 0;
}

static void vid_event(XEvent *ev)
{
    int act;

    if (!ev)
        return;
    if (ev->type == Expose && ev->xexpose.count == 0) {
        paint_video();
        st_raise_picture();
        return;
    }
    if (ev->type == ClientMessage && (Atom)ev->xclient.data.l[0] == wm_delete) {
        st_stop_station();
        return;
    }
    if (ev->type == KeyPress) {
        KeySym ks;
        char ch[8];

        XLookupString(&ev->xkey, ch, sizeof ch, &ks, NULL);
        if (ks == XK_Escape)
            st_stop_station();
        else if (ks == XK_space)
            st_toggle_pause();
        else if (ch[0] == '-' || ks == XK_minus) {
            if (vol > 0)
                vol -= 10;
            st_send_vol();
            st_show_controls();
        } else if (ch[0] == '+' || ch[0] == '=') {
            if (vol < 100)
                vol += 10;
            st_send_vol();
            st_show_controls();
        }
        if (vid_on)
            st_raise_picture();
        return;
    }
    if (ev->type != ButtonPress)
        return;
    act = st_ctrl_hit(vid_w, vid_h - 30, 26, ev->xbutton.x, ev->xbutton.y);
    if (act == 2)
        st_stop_station();
    else
        st_raise_picture();
}

static void start_play(const char *url, const char *title)
{
    const char *bin;
    pid_t pid;

    bin = script_path("minios-streaming-play");
    if (!bin || !url || !url[0]) {
        snprintf(status, sizeof status, "Could not start this station.");
        return;
    }
    fit_copy(now_title, (int)sizeof now_title, title && title[0] ? title : url);
    paused = 0;
    want_pause = 0;
    vol_sent = 0;
    snprintf(status, sizeof status, "Loading the station. Please wait.");
    hide_pop();
    stop_play();
    pic_lift = 0;
    if (st_play_is_video(url)) {
        show_video();
        fprintf(stderr, "stream-v256 view\n");
    } else {
        hide_video();
        XRaiseWindow(dpy, win);
        fprintf(stderr, "stream-v256 row\n");
    }
    XFlush(dpy);
    pid = fork();
    if (pid < 0) {
        snprintf(status, sizeof status, "Could not start this station.");
        st_show_controls();
        return;
    }
    if (pid == 0) {
        setpgid(0, 0);
        execl(bin, "minios-streaming-play", url, (char *)0);
        _exit(127);
    }
    setpgid(pid, pid);
    play_pid = pid;
    play_started = time(NULL);
    fprintf(stderr, "stream-v247 play\n");
    snprintf(status, sizeof status, "Loading the station. Please wait.");
    st_show_controls();
}

static void st_step(int dir)
{
    int n = play_i + dir;

    if (n < 0 || n >= nres)
        return;
    play_i = n;
    layout();
    if (page_rows > 0)
        page = n / page_rows;
    start_play(urls[n], titles[n]);
}

static void pop_event(XEvent *ev)
{
    int x, y;

    if (!ev)
        return;
    if (ev->type == Expose && ev->xexpose.count == 0) {
        paint_pop();
        return;
    }
    if (ev->type == ClientMessage && (Atom)ev->xclient.data.l[0] == wm_delete) {
        hide_pop();
        stop_play();
        snprintf(status, sizeof status, "Stopped.");
        paint();
        return;
    }
    if (ev->type == KeyPress) {
        KeySym ks;
        char ch[8];

        XLookupString(&ev->xkey, ch, sizeof ch, &ks, NULL);
        if (ks == XK_Escape) {
            hide_pop();
            stop_play();
            snprintf(status, sizeof status, "Stopped.");
            paint();
        } else if (ks == XK_space) {
            st_toggle_pause();
        } else if (ch[0] == '-' || ks == XK_minus) {
            if (vol > 0)
                vol -= 10;
            st_send_vol();
            paint_pop();
        } else if (ch[0] == '+' || ch[0] == '=') {
            if (vol < 100)
                vol += 10;
            st_send_vol();
            paint_pop();
        }
        return;
    }
    if (ev->type != ButtonPress)
        return;
    x = ev->xbutton.x;
    y = ev->xbutton.y;
    if (!hit(x, y, 16, 96, 540, 36))
        return;
    if (hit(x, y, 16, 96, 80, 36))
        st_toggle_pause();
    else if (hit(x, y, 108, 96, 110, 36))
        st_step(-1);
    else if (hit(x, y, 228, 96, 80, 36))
        st_step(1);
    else if (hit(x, y, 324, 96, 40, 36)) {
        if (vol > 0)
            vol -= 10;
        st_send_vol();
        paint_pop();
    } else if (hit(x, y, 412, 96, 40, 36)) {
        if (vol < 100)
            vol += 10;
        st_send_vol();
        paint_pop();
    } else if (hit(x, y, 464, 96, 76, 36)) {
        hide_pop();
        stop_play();
        snprintf(status, sizeof status, "Stopped.");
        paint();
    }
}

static int is_url(const char *s)
{
    return strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0;
}

/* A picture playlist is video. A radio codec is audio. */
static int st_is_video(const char *codec, const char *url)
{
    if (codec && (!strcmp(codec, "TV") || !strcmp(codec, "Video")))
        return 1;
    if (!url)
        return 0;
    if (strstr(url, ".m3u8") || strstr(url, ".smil") || strstr(url, "/hls/"))
        return 1;
    return 0;
}

static void st_media_kind(char *dst, int n, const char *codec, const char *rate, const char *url)
{
    const char *extra = (rate && rate[0]) ? rate : "";

    if (st_is_video(codec, url))
        snprintf(dst, n, "Video %s", extra);
    else if (codec && codec[0] && strcmp(codec, "Audio") && strcmp(codec, "URL"))
        snprintf(dst, n, "Audio %s %s", codec, extra);
    else
        snprintf(dst, n, "Audio %s", extra);
}

static void add_row(const char *title, const char *url, const char *codec, const char *rate)
{
    char kind[48];

    if (nres >= MAXR || !url || !url[0])
        return;
    fit_copy(titles[nres], (int)sizeof titles[0], title && title[0] ? title : url);
    fit_copy(urls[nres], (int)sizeof urls[0], url);
    st_media_kind(kind, (int)sizeof kind, codec, rate, url);
    fit_copy(kinds[nres], (int)sizeof kinds[0], kind);
    nres++;
}

static void take_line(char *line)
{
    char *b, *c, *d;
    size_t n;

    if (!line)
        return;
    n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = 0;
    if (n == 0)
        return;
    if (strncmp(line, "STATUS\t", 7) == 0) {
        fit_copy(status, (int)sizeof status, line + 7);
        return;
    }
    b = strchr(line, '\t');
    if (!b)
        return;
    *b++ = 0;
    c = strchr(b, '\t');
    if (c)
        *c++ = 0;
    d = c ? strchr(c, '\t') : NULL;
    if (d)
        *d++ = 0;
    if (strncmp(b, "http://", 7) != 0 && strncmp(b, "https://", 8) != 0)
        return;
    add_row(line, b, c ? c : "", d ? d : "");
}

static void eat(const char *buf, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (buf[i] == '\n') {
            acc[acc_n] = 0;
            take_line(acc);
            acc_n = 0;
            continue;
        }
        if (buf[i] == '\r')
            continue;
        if (acc_n < (int)sizeof acc - 1)
            acc[acc_n++] = buf[i];
    }
}

static void start_search(void)
{
    const char *bin;
    int pfd[2];
    pid_t pid;

    while (qlen > 0 && query[qlen - 1] == ' ')
        query[--qlen] = 0;
    if (qlen < 1) {
        snprintf(status, sizeof status, "Type a word, or paste a station address.");
        return;
    }
    if (is_url(query)) {
        stop_search();
        nres = 0;
        page = 0;
        add_row(query, query, "URL", "");
        play_i = 0;
        start_play(query, query);
        return;
    }
    bin = script_path("minios-streaming-search");
    if (!bin) {
        snprintf(status, sizeof status, "Search tool is missing.");
        return;
    }
    stop_search();
    nres = 0;
    page = 0;
    snprintf(status, sizeof status, "Searching stations.");
    if (pipe(pfd) != 0) {
        snprintf(status, sizeof status, "Search could not start.");
        return;
    }
    pid = fork();
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        snprintf(status, sizeof status, "Search could not start.");
        return;
    }
    if (pid == 0) {
        setpgid(0, 0);
        dup2(pfd[1], 1);
        close(pfd[0]);
        close(pfd[1]);
        execl(bin, "minios-streaming-search", query, (char *)0);
        _exit(127);
    }
    setpgid(pid, pid);
    close(pfd[1]);
    search_fd = pfd[0];
    fcntl(search_fd, F_SETFL, O_NONBLOCK);
    search_pid = pid;
    searching = 1;
    fprintf(stderr, "stream-v247 search\n");
}

static void layout(void)
{
    int fw = fs ? fs->max_bounds.width : 9;

    btn_h = 28;
    btn_y = 46;
    close_x = ww - 16 - 72;
    stop_x = close_x - 8 - 64;
    go_w = 110;
    go_x = stop_x - 8 - go_w;
    field_x = 96;
    field_w = go_x - 8 - field_x;
    if (field_w < 120)
        field_w = 120;
    list_y = 96;
    row_h = 36;
    pagebtn_w = 115;
    pagebtn_h = 23;
    pagebtn_y = wh - 8 - pagebtn_h;
    prev_x = 16;
    next_x = ww - 16 - pagebtn_w;
    page_rows = (pagebtn_y - list_y - 8) / row_h;
    if (page_rows < 4)
        page_rows = 4;
    if (page_rows > 18)
        page_rows = 18;
    (void)fw;
}

static void paint(void)
{
    int i, start, y, pages, shown;
    char line[120];
    char pagebuf[96];

    layout();
    fill(bg, 0, 0, ww, wh);
    text_at(fg, 16, 28, "MiniOS Streaming Player");
    text_at(fg, 16, btn_y + 18, "Term/URL");
    fill(field, field_x, btn_y, field_w, btn_h);
    frame(fg, field_x, btn_y, field_w, btn_h);
    fit_copy(line, 70, query);
    text_at(fg, field_x + 6, btn_y + 18, line);
    fill(btn, go_x, btn_y, go_w, btn_h);
    frame(fg, go_x, btn_y, go_w, btn_h);
    text_at(fg, go_x + 12, btn_y + 18, "Go search!");
    fill(btn, stop_x, btn_y, 64, btn_h);
    frame(fg, stop_x, btn_y, 64, btn_h);
    text_at(fg, stop_x + 14, btn_y + 18, "Stop");
    fill(btn, close_x, btn_y, 72, btn_h);
    frame(fg, close_x, btn_y, 72, btn_h);
    text_at(fg, close_x + 14, btn_y + 18, "Close");
    text_at(fg, 16, 88, status);
    start = page * page_rows;
    for (i = 0; i < page_rows && start + i < nres; i++) {
        y = list_y + i * row_h;
        if (st_audio_row_on() && start + i == play_i) {
            st_paint_audio_row(y, titles[start + i], kinds[start + i]);
            continue;
        }
        fill(i % 2 ? field : bg, 12, y, ww - 24, row_h - 4);
        frame(fg, 12, y, ww - 24, row_h - 4);
        {
            int maxc = (ww - 150) / 9;
            int video = strncmp(kinds[start + i], "Video", 5) == 0;
            if (maxc > 70)
                maxc = 70;
            if (maxc < 12)
                maxc = 12;
            fit_copy(line, maxc, titles[start + i]);
            text_at(fg, 20, y + 16, line);
            text_at(hi, 20, y + 30, kinds[start + i]);
            fill(video ? 0xD8E8FF : 0xE8F0E8, ww - 108, y + 6, 84, row_h - 14);
            frame(fg, ww - 108, y + 6, 84, row_h - 14);
            text_at(fg, ww - 90, y + 24, video ? "Video" : "Audio");
        }
    }
    pages = nres > 0 ? (nres + page_rows - 1) / page_rows : 0;
    shown = pages > 0 ? page + 1 : 0;
    if (pages < 1)
        snprintf(pagebuf, sizeof pagebuf, "Type a word, or paste http:// address.");
    else
        snprintf(pagebuf, sizeof pagebuf, "Page %d of %d (%d stations). Click one to play.",
                 shown, pages, nres);
    text_at(fg, prev_x + pagebtn_w + 16, pagebtn_y + 16, pagebuf);
    fill(btn, prev_x, pagebtn_y, pagebtn_w, pagebtn_h);
    frame(fg, prev_x, pagebtn_y, pagebtn_w, pagebtn_h);
    text_at(fg, prev_x + 16, pagebtn_y + 16, "<Previous");
    fill(btn, next_x, pagebtn_y, pagebtn_w, pagebtn_h);
    frame(fg, next_x, pagebtn_y, pagebtn_w, pagebtn_h);
    text_at(fg, next_x + 36, pagebtn_y + 16, "Next>");
    XFlush(dpy);
}

static int hit(int x, int y, int bx, int by, int bw, int bh)
{
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

static void finish(void)
{
    stop_search();
    stop_play();
    hide_pop();
    hide_video();
    unlink("/tmp/minios-streaming.ready");
}

static void pump_search(void)
{
    char buf[512];
    int n, st;
    fd_set fds;
    struct timeval tv;

    if (search_fd < 0)
        return;
    FD_ZERO(&fds);
    FD_SET(search_fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (select(search_fd + 1, &fds, NULL, NULL, &tv) <= 0)
        return;
    n = (int)read(search_fd, buf, sizeof buf);
    if (n > 0) {
        eat(buf, n);
        paint();
        return;
    }
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        close(search_fd);
        search_fd = -1;
        if (search_pid > 0) {
            waitpid(search_pid, &st, 0);
            search_pid = 0;
        }
        searching = 0;
        if (nres < 1 && strncmp(status, "No ", 3) != 0 && strncmp(status, "The ", 4) != 0)
            snprintf(status, sizeof status, "No station came back. Try another word, or paste an address.");
        else if (nres > 0)
            snprintf(status, sizeof status, "%d stations. Click one to play.", nres);
        paint();
    }
}

int main(void)
{
    XEvent ev;
    int wx, wy;

    signal(SIGPIPE, SIG_IGN);
    XSetErrorHandler(x_error);
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "stream-v247 no display\n");
        return 1;
    }
    scr = DefaultScreen(dpy);
    wx = DisplayWidth(dpy, scr);
    wy = DisplayHeight(dpy, scr);
    if (wx < 640)
        wx = 1024;
    if (wy < 480)
        wy = 768;
    /* Same edges as YouTube Search: 40 on the left, 56 for the bottom bar. */
    ww = wx - 40 - 4;
    wh = wy - 56 - 4;
    wx = 40;
    wy = 4;
    fprintf(stderr, "stream-v252 full\n");
    fprintf(stderr, "stream-v254 kind\n");
    st_keep_pop = show_pop;
    bg = 0xD8D8D8;
    fg = BlackPixel(dpy, scr);
    btn = 0xC0C0C0;
    field = 0xFFFFFF;
    hi = 0x203060;
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), wx, wy, (unsigned)ww, (unsigned)wh,
                              1, fg, bg);
    XStoreName(dpy, win, "MiniOS Streaming Player");
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    gc = XCreateGC(dpy, win, 0, NULL);
    fs = XLoadQueryFont(dpy, "9x15");
    if (!fs)
        fs = XLoadQueryFont(dpy, "fixed");
    if (fs)
        XSetFont(dpy, gc, fs->fid);
    snprintf(status, sizeof status, "Type a word, or paste a station address, then Go search!");
    XMapWindow(dpy, win);
    {
        FILE *rf = fopen("/tmp/minios-streaming.ready", "w");
        if (rf) {
            fputs("ready\n", rf);
            fclose(rf);
        }
    }
    fprintf(stderr, "stream-v247 open\n");
    for (;;) {
        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (vid_on && ev.xany.window == vid) {
                vid_event(&ev);
                continue;
            }
            if (pop_on && ev.xany.window == pop) {
                pop_event(&ev);
                continue;
            }
            if (ev.type == Expose && ev.xexpose.count == 0)
                paint();
            else if (ev.type == ConfigureNotify) {
                ww = ev.xconfigure.width;
                wh = ev.xconfigure.height;
                paint();
            } else if (ev.type == ClientMessage &&
                       (Atom)ev.xclient.data.l[0] == wm_delete) {
                finish();
                return 0;
            } else if (ev.type == KeyPress) {
                KeySym ks;
                char ch[8];
                int n = XLookupString(&ev.xkey, ch, sizeof ch, &ks, NULL);
                if (ks == XK_Return) {
                    start_search();
                    paint();
                } else if (ks == XK_BackSpace) {
                    if (qlen > 0)
                        query[--qlen] = 0;
                    paint();
                } else if (ks == XK_Escape) {
                    finish();
                    return 0;
                } else if (n == 1 && qlen < (int)sizeof query - 1 &&
                           (unsigned char)ch[0] >= 32 && (unsigned char)ch[0] < 127) {
                    query[qlen++] = ch[0];
                    query[qlen] = 0;
                    paint();
                }
            } else if (ev.type == ButtonPress) {
                int x = ev.xbutton.x, y = ev.xbutton.y, i, start;
                layout();
                if (hit(x, y, close_x, btn_y, 72, btn_h) ||
                    (ev.xbutton.button == 1 && x > ww - 20 && y < 20)) {
                    finish();
                    return 0;
                }
                if (hit(x, y, stop_x, btn_y, 64, btn_h)) {
                    stop_search();
                    st_stop_station();
                    continue;
                }
                if (hit(x, y, go_x, btn_y, go_w, btn_h)) {
                    start_search();
                    paint();
                    continue;
                }
                if (hit(x, y, prev_x, pagebtn_y, pagebtn_w, pagebtn_h) && page > 0) {
                    page--;
                    paint();
                    continue;
                }
                if (hit(x, y, next_x, pagebtn_y, pagebtn_w, pagebtn_h) &&
                    (page + 1) * page_rows < nres) {
                    page++;
                    paint();
                    continue;
                }
                start = page * page_rows;
                for (i = 0; i < page_rows && start + i < nres; i++) {
                    int ry = list_y + i * row_h;
                    if (st_audio_row_on() && start + i == play_i) {
                        int act = st_ctrl_hit(ww, ry + 4, row_h - 10, x, y);
                        if (act == 2) {
                            st_stop_station();
                            break;
                        }
                        if (act == 1)
                            break;
                    }
                    if (hit(x, y, 12, ry, ww - 24, row_h - 4)) {
                        play_i = start + i;
                        start_play(urls[start + i], titles[start + i]);
                        paint();
                        break;
                    }
                }
            }
        }
        pump_search();
        if (play_pid > 0 && waitpid(play_pid, NULL, WNOHANG) == play_pid) {
            play_pid = 0;
            if (time(NULL) - play_started < 3)
                snprintf(status, sizeof status, "This station did not start. Try another.");
            else
                snprintf(status, sizeof status, "The station stopped. Click another, or search again.");
            paint();
            if (vid_on)
                paint_video();
            else if (pop_on)
                paint_pop();
        }
        if (play_pid > 0 && !vol_sent &&
            time(NULL) - play_started >= 1) {
            st_send_vol();
            if (want_pause)
                st_cmd("pause\n");
            vol_sent = 1;
            st_show_controls();
        }
        if (vid_on && play_pid > 0 && time(NULL) != pic_lift) {
            st_raise_picture();
            pic_lift = time(NULL);
        }
        usleep(40000);
    }
}
