/* Historical X11 bar. The built binary is compiled from
 * minios-player-bar.cxx (FLTK black bar, cyan labels, v19).
 * MiniOS-Player-v16
 * Do not raise the backdrop on a timer (that flickered the resource strip
 * and hid the bar). Loading status lives on the bar. Quit always works.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMD "/tmp/minios-mp.cmd"
#define OUT "/tmp/minios-mp.out"
#define PIDF "/tmp/minios-mp.pid"
#define FAILF "/tmp/minios-mp.fail"
#define SUBF "/tmp/minios-ytplay.sub"
#define GEOMF "/tmp/minios-mp.geom"
#define PROGF "/tmp/minios-dl.progress"
#define PLF "/tmp/minios-yt-playlist"
#define SHORTF "/tmp/minios-yt-shorts"
#define BAR_H 80
#define TITLE "MiniOS Player"
#define TIKTOKF "/tmp/minios-yt-tiktok"
#define QUITF "/tmp/minios-mp.quit"
#define BARPID "/tmp/minios-player-bar.pid"
#define MAXPL 40
#define IC_PLAY 1
#define IC_PAUSE 2
#define IC_PREV 3
#define IC_NEXT 4

static Display *dpy;
static Window loadw, barw, mpwin;
static GC gc, lgc;
static int scr;
static int paused = 1;
static int pct;
static int tpos, tlen;
static int vol = 80;
static int autoplay = 1;
static int loop_mode; /* 0 off, 1 this clip, 2 whole list */
static int cc_on;
static int have_sub;
static int cc_user;
static int cc_fetching;
static int pause_user;
static int last_painted_pct = -1;
static int last_painted_paused = -1;
static int last_painted_cc = -1;
static int last_painted_vol = -1;
static int last_painted_auto = -1;
static int last_painted_loop = -1;
static int last_painted_tpos = -1;
static int last_painted_tlen = -1;
static int last_painted_dl = -1;
static int last_painted_skip = -1;
static char last_dl_lab[48];
static int have_playback;
static int load_failed;
static int dl_pct;
static char dl_lab[48];
static pid_t mpid;
static int dw, dh;
static unsigned long fg, bg, btn, fillc, track, accent, dim, hi;
static unsigned long quiet_until;
static void paint_bar(int force);
static void paint_load(void);
static void start_pl_item(int idx);
static void restack_play(void);
static void mark_seen(void);
static void read_progress(void);
static void blank_load(void);
static void request_quit(void);
static void grab_keys(void);
static void listen_video_clicks(Window w);
static int signal_mplayers(int sig);
static void send_cmd(const char *s);

static int bx_tog, bx_prev, bx_next, bx_auto, bx_loop, bx_add, bx_seek, bw_seek, bx_vm, bx_vp, bx_cc, bx_quit;
static Window tipw;
static char tip[64];
static int tip_on;
static const int by = 42, bh = 32;
static int shorts_on, queue_on, pl_count, pl_cur, user_quit, skip_busy;
static char pl_urls[MAXPL][160];

static unsigned long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)tv.tv_sec * 1000UL + (unsigned long)tv.tv_usec / 1000UL;
}

static int file_exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

static void dismiss_popup(void)
{
    if (file_exists("/opt/minios/minios-loading"))
        system("/opt/minios/minios-loading stop >/dev/null 2>&1");
    system("pkill -x xmessage >/dev/null 2>&1; rm -f /tmp/minios-run-alert.lock");
}

static void grab_keys(void)
{
    if (!dpy || !barw)
        return;
    XSetInputFocus(dpy, barw, RevertToParent, CurrentTime);
    XGrabKeyboard(dpy, barw, True, GrabModeAsync, GrabModeAsync, CurrentTime);
}

static void listen_video_clicks(Window w)
{
    XWindowAttributes wa;

    if (!dpy || w == None || w == loadw || w == barw || w == tipw)
        return;
    if (!XGetWindowAttributes(dpy, w, &wa))
        return;
    XSelectInput(dpy, w, wa.your_event_mask | ButtonPressMask);
    mpwin = w;
}

static void blank_load(void)
{
    if (!dpy || !loadw)
        return;
    XSetForeground(dpy, lgc, bg);
    XFillRectangle(dpy, loadw, lgc, 0, 0, dw, dh);
    XFlush(dpy);
}

static void request_quit(void)
{
    FILE *f;

    user_quit = 1;
    skip_busy = 0;
    autoplay = 0;
    f = fopen(QUITF, "w");
    if (f) {
        fputs("1\n", f);
        fclose(f);
    }
    dismiss_popup();
    signal_mplayers(SIGCONT);
    send_cmd("quit\n");
    signal_mplayers(SIGTERM);
    system("pkill -x minios-shorts-play >/dev/null 2>&1");
    system("pkill -f minios-yt-resolve >/dev/null 2>&1");
    load_failed = 1;
}

static int signal_mplayers(int sig)
{
    DIR *d;
    struct dirent *e;
    FILE *f;
    char path[64], name[32];
    pid_t p;
    int n = 0;

    d = opendir("/proc");
    if (!d)
        return 0;
    while ((e = readdir(d))) {
        if (!isdigit((unsigned char)e->d_name[0]))
            continue;
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        f = fopen(path, "r");
        if (!f)
            continue;
        if (!fgets(name, sizeof(name), f)) {
            fclose(f);
            continue;
        }
        fclose(f);
        if (strncmp(name, "mplayer", 7) != 0)
            continue;
        if (name[7] != '\n' && name[7] != '\0')
            continue;
        p = (pid_t)atoi(e->d_name);
        if (p > 1 && kill(p, sig) == 0)
            n++;
    }
    closedir(d);
    return n;
}

static void send_cmd(const char *s)
{
    int fd, n, left, flags;
    const char *p;

    /* Same as: echo "pause" > /tmp/minios-mp.cmd
       Blocking open so the write waits for mplayer, not O_NONBLOCK drop. */
    fd = open(CMD, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "player-v7 fifo open fail (%s) cmd=%s", strerror(errno), s);
        return;
    }
    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    p = s;
    left = (int)strlen(s);
    while (left > 0) {
        n = (int)write(fd, p, (size_t)left);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "player-v7 fifo write fail (%s) cmd=%s", strerror(errno), s);
            break;
        }
        p += n;
        left -= n;
    }
    close(fd);
}

static void read_answers(void)
{
    FILE *f;
    char line[192];

    f = fopen(OUT, "r");
    if (!f)
        return;
    if (fseek(f, -1600, SEEK_END) != 0)
        rewind(f);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "ANS_PERCENT_POSITION=", 21) == 0) {
            pct = atoi(line + 21);
            if (pct < 0)
                pct = 0;
            if (pct > 100)
                pct = 100;
            have_playback = 1;
        } else if (strncmp(line, "ANS_TIME_POSITION=", 18) == 0) {
            tpos = atoi(line + 18);
            if (tpos < 0)
                tpos = 0;
            have_playback = 1;
        } else if (strncmp(line, "ANS_LENGTH=", 11) == 0) {
            tlen = atoi(line + 11);
            if (tlen < 0)
                tlen = 0;
        } else if (strstr(line, "Starting playback") != NULL ||
                   strstr(line, "VO: [x11]") != NULL) {
            have_playback = 1;
        }
    }
    fclose(f);
}

static void read_pid_files(void)
{
    FILE *f;
    char buf[64];

    if (mpid <= 0 && file_exists(PIDF)) {
        f = fopen(PIDF, "r");
        if (f) {
            if (fgets(buf, sizeof(buf), f))
                mpid = (pid_t)atoi(buf);
            fclose(f);
            if (mpid > 0 && kill(mpid, 0) != 0) {
                mpid = 0;
                unlink(PIDF);
            } else if (mpid > 0) {
                have_playback = 1;
                if (!pause_user)
                    paused = 0;
                fprintf(stderr, "player-v17 wrapper_pid=%d\n", (int)mpid);
                dismiss_popup();
                blank_load();
                restack_play();
                grab_keys();
                paint_bar(1);
                XFlush(dpy);
            }
        }
    }
    if (file_exists(FAILF))
        load_failed = 1;
    if (!have_sub) {
        if (file_exists(SUBF) ||
            file_exists("/tmp/minios-ytplay.en.srt") ||
            file_exists("/tmp/minios-ytplay.en.vtt") ||
            file_exists("/tmp/minios-ytplay.srt")) {
            have_sub = 1;
            if (cc_fetching) {
                cc_on = 1;
                cc_user = 1;
                cc_fetching = 0;
                send_cmd("pausing_keep_force sub_load /tmp/minios-ytplay.en.srt\n");
                send_cmd("pausing_keep_force sub_visibility 1\n");
                fprintf(stderr, "player-v11 cc loaded on click\n");
            } else if (!cc_user && !shorts_on)
                cc_on = 1;
        }
    }
}

static void clear_subs(void)
{
    unlink(SUBF);
    unlink("/tmp/minios-ytplay.en.srt");
    unlink("/tmp/minios-ytplay.en.vtt");
    unlink("/tmp/minios-ytplay.srt");
    have_sub = 0;
    cc_on = 0;
    cc_user = 0;
    cc_fetching = 0;
}

static void request_cc(void)
{
    const char *bin, *url;

    if (cc_fetching || have_sub)
        return;
    url = (shorts_on && pl_count > 0) ? pl_urls[pl_cur] : "";
    if (!url[0])
        return;
    cc_fetching = 1;
    bin = "/opt/minios/minios-yt-resolve";
    if (access(bin, X_OK) != 0)
        bin = "/usr/local/bin/minios-yt-resolve";
    if (fork() == 0) {
        execl(bin, "minios-yt-resolve", "--captions", url, (char *)0);
        _exit(127);
    }
    fprintf(stderr, "player-v11 cc fetch %s\n", url);
}

static void load_playlist(void)
{
    FILE *f;
    char line[160];

    shorts_on = file_exists(SHORTF) || file_exists(TIKTOKF);
    pl_count = 0;
    pl_cur = 0;
    if (!file_exists(PLF))
        return;
    f = fopen(PLF, "r");
    if (!f)
        return;
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return;
    }
    pl_cur = atoi(line);
    while (pl_count < MAXPL && fgets(line, sizeof(line), f)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r'))
            line[--L] = 0;
        if (!L)
            continue;
        snprintf(pl_urls[pl_count], sizeof(pl_urls[0]), "%s", line);
        pl_count++;
    }
    fclose(f);
    if (pl_cur < 0)
        pl_cur = 0;
    if (pl_count > 0 && pl_cur >= pl_count)
        pl_cur = 0;
    queue_on = (pl_count > 1);
}

static void save_pl_index(void)
{
    FILE *f;
    int i;

    f = fopen(PLF, "w");
    if (!f)
        return;
    fprintf(f, "%d\n", pl_cur);
    for (i = 0; i < pl_count; i++)
        fprintf(f, "%s\n", pl_urls[i]);
    fclose(f);
}

static void mark_seen(void)
{
    FILE *sf;
    const char *u, *id;

    if (file_exists(TIKTOKF))
        return;
    if (!shorts_on || pl_count <= 0)
        return;
    u = pl_urls[pl_cur];
    if (!u[0])
        return;
    if (strstr(u, "tiktok.com"))
        return;
    id = strrchr(u, '/');
    sf = fopen("/tmp/minios-yt-seen", "a");
    if (!sf)
        return;
    fprintf(sf, "%s|%s\n", id ? id + 1 : u, u);
    fclose(sf);
}

/* Keep the black backdrop above the desktop, under the video. Never send it
 * to the bottom (that lets the wallpaper show on the sides and through gaps). */
static int is_mplayer_window(Window w)
{
    XClassHint ch;
    char *name = NULL;
    int hit = 0;

    if (XFetchName(dpy, w, &name) && name) {
        if (strstr(name, "MPlayer") || strstr(name, "mplayer") ||
            strstr(name, "ytplay") || strstr(name, ".mp4"))
            hit = 1;
        XFree(name);
        if (hit)
            return 1;
    }
    memset(&ch, 0, sizeof(ch));
    if (XGetClassHint(dpy, w, &ch)) {
        if (ch.res_class && (strstr(ch.res_class, "MPlayer") ||
                             strstr(ch.res_class, "mplayer")))
            hit = 1;
        if (ch.res_name && (strstr(ch.res_name, "MPlayer") ||
                            strstr(ch.res_name, "mplayer")))
            hit = 1;
        if (ch.res_name)
            XFree(ch.res_name);
        if (ch.res_class)
            XFree(ch.res_class);
    }
    return hit;
}

static void restack_play(void)
{
    Window root, parent, *kids = NULL;
    unsigned n = 0, i;
    int found = 0;

    if (XQueryTree(dpy, RootWindow(dpy, scr), &root, &parent, &kids, &n)) {
        for (i = 0; i < n; i++) {
            if (kids[i] == loadw || kids[i] == barw || kids[i] == tipw)
                continue;
            if (is_mplayer_window(kids[i])) {
                found = 1;
                listen_video_clicks(kids[i]);
                break;
            }
        }
        if (found) {
            for (i = 0; i < n; i++) {
                if (kids[i] == loadw || kids[i] == barw || kids[i] == tipw)
                    continue;
                if (is_mplayer_window(kids[i])) {
                    listen_video_clicks(kids[i]);
                    XRaiseWindow(dpy, kids[i]);
                }
            }
        }
        if (kids)
            XFree(kids);
    }
    XRaiseWindow(dpy, barw);
    if (tip_on)
        XRaiseWindow(dpy, tipw);
    XFlush(dpy);
}

static void stop_current_mplayer(void)
{
    int i;

    send_cmd("quit\n");
    signal_mplayers(SIGTERM);
    for (i = 0; i < 30; i++) {
        if (signal_mplayers(0) == 0)
            break;
        usleep(40000);
    }
    if (signal_mplayers(0))
        signal_mplayers(SIGKILL);
    usleep(80000);
    mpid = 0;
}

static void write_geom(void)
{
    FILE *gf;
    int vw, vh, vx, vy;

    vh = dh - BAR_H;
    if (vh < 80)
        vh = dh / 2;
    if (shorts_on) {
        /* 9:16 portrait, centered. Do not stretch a Short into 1024x716. */
        vw = vh * 9 / 16;
        if (vw < 160)
            vw = 160;
        if (vw > dw - 8)
            vw = dw - 8;
        vx = (dw - vw) / 2;
        if (vx < 0)
            vx = 0;
        vy = 0;
    } else {
        vw = dw;
        vx = 0;
        vy = 0;
    }
    gf = fopen(GEOMF, "w");
    if (gf) {
        fprintf(gf, "%dx%d+%d+%d\n", vw, vh, vx, vy);
        fclose(gf);
    }
    fprintf(stderr, "player-v11 geom=%dx%d+%d+%d shorts=%d\n", vw, vh, vx, vy, shorts_on);
}

static void start_pl_item(int idx)
{
    const char *bin;

    if (pl_count <= 0 || user_quit || file_exists(QUITF))
        return;
    if (idx < 0) {
        if (loop_mode != 2)
            return;
        idx = pl_count - 1;
    }
    if (idx >= pl_count) {
        if (loop_mode != 2)
            return;
        idx = 0;
    }
    skip_busy = 1;
    have_playback = 0;
    dismiss_popup();
    clear_subs();
    stop_current_mplayer();
    pl_cur = idx;
    save_pl_index();
    mark_seen();
    if (!pause_user)
        paused = 1;
    pct = 0;
    tpos = 0;
    tlen = 0;
    unlink(PIDF);
    unlink(FAILF);
    {
        FILE *pf = fopen(PROGF, "w");
        if (pf) {
            fprintf(pf, "0\nLoading next video\n");
            fclose(pf);
        }
    }
    write_geom();
    bin = "/opt/minios/minios-shorts-play";
    if (access(bin, X_OK) != 0)
        bin = "/usr/local/bin/minios-shorts-play";
    if (fork() == 0) {
        execl(bin, "minios-shorts-play", pl_urls[pl_cur], (char *)0);
        _exit(127);
    }
    /* Backdrop is already mapped behind the old picture. Do not raise it
     * over the bar or the resource strip. */
    paint_load();
    paint_bar(1);
    XRaiseWindow(dpy, barw);
    XSetInputFocus(dpy, barw, RevertToParent, CurrentTime);
    fprintf(stderr, "player-v16 next clip %d %s\n", pl_cur, pl_urls[pl_cur]);
}

static void fmt_time(char *buf, size_t n, int sec)
{
    if (sec < 0)
        sec = 0;
    if (sec >= 3600)
        snprintf(buf, n, "%d:%02d:%02d", sec / 3600, (sec / 60) % 60, sec % 60);
    else
        snprintf(buf, n, "%d:%02d", sec / 60, sec % 60);
}

static void layout(void)
{
    bx_seek = 72;
    bw_seek = dw - 72 - 80;
    if (bw_seek < 80)
        bw_seek = 80;
    bx_tog = 10;
    if (queue_on) {
        bx_prev = 58;
        bx_next = 102;
        bx_auto = 150;
        bx_loop = 206;
        bx_add = 262;
    } else {
        bx_prev = -400;
        bx_next = -400;
        bx_auto = -400;
        bx_loop = -400;
        bx_add = -400;
    }
    bx_quit = dw - 64;
    bx_cc = dw - 112;
    bx_vp = dw - 154;
    bx_vm = dw - 196;
}

static void draw_btn_face(int x, int y, int width, int height, unsigned long fill)
{
    XSetForeground(dpy, gc, fill);
    XFillRectangle(dpy, barw, gc, x, y, width, height);
    XSetForeground(dpy, gc, hi);
    XFillRectangle(dpy, barw, gc, x + 1, y + 1, width - 2, 2);
    XSetForeground(dpy, gc, dim);
    XDrawRectangle(dpy, barw, gc, x, y, width, height);
}

static void draw_btn(int x, int y, int width, int height, unsigned long fill,
                    const char *lab, int labx)
{
    int tw = (int)strlen(lab) * 6;
    draw_btn_face(x, y, width, height, fill);
    XSetForeground(dpy, gc, fg);
    XDrawString(dpy, barw, gc, labx ? labx : (x + (width - tw) / 2), y + 21,
                lab, (int)strlen(lab));
}

static void draw_icon_btn(int x, int y, int w, int h, unsigned long fill, int kind)
{
    int cx = x + w / 2, cy = y + h / 2;
    XPoint t[3];

    draw_btn_face(x, y, w, h, fill);
    XSetForeground(dpy, gc, fg);
    if (kind == IC_PLAY) {
        t[0].x = (short)(cx - 5);
        t[0].y = (short)(cy - 8);
        t[1].x = (short)(cx - 5);
        t[1].y = (short)(cy + 8);
        t[2].x = (short)(cx + 9);
        t[2].y = (short)cy;
        XFillPolygon(dpy, barw, gc, t, 3, Convex, CoordModeOrigin);
    } else if (kind == IC_PAUSE) {
        XFillRectangle(dpy, barw, gc, cx - 7, cy - 8, 5, 16);
        XFillRectangle(dpy, barw, gc, cx + 3, cy - 8, 5, 16);
    } else if (kind == IC_PREV) {
        t[0].x = (short)(cx + 2);
        t[0].y = (short)(cy - 7);
        t[1].x = (short)(cx + 2);
        t[1].y = (short)(cy + 7);
        t[2].x = (short)(cx - 8);
        t[2].y = (short)cy;
        XFillPolygon(dpy, barw, gc, t, 3, Convex, CoordModeOrigin);
        XFillRectangle(dpy, barw, gc, cx - 10, cy - 7, 3, 14);
    } else if (kind == IC_NEXT) {
        t[0].x = (short)(cx - 2);
        t[0].y = (short)(cy - 7);
        t[1].x = (short)(cx - 2);
        t[1].y = (short)(cy + 7);
        t[2].x = (short)(cx + 8);
        t[2].y = (short)cy;
        XFillPolygon(dpy, barw, gc, t, 3, Convex, CoordModeOrigin);
        XFillRectangle(dpy, barw, gc, cx + 8, cy - 7, 3, 14);
    }
}

static void paint_bar(int force)
{
    char lab[16], t1[16], t2[16];
    int fill;

    if (!force &&
        pct == last_painted_pct &&
        paused == last_painted_paused &&
        cc_on == last_painted_cc &&
        vol == last_painted_vol &&
        autoplay == last_painted_auto &&
        loop_mode == last_painted_loop &&
        tpos == last_painted_tpos &&
        tlen == last_painted_tlen &&
        dl_pct == last_painted_dl &&
        skip_busy == last_painted_skip &&
        strcmp(dl_lab, last_dl_lab) == 0)
        return;
    last_painted_pct = pct;
    last_painted_paused = paused;
    last_painted_cc = cc_on;
    last_painted_vol = vol;
    last_painted_auto = autoplay;
    last_painted_loop = loop_mode;
    last_painted_tpos = tpos;
    last_painted_tlen = tlen;
    last_painted_dl = dl_pct;
    last_painted_skip = skip_busy;
    snprintf(last_dl_lab, sizeof(last_dl_lab), "%s", dl_lab);

    XSetForeground(dpy, gc, bg);
    XFillRectangle(dpy, barw, gc, 0, 0, dw, BAR_H);
    XSetForeground(dpy, gc, fillc);
    XFillRectangle(dpy, barw, gc, 0, 0, dw, 3);

    fmt_time(t1, sizeof(t1), tpos);
    fmt_time(t2, sizeof(t2), tlen);
    XSetForeground(dpy, gc, fg);
    XDrawString(dpy, barw, gc, 8, 18, t1, (int)strlen(t1));
    XDrawString(dpy, barw, gc, dw - 72, 18, t2, (int)strlen(t2));

    XSetForeground(dpy, gc, track);
    XFillRectangle(dpy, barw, gc, bx_seek, 8, bw_seek, 10);
    fill = have_playback ? (bw_seek * pct / 100) : 0;
    if (tlen > 0)
        fill = bw_seek * tpos / tlen;
    if (fill > bw_seek)
        fill = bw_seek;
    if (fill > 0) {
        XSetForeground(dpy, gc, fillc);
        XFillRectangle(dpy, barw, gc, bx_seek, 8, fill, 10);
    }
    XSetForeground(dpy, gc, fg);
    XDrawRectangle(dpy, barw, gc, bx_seek, 8, bw_seek, 10);
    if (fill > 0) {
        int kx = bx_seek + fill - 4;
        if (kx < bx_seek)
            kx = bx_seek;
        XFillRectangle(dpy, barw, gc, kx, 5, 8, 16);
    }
    {
        char st[96];
        int loading = (skip_busy || !have_playback || mpid <= 0);
        if (loading) {
            read_progress();
            if (pl_count > 0)
                snprintf(st, sizeof(st), "Clip %d of %d   %s %d%%",
                         pl_cur + 1, pl_count,
                         dl_lab[0] ? dl_lab : "Loading video", dl_pct);
            else
                snprintf(st, sizeof(st), "%s %d%%",
                         dl_lab[0] ? dl_lab : "Loading video", dl_pct);
        } else if (pl_count > 0) {
            snprintf(st, sizeof(st), "Playing clip %d of %d",
                     pl_cur + 1, pl_count);
        } else {
            snprintf(st, sizeof(st), "Playing");
        }
        XSetForeground(dpy, gc, fg);
        XDrawString(dpy, barw, gc, 10, 36, st, (int)strlen(st));
    }

    draw_icon_btn(bx_tog, by, 44, bh, paused ? btn : accent,
                  paused ? IC_PLAY : IC_PAUSE);
    if (queue_on) {
        draw_icon_btn(bx_prev, by, 40, bh, btn, IC_PREV);
        draw_icon_btn(bx_next, by, 40, bh, btn, IC_NEXT);
        draw_btn(bx_auto, by, 52, bh, autoplay ? accent : btn, "Auto", 0);
        {
            const char *ll = "Loop";
            if (loop_mode == 1)
                ll = "One";
            else if (loop_mode == 2)
                ll = "All";
            draw_btn(bx_loop, by, 52, bh, loop_mode ? accent : btn, ll, 0);
        }
        draw_btn(bx_add, by, 48, bh, btn, "Add", 0);
    }

    snprintf(lab, sizeof(lab), "%d", vol);
    XSetForeground(dpy, gc, fg);
    XDrawString(dpy, barw, gc, bx_vm - 26, by + 21, lab, (int)strlen(lab));
    draw_btn(bx_vm, by, 36, bh, btn, "-", 0);
    draw_btn(bx_vp, by, 36, bh, btn, "+", 0);
    draw_btn(bx_cc, by, 40, bh,
             (cc_on && have_sub) ? accent : (cc_fetching ? hi : btn), "CC", 0);
    draw_btn(bx_quit, by, 52, bh, dim, "Quit", 0);
    XFlush(dpy);
}

static void read_progress(void)
{
    FILE *f;
    char line[48];
    int n;

    f = fopen(PROGF, "r");
    if (!f)
        return;
    if (fgets(line, sizeof(line), f)) {
        n = atoi(line);
        if (n < 0)
            n = 0;
        if (n > 100)
            n = 100;
        dl_pct = n;
    }
    if (fgets(line, sizeof(line), f)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r'))
            line[--L] = 0;
        if (L >= sizeof(dl_lab))
            L = sizeof(dl_lab) - 1;
        memcpy(dl_lab, line, L);
        dl_lab[L] = 0;
    }
    fclose(f);
}

static void paint_load(void)
{
    const char *msg;
    char pcts[16];
    int x, bar_w, bar_x, bar_y, fill;
    int playing = (have_playback && !skip_busy && mpid > 0 && kill(mpid, 0) == 0);

    /* Never paint or raise over a live picture. Raising here hid the bar
     * and flickered the resource strip. */
    if (playing)
        return;

    XSetForeground(dpy, lgc, bg);
    XFillRectangle(dpy, loadw, lgc, 0, 0, dw, dh);
    read_progress();
    XSetForeground(dpy, lgc, fg);
    if (load_failed)
        msg = "Could not load the video.";
    else if (dl_lab[0])
        msg = dl_lab;
    else
        msg = "Loading video. Please wait.";
    x = (dw / 2) - ((int)strlen(msg) * 5);
    if (x < 16)
        x = 16;
    XDrawString(dpy, loadw, lgc, x, dh / 2 - 36, msg, (int)strlen(msg));
    if (pl_count > 0) {
        char extra[80];
        snprintf(extra, sizeof(extra), "Search result %d of %d",
                 pl_cur + 1, pl_count);
        x = (dw / 2) - ((int)strlen(extra) * 5);
        if (x < 16)
            x = 16;
        XDrawString(dpy, loadw, lgc, x, dh / 2 - 16, extra, (int)strlen(extra));
    }

    bar_w = dw * 2 / 3;
    if (bar_w < 160)
        bar_w = dw - 40;
    bar_x = (dw - bar_w) / 2;
    bar_y = dh / 2 + 12;
    XSetForeground(dpy, lgc, track);
    XFillRectangle(dpy, loadw, lgc, bar_x, bar_y, bar_w, 18);
    fill = bar_w * dl_pct / 100;
    if (fill > 0) {
        XSetForeground(dpy, lgc, fillc);
        XFillRectangle(dpy, loadw, lgc, bar_x, bar_y, fill, 18);
    }
    XSetForeground(dpy, lgc, fg);
    XDrawRectangle(dpy, loadw, lgc, bar_x, bar_y, bar_w, 18);
    snprintf(pcts, sizeof(pcts), "%d%%", dl_pct);
    XDrawString(dpy, loadw, lgc, bar_x + bar_w + 8, bar_y + 14, pcts, (int)strlen(pcts));
    XFlush(dpy);
}

static void toggle_pause(const char *why)
{
    /* Exactly: echo "pause" > /tmp/minios-mp.cmd */
    send_cmd("pause\n");
    paused = !paused;
    pause_user = 1;
    quiet_until = now_ms() + 1500;
    fprintf(stderr, "player-v8 %s wrote pause now=%d\n", why, paused);
}

static void click_bar(int x, int y)
{
    char buf[48];
    int p;

    if (y >= 4 && y <= 22 && x >= bx_seek && x < bx_seek + bw_seek &&
        (have_playback || mpid > 0)) {
        p = (x - bx_seek) * 100 / bw_seek;
        if (p < 0)
            p = 0;
        if (p > 100)
            p = 100;
        snprintf(buf, sizeof(buf), "pausing_keep_force seek %d 1\n", p);
        send_cmd(buf);
        pct = p;
        if (tlen > 0)
            tpos = tlen * p / 100;
        paint_bar(1);
        return;
    }
    if (y < by - 2 || y > by + bh + 2)
        return;
    if (x >= bx_quit && x < bx_quit + 56) {
        fprintf(stderr, "player-v17 quit click\n");
        request_quit();
        return;
    }
    if (queue_on && x >= bx_prev && x < bx_prev + 40) {
        start_pl_item(pl_cur - 1);
        paint_bar(1);
        return;
    }
    if (queue_on && x >= bx_next && x < bx_next + 40) {
        start_pl_item(pl_cur + 1);
        paint_bar(1);
        return;
    }
    if (queue_on && x >= bx_auto && x < bx_auto + 52) {
        autoplay = !autoplay;
        paint_bar(1);
        return;
    }
    if (queue_on && x >= bx_loop && x < bx_loop + 52) {
        loop_mode = (loop_mode + 1) % 3;
        paint_bar(1);
        return;
    }
    if (queue_on && x >= bx_add && x < bx_add + 48) {
        mark_seen();
        paint_bar(1);
        return;
    }
    if (!have_playback && !mpid)
        return;
    if (x >= bx_tog && x < bx_tog + 44) {
        toggle_pause("toggle");
        paint_bar(1);
        return;
    }
    if (x >= bx_vm && x < bx_vm + 36) {
        vol -= 10;
        if (vol < 0)
            vol = 0;
        snprintf(buf, sizeof(buf), "pausing_keep_force volume %d 1\n", vol);
        send_cmd(buf);
        fprintf(stderr, "player-v7 vol %d\n", vol);
        paint_bar(1);
        return;
    }
    if (x >= bx_vp && x < bx_vp + 36) {
        vol += 10;
        if (vol > 100)
            vol = 100;
        snprintf(buf, sizeof(buf), "pausing_keep_force volume %d 1\n", vol);
        send_cmd(buf);
        fprintf(stderr, "player-v7 vol %d\n", vol);
        paint_bar(1);
        return;
    }
    if (x >= bx_cc && x < bx_cc + 40) {
        if (have_sub) {
            cc_on = !cc_on;
            cc_user = 1;
            send_cmd(cc_on ? "pausing_keep_force sub_visibility 1\n"
                           : "pausing_keep_force sub_visibility 0\n");
        } else if (shorts_on && !cc_fetching) {
            request_cc();
        }
        fprintf(stderr, "player-v11 cc on=%d have=%d fetch=%d\n",
                cc_on, have_sub, cc_fetching);
        paint_bar(1);
    }
}

static void key_cmd(KeySym ks)
{
    char buf[48];

    if (ks == XK_space)
        toggle_pause("space");
    else if ((ks == XK_n || ks == XK_N || ks == XK_Down) && queue_on) {
        signal_mplayers(SIGTERM);
        start_pl_item(pl_cur + 1);
    } else if ((ks == XK_p || ks == XK_P) && queue_on) {
        signal_mplayers(SIGTERM);
        start_pl_item(pl_cur - 1);
    } else if ((ks == XK_a || ks == XK_A) && queue_on) {
        autoplay = !autoplay;
    } else if ((ks == XK_l || ks == XK_L) && queue_on) {
        loop_mode = (loop_mode + 1) % 3;
    } else if (ks == XK_Left)
        send_cmd("seek -10 0\n");
    else if (ks == XK_Right)
        send_cmd("seek 10 0\n");
    else if (ks == XK_minus || ks == XK_KP_Subtract || ks == XK_underscore ||
             ks == XK_9) {
        vol -= 10;
        if (vol < 0)
            vol = 0;
        snprintf(buf, sizeof(buf), "pausing_keep_force volume %d 1\n", vol);
        send_cmd(buf);
    } else if (ks == XK_plus || ks == XK_KP_Add || ks == XK_equal ||
               ks == XK_0 || ks == XK_Up) {
        vol += 10;
        if (vol > 100)
            vol = 100;
        snprintf(buf, sizeof(buf), "pausing_keep_force volume %d 1\n", vol);
        send_cmd(buf);
    } else if (ks == XK_m || ks == XK_M)
        send_cmd("mute\n");
    else if (ks == XK_c || ks == XK_C)
        click_bar(bx_cc + 4, by + 4);
    else if (ks == XK_q || ks == XK_Q || ks == XK_Escape) {
        request_quit();
        return;
    }
    paint_bar(1);
}

static int mplayer_alive(void)
{
    if (mpid <= 0)
        return 1;
    return kill(mpid, 0) == 0;
}

int main(int argc, char **argv)
{
    XSetWindowAttributes swa;
    XColor col, dummy;
    Colormap cmap;
    XEvent ev;
    XFontStruct *fs;
    int xfd, vh;
    unsigned long last = 0, started;

    if (argc > 1)
        mpid = (pid_t)atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN);
    {
        FILE *pf = fopen(BARPID, "r");
        pid_t other = 0;
        if (pf) {
            if (fscanf(pf, "%d", &other) != 1)
                other = 0;
            fclose(pf);
        }
        if (other > 1 && other != getpid() && kill(other, 0) == 0) {
            fprintf(stderr, "player-v17 already running pid=%d\n", (int)other);
            return 0;
        }
        pf = fopen(BARPID, "w");
        if (pf) {
            fprintf(pf, "%d\n", (int)getpid());
            fclose(pf);
        }
    }

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "player-v7 no display\n");
        return 1;
    }
    scr = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, scr);
    bg = BlackPixel(dpy, scr);
    fg = WhitePixel(dpy, scr);
    btn = track = fillc = accent = dim = hi = WhitePixel(dpy, scr);
    if (XAllocNamedColor(dpy, cmap, "#303030", &col, &dummy))
        btn = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#202020", &col, &dummy))
        track = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#c8a000", &col, &dummy))
        fillc = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#8a7000", &col, &dummy))
        accent = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#505050", &col, &dummy))
        dim = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#4a4a4a", &col, &dummy))
        hi = col.pixel;

    dw = DisplayWidth(dpy, scr);
    dh = DisplayHeight(dpy, scr);
    vh = dh - BAR_H;
    if (vh < 80)
        vh = dh / 2;
    load_playlist();
    if (shorts_on)
        clear_subs();
    mark_seen();
    layout();
    write_geom();

    swa.override_redirect = True;
    swa.background_pixel = bg;
    swa.border_pixel = bg;
    swa.backing_store = WhenMapped;
    swa.save_under = False;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;
    loadw = XCreateWindow(dpy, RootWindow(dpy, scr), 0, 0, dw, dh, 0,
                          CopyFromParent, InputOutput, CopyFromParent,
                          CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
                          &swa);
    XStoreName(dpy, loadw, TITLE);
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | PointerMotionMask;
    barw = XCreateWindow(dpy, RootWindow(dpy, scr), 0, vh, dw, BAR_H, 0,
                         CopyFromParent, InputOutput, CopyFromParent,
                         CWOverrideRedirect | CWBackPixel | CWBackingStore |
                         CWEventMask, &swa);
    XStoreName(dpy, barw, TITLE);
    swa.event_mask = ExposureMask;
    tipw = XCreateWindow(dpy, RootWindow(dpy, scr), 0, 0, 80, 20, 0,
                         CopyFromParent, InputOutput, CopyFromParent,
                         CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
                         &swa);
    XStoreName(dpy, tipw, "tip");

    gc = XCreateGC(dpy, barw, 0, NULL);
    lgc = XCreateGC(dpy, loadw, 0, NULL);
    fs = XLoadQueryFont(dpy, "10x20");
    if (!fs)
        fs = XLoadQueryFont(dpy, "9x15");
    if (!fs)
        fs = XLoadQueryFont(dpy, "fixed");
    if (fs) {
        XSetFont(dpy, gc, fs->fid);
        XSetFont(dpy, lgc, fs->fid);
    }

    dismiss_popup();
    XMapWindow(dpy, loadw);
    XMapRaised(dpy, barw);
    grab_keys();
    paint_load();
    paint_bar(1);
    if (mpid > 0) {
        have_playback = 1;
        if (!pause_user)
            paused = 0;
        restack_play();
    }

    started = now_ms();
    xfd = ConnectionNumber(dpy);
    fprintf(stderr, "player-v18 screen=%dx%d shorts=%d queue=%d n=%d loop=%d\n",
            dw, dh, shorts_on, queue_on, pl_count, loop_mode);

    while (1) {
        struct timeval tv;
        fd_set rfds;
        unsigned long now;
        int was_playing = (mpid > 0);

        read_pid_files();
        if (file_exists(QUITF))
            request_quit();
        if (skip_busy && mpid > 0 && kill(mpid, 0) == 0) {
            skip_busy = 0;
            have_playback = 1;
            if (!pause_user)
                paused = 0;
            blank_load();
            restack_play();
            grab_keys();
            started = now_ms();
        }
        now = now_ms();
        if (now - last >= 400) {
            if (mpid > 0 && now >= quiet_until && !paused && !skip_busy) {
                send_cmd("pausing_keep_force get_time_pos\n");
                send_cmd("pausing_keep_force get_time_length\n");
                send_cmd("pausing_keep_force get_percent_pos\n");
            }
            read_answers();
            if (shorts_on && tlen > 180 && !skip_busy && have_playback &&
                !user_quit && queue_on) {
                fprintf(stderr, "player-v18 skip long clip tlen=%d\n", tlen);
                tlen = 0;
                start_pl_item(pl_cur + 1);
            }
            if (skip_busy || !have_playback || mpid <= 0 || kill(mpid, 0) != 0)
                paint_load();
            paint_bar(0);
            last = now;
        }

        if (user_quit)
            break;
        if (was_playing && mpid > 0 && !mplayer_alive()) {
            mpid = 0;
            have_playback = 0;
            if (user_quit)
                break;
            if (skip_busy) {
                /* Next/Prev already started the following clip. Stay up. */
                started = now_ms();
            } else if (loop_mode == 1 && pl_count > 0) {
                start_pl_item(pl_cur);
                started = now_ms();
            } else if (queue_on && (autoplay || loop_mode == 2)) {
                if (pl_cur + 1 >= pl_count && loop_mode != 2) {
                    snprintf(dl_lab, sizeof(dl_lab), "End of list");
                    dl_pct = 100;
                    paint_load();
                } else {
                    start_pl_item(pl_cur + 1);
                }
                started = now_ms();
            } else {
                snprintf(dl_lab, sizeof(dl_lab), "Clip finished");
                dl_pct = 100;
                paint_load();
            }
        }
        if (mpid <= 0 && !load_failed && !skip_busy && now - started > 240000UL) {
            fprintf(stderr, "player-v14 load timeout\n");
            load_failed = 1;
            paint_load();
        }

        tv.tv_sec = 0;
        tv.tv_usec = 150000;
        FD_ZERO(&rfds);
        FD_SET(xfd, &rfds);
        select(xfd + 1, &rfds, NULL, NULL, &tv);
        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                if (ev.xexpose.window == barw)
                    paint_bar(1);
                else if (ev.xexpose.window == loadw)
                    paint_load();
            } else if (ev.type == ButtonPress && ev.xbutton.window == barw) {
                if (tip_on)
                    XUnmapWindow(dpy, tipw);
                tip_on = 0;
                click_bar(ev.xbutton.x, ev.xbutton.y);
            } else if (ev.type == ButtonPress &&
                       ev.xbutton.window != tipw &&
                       (have_playback || mpid > 0)) {
                /* Click the picture (or the black sides) to play or pause. */
                if (tip_on)
                    XUnmapWindow(dpy, tipw);
                tip_on = 0;
                toggle_pause("picture");
                paint_bar(1);
            } else if (ev.type == MotionNotify && ev.xmotion.window == barw) {
                int mx = ev.xmotion.x, my = ev.xmotion.y;
                const char *s = NULL;
                if (my >= by - 2) {
                    if (mx >= bx_tog && mx < bx_tog + 44)
                        s = "Play or pause";
                    else if (queue_on && mx >= bx_prev && mx < bx_prev + 40)
                        s = "Previous clip";
                    else if (queue_on && mx >= bx_next && mx < bx_next + 40)
                        s = "Next clip. Player stays open.";
                    else if (queue_on && mx >= bx_auto && mx < bx_auto + 52)
                        s = "Play the next clip when this one ends";
                    else if (queue_on && mx >= bx_loop && mx < bx_loop + 52)
                        s = "Off, then this clip (One), then whole list (All)";
                    else if (queue_on && mx >= bx_add && mx < bx_add + 48)
                        s = "Add this clip to FLTube My Lists";
                    else if (mx >= bx_vm && mx < bx_vm + 36)
                        s = "Volume down. Keyboard - also works.";
                    else if (mx >= bx_vp && mx < bx_vp + 36)
                        s = "Volume up. Keyboard + also works.";
                    else if (mx >= bx_cc && mx < bx_cc + 40)
                        s = shorts_on ? "Click to download captions" : "Captions on or off";
                    else if (mx >= bx_quit && mx < bx_quit + 52)
                        s = "Close the player";
                }
                if (s) {
                    int tw = (int)strlen(s) * 10 + 16, th = 26, tx = mx;
                    if (tx + tw > dw)
                        tx = dw - tw;
                    if (tx < 0)
                        tx = 0;
                    XMoveResizeWindow(dpy, tipw, tx, dh - BAR_H - 28, tw, th);
                    XMapRaised(dpy, tipw);
                    XSetForeground(dpy, gc, bg);
                    XFillRectangle(dpy, tipw, gc, 0, 0, tw, th);
                    XSetForeground(dpy, gc, fg);
                    XDrawRectangle(dpy, tipw, gc, 0, 0, tw - 1, th - 1);
                    XDrawString(dpy, tipw, gc, 8, 18, s, (int)strlen(s));
                    tip_on = 1;
                } else if (tip_on) {
                    XUnmapWindow(dpy, tipw);
                    tip_on = 0;
                }
            } else if (ev.type == KeyPress) {
                key_cmd(XLookupKeysym(&ev.xkey, 0));
            }
        }
        if (user_quit)
            break;
    }

    send_cmd("quit\n");
    unlink(GEOMF);
    unlink(PIDF);
    unlink(BARPID);
    if (dpy)
        XUngrabKeyboard(dpy, CurrentTime);
    XCloseDisplay(dpy);
    return 0;
}
