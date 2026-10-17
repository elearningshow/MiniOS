/* MiniOS Music v4 — fullscreen chrome matching the YouTube player.
 * Opaque window (no save_under). Play/Pause / seek / volume / Quit.
 * Loop: off / this song / whole list. Auto plays the next song.
 * Sound: keep the slave fifo open. Reclaim leftover video mplayer.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAXS 64
#define MAXP 256
#define CMD "/tmp/minios-music.cmd"
#define OUT "/tmp/minios-music.out"
#define LOGF "/tmp/minios-music.log"
#define VCMD "/tmp/minios-mp.cmd"
#define BAR_H 68
#define ROW 22
#define IC_PLAY 1
#define IC_PAUSE 2
#define IC_PREV 3
#define IC_NEXT 4

static Display *dpy;
static Window win;
static GC gc;
static int scr, dw, dh;
static unsigned long fg, bg, btn, accent, dim, track, fillc, hi;
static char paths[MAXS][MAXP];
static char titles[MAXS][48];
static int order[MAXS];
static int nsongs, cur = -1, shuffle, paused, pct, vol = 80, autoplay, loop_mode;
static int tpos, tlen;
static pid_t mpid;
static int cmdfd = -1;
static int last_pct = -1, last_paused = -1, last_cur = -1, last_vol = -1;
static int last_shuffle = -1, last_mpid = -1, last_auto = -1, last_loop = -1;
static int last_tpos = -1, last_tlen = -1;
static unsigned long quiet_until;
static int need_vol;
static char status[96];
static char last_status[96];

static int bx_tog, bx_prev, bx_next, bx_auto, bx_loop, bx_add, bx_seek, bw_seek;
static int bx_vm, bx_vp, bx_shuf, bx_quit;
static char tip[64];
static int tip_on;
static const int by = 28, bh = 30;

static void paint(int force);

static unsigned long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)tv.tv_sec * 1000UL + (unsigned long)tv.tv_usec / 1000UL;
}

static void logm(const char *s)
{
    FILE *f = fopen(LOGF, "a");
    if (!f)
        return;
    fprintf(f, "%s\n", s);
    fclose(f);
}

static int is_audio(const char *n)
{
    const char *d = strrchr(n, '.');
    if (!d)
        return 0;
    return !strcasecmp(d, ".mp3") || !strcasecmp(d, ".ogg") ||
           !strcasecmp(d, ".wav") || !strcasecmp(d, ".m4a") ||
           !strcasecmp(d, ".flac");
}

static int same_title(const char *path)
{
    const char *base;
    char t[48], *dot;
    int i;

    base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(t, sizeof(t), "%s", base);
    dot = strrchr(t, '.');
    if (dot)
        *dot = 0;
    for (i = 0; i < nsongs; i++)
        if (strcmp(titles[i], t) == 0)
            return 1;
    return 0;
}

static void add_file(const char *path)
{
    const char *base, *dot;

    if (nsongs >= MAXS)
        return;
    if (same_title(path))
        return;
    snprintf(paths[nsongs], MAXP, "%s", path);
    base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(titles[nsongs], sizeof(titles[0]), "%s", base);
    dot = strrchr(titles[nsongs], '.');
    if (dot)
        titles[nsongs][dot - titles[nsongs]] = 0;
    order[nsongs] = nsongs;
    nsongs++;
}

static void scan_dir(const char *dir)
{
    DIR *d;
    struct dirent *e;
    char path[MAXP];
    struct stat st;

    d = opendir(dir);
    if (!d)
        return;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%.200s", dir, e->d_name);
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        if (is_audio(e->d_name))
            add_file(path);
    }
    closedir(d);
}

static void scan_all(void)
{
    DIR *mnt;
    struct dirent *e;
    char path[MAXP];

    nsongs = 0;
    scan_dir("/opt/minios/music");
    scan_dir("/home/tc/music");
    mnt = opendir("/mnt");
    if (mnt) {
        while ((e = readdir(mnt))) {
            if (e->d_name[0] == '.')
                continue;
            snprintf(path, sizeof(path), "/mnt/%.180s/minios-music", e->d_name);
            scan_dir(path);
            snprintf(path, sizeof(path), "/mnt/%.200s/music", e->d_name);
            scan_dir(path);
        }
        closedir(mnt);
    }
}

static void shuffle_order(void)
{
    int i, j, t;

    for (i = 0; i < nsongs; i++)
        order[i] = i;
    if (!shuffle || nsongs < 2)
        return;
    for (i = nsongs - 1; i > 0; i--) {
        j = rand() % (i + 1);
        t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
}

static void send_cmd(const char *s)
{
    int n;

    if (cmdfd >= 0) {
        n = (int)write(cmdfd, s, strlen(s));
        (void)n;
        return;
    }
}

static void fifo_write(const char *path, const char *s)
{
    int fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
        return;
    if (write(fd, s, strlen(s)) < 0) {
        /* ignore */
    }
    close(fd);
}

static int kill_named(const char *want, int sig)
{
    DIR *d;
    struct dirent *e;
    FILE *f;
    char path[80], name[32];
    pid_t p;
    size_t L = strlen(want);
    int n = 0;

    d = opendir("/proc");
    if (!d)
        return 0;
    while ((e = readdir(d))) {
        p = (pid_t)atoi(e->d_name);
        if (p <= 1 || p == getpid() || p == mpid)
            continue;
        snprintf(path, sizeof(path), "/proc/%d/comm", (int)p);
        f = fopen(path, "r");
        if (!f)
            continue;
        if (!fgets(name, sizeof(name), f)) {
            fclose(f);
            continue;
        }
        fclose(f);
        if (strncmp(name, want, L) == 0 &&
            (name[L] == '\n' || name[L] == 0)) {
            if (sig == 0) {
                n++;
            } else if (kill(p, sig) == 0) {
                n++;
            }
        }
    }
    closedir(d);
    return n;
}

/* Video leftover used to print "Close the video player first" and stay silent. */
static void reclaim_audio(void)
{
    int i;

    fifo_write(VCMD, "quit\n");
    fifo_write(CMD, "quit\n");
    kill_named("minios-player-bar", SIGTERM);
    kill_named("mplayer", SIGTERM);
    for (i = 0; i < 25; i++) {
        if (!kill_named("mplayer", 0))
            break;
        usleep(40000);
    }
    if (kill_named("mplayer", 0))
        kill_named("mplayer", SIGKILL);
    usleep(80000);
}

static void stop_play(void)
{
    int i;

    if (mpid > 0) {
        send_cmd("quit\n");
        kill(mpid, SIGTERM);
        for (i = 0; i < 20; i++) {
            if (waitpid(mpid, NULL, WNOHANG) == mpid)
                break;
            usleep(30000);
        }
        if (kill(mpid, 0) == 0)
            kill(mpid, SIGKILL);
        waitpid(mpid, NULL, WNOHANG);
        mpid = 0;
    }
    if (cmdfd >= 0) {
        close(cmdfd);
        cmdfd = -1;
    }
    paused = 1;
    pct = 0;
    need_vol = 0;
}

static int find_mplayer(char *buf, size_t n)
{
    const char *env = getenv("MINIOS_MPLAYER");
    const char *cands[6];
    int i, k = 0;

    if (env && env[0])
        cands[k++] = env;
    cands[k++] = "/tmp/tcloop/mplayer-cli/usr/local/bin/mplayer";
    cands[k++] = "/usr/local/bin/mplayer";
    cands[k++] = "/usr/bin/mplayer";
    cands[k++] = NULL;
    for (i = 0; cands[i]; i++) {
        if (cands[i][0] == '/' && access(cands[i], X_OK) != 0)
            continue;
        snprintf(buf, n, "%s", cands[i]);
        return 1;
    }
    return 0;
}

static void start_play(int order_i)
{
    char bin[MAXP];
    const char *file;
    char msg[200];

    if (nsongs <= 0)
        return;
    if (order_i < 0)
        order_i = 0;
    if (order_i >= nsongs)
        order_i = 0;
    cur = order_i;
    file = paths[order[cur]];
    stop_play();
    reclaim_audio();
    if (!find_mplayer(bin, sizeof(bin))) {
        snprintf(status, sizeof(status), "mplayer is missing.");
        logm(status);
        return;
    }
    unlink(CMD);
    unlink(OUT);
    if (mkfifo(CMD, 0666) != 0 && errno != EEXIST) {
        snprintf(status, sizeof(status), "Could not create control pipe.");
        return;
    }
    /* Writer stays open so mplayer does not block on -input file= */
    cmdfd = open(CMD, O_RDWR | O_NONBLOCK);
    if (cmdfd < 0) {
        snprintf(status, sizeof(status), "Could not open control pipe.");
        logm(status);
        return;
    }
    snprintf(msg, sizeof(msg), "exec %s %s", bin, file);
    logm(msg);
    mpid = fork();
    if (mpid == 0) {
        int out = open(OUT, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out >= 0) {
            dup2(out, 1);
            dup2(out, 2);
            if (out > 2)
                close(out);
        }
        execl(bin, "mplayer", "-slave", "-quiet", "-novideo", "-vo", "null",
              "-ao", "alsa,oss,null", "-softvol", "-softvol-max", "100",
              "-input", "file=" CMD,
              "--", file, (char *)0);
        _exit(127);
    }
    if (mpid < 0) {
        mpid = 0;
        snprintf(status, sizeof(status), "Could not start mplayer.");
        return;
    }
    paused = 0;
    pct = 0;
    tpos = 0;
    tlen = 0;
    need_vol = 1;
    quiet_until = now_ms() + 600;
    snprintf(status, sizeof(status), "%s", titles[order[cur]]);
}

static void play_next(int dir)
{
    int n;

    if (nsongs <= 0)
        return;
    if (cur < 0)
        cur = 0;
    n = cur + dir;
    if (n < 0 || n >= nsongs) {
        if (loop_mode != 2)
            return;
        n = (n < 0) ? nsongs - 1 : 0;
    }
    start_play(n);
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
    /* Two rows: duration + seek on top, buttons below. Volume never overlaps seek. */
    bx_seek = 72;
    bw_seek = dw - 72 - 80;
    if (bw_seek < 80)
        bw_seek = 80;
    bx_tog = 10;
    bx_prev = 58;
    bx_next = 102;
    bx_auto = 148;
    bx_loop = 204;
    bx_add = 260;
    bx_quit = dw - 70;
    bx_shuf = dw - 158;
    bx_vp = dw - 202;
    bx_vm = dw - 242;
}

static void draw_btn_face(int x, int y, int w, int h, unsigned long fill)
{
    XSetForeground(dpy, gc, fill);
    XFillRectangle(dpy, win, gc, x, y, w, h);
    XSetForeground(dpy, gc, hi);
    XFillRectangle(dpy, win, gc, x + 1, y + 1, w - 2, 2);
    XSetForeground(dpy, gc, fg);
    XDrawRectangle(dpy, win, gc, x, y, w, h);
}

static void draw_btn(int x, int y, int w, int h, unsigned long fill, const char *lab)
{
    int tw = (int)strlen(lab) * 6;
    draw_btn_face(x, y, w, h, fill);
    XSetForeground(dpy, gc, fg);
    XDrawString(dpy, win, gc, x + (w - tw) / 2, y + 20, lab, (int)strlen(lab));
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
        XFillPolygon(dpy, win, gc, t, 3, Convex, CoordModeOrigin);
    } else if (kind == IC_PAUSE) {
        XFillRectangle(dpy, win, gc, cx - 7, cy - 8, 5, 16);
        XFillRectangle(dpy, win, gc, cx + 3, cy - 8, 5, 16);
    } else if (kind == IC_PREV) {
        t[0].x = (short)(cx + 2);
        t[0].y = (short)(cy - 7);
        t[1].x = (short)(cx + 2);
        t[1].y = (short)(cy + 7);
        t[2].x = (short)(cx - 8);
        t[2].y = (short)cy;
        XFillPolygon(dpy, win, gc, t, 3, Convex, CoordModeOrigin);
        XFillRectangle(dpy, win, gc, cx - 10, cy - 7, 3, 14);
    } else if (kind == IC_NEXT) {
        t[0].x = (short)(cx - 2);
        t[0].y = (short)(cy - 7);
        t[1].x = (short)(cx - 2);
        t[1].y = (short)(cy + 7);
        t[2].x = (short)(cx + 8);
        t[2].y = (short)cy;
        XFillPolygon(dpy, win, gc, t, 3, Convex, CoordModeOrigin);
        XFillRectangle(dpy, win, gc, cx + 8, cy - 7, 3, 14);
    }
}

static int list_top(void)
{
    return dh / 2 + 8;
}

static int list_vis(void)
{
    int h = dh - BAR_H - list_top() - 16;
    int v = h / ROW;
    if (v < 1)
        v = 1;
    if (v > 10)
        v = 10;
    return v;
}

static void paint(int force)
{
    int i, y, vis, si, fill, playing;
    char lab[16];
    const char *now;

    playing = (mpid > 0 && !paused);
    now = status[0] ? status : "Select a song";

    if (!force &&
        pct == last_pct && paused == last_paused && cur == last_cur &&
        vol == last_vol && shuffle == last_shuffle &&
        autoplay == last_auto && loop_mode == last_loop &&
        tpos == last_tpos && tlen == last_tlen &&
        (mpid > 0) == (last_mpid > 0) &&
        strcmp(status, last_status) == 0)
        return;
    last_pct = pct;
    last_paused = paused;
    last_cur = cur;
    last_vol = vol;
    last_shuffle = shuffle;
    last_auto = autoplay;
    last_loop = loop_mode;
    last_tpos = tpos;
    last_tlen = tlen;
    last_mpid = mpid;
    snprintf(last_status, sizeof(last_status), "%s", status);

    XSetForeground(dpy, gc, bg);
    XFillRectangle(dpy, win, gc, 0, 0, dw, dh);

    XSetForeground(dpy, gc, dim);
    XDrawString(dpy, win, gc, 20, 28, "MiniOS Music", 12);

    {
        int tw = (int)strlen(now) * 6;
        int tx = (dw - tw) / 2;
        if (tx < 20)
            tx = 20;
        XSetForeground(dpy, gc, fg);
        XDrawString(dpy, win, gc, tx, dh / 2 - 36, now, (int)strlen(now));
    }

    vis = list_vis();
    si = 0;
    if (cur >= vis)
        si = cur - vis + 1;
    for (i = 0; i < vis && si + i < nsongs; i++) {
        y = list_top() + i * ROW;
        if (si + i == cur) {
            XSetForeground(dpy, gc, accent);
            XFillRectangle(dpy, win, gc, dw / 5, y - 16, dw * 3 / 5, ROW);
            XSetForeground(dpy, gc, bg);
        } else {
            XSetForeground(dpy, gc, fg);
        }
        {
            int tw = (int)strlen(titles[order[si + i]]) * 6;
            int tx = (dw - tw) / 2;
            if (tx < 24)
                tx = 24;
            XDrawString(dpy, win, gc, tx, y,
                        titles[order[si + i]],
                        (int)strlen(titles[order[si + i]]));
        }
    }
    if (nsongs == 0) {
        XSetForeground(dpy, gc, fg);
        XDrawString(dpy, win, gc, 40, list_top(),
                    "No songs found. Put MP3 files in minios-music on the USB.", 58);
    }

    XSetForeground(dpy, gc, bg);
    XFillRectangle(dpy, win, gc, 0, dh - BAR_H, dw, BAR_H);
    XSetForeground(dpy, gc, fillc);
    XFillRectangle(dpy, win, gc, 0, dh - BAR_H, dw, 3);
    {
        char t1[16], t2[16];
        int by0 = dh - BAR_H;
        fmt_time(t1, sizeof(t1), tpos);
        fmt_time(t2, sizeof(t2), tlen);
        XSetForeground(dpy, gc, fg);
        XDrawString(dpy, win, gc, 8, by0 + 18, t1, (int)strlen(t1));
        XDrawString(dpy, win, gc, dw - 72, by0 + 18, t2, (int)strlen(t2));
        XSetForeground(dpy, gc, track);
        XFillRectangle(dpy, win, gc, bx_seek, by0 + 8, bw_seek, 12);
        fill = (mpid > 0) ? (bw_seek * pct / 100) : 0;
        if (tlen > 0)
            fill = bw_seek * tpos / tlen;
        if (fill > bw_seek)
            fill = bw_seek;
        if (fill > 0) {
            XSetForeground(dpy, gc, fillc);
            XFillRectangle(dpy, win, gc, bx_seek, by0 + 8, fill, 12);
        }
        XSetForeground(dpy, gc, fg);
        XDrawRectangle(dpy, win, gc, bx_seek, by0 + 8, bw_seek, 12);
    }
    draw_icon_btn(bx_tog, dh - BAR_H + by, 44, bh, playing ? accent : btn,
                  playing ? IC_PAUSE : IC_PLAY);
    draw_icon_btn(bx_prev, dh - BAR_H + by, 40, bh, btn, IC_PREV);
    draw_icon_btn(bx_next, dh - BAR_H + by, 40, bh, btn, IC_NEXT);
    draw_btn(bx_auto, dh - BAR_H + by, 48, bh, autoplay ? accent : btn, "Auto");
    {
        const char *ll = "Loop";
        if (loop_mode == 1)
            ll = "One";
        else if (loop_mode == 2)
            ll = "All";
        draw_btn(bx_loop, dh - BAR_H + by, 48, bh, loop_mode ? accent : btn, ll);
    }
    draw_btn(bx_add, dh - BAR_H + by, 48, bh, btn, "Add");
    snprintf(lab, sizeof(lab), "%d", vol);
    XDrawString(dpy, win, gc, bx_vm - 28, dh - BAR_H + by + 20, lab, (int)strlen(lab));
    draw_btn(bx_vm, dh - BAR_H + by, 36, bh, btn, "-");
    draw_btn(bx_vp, dh - BAR_H + by, 36, bh, btn, "+");
    draw_btn(bx_shuf, dh - BAR_H + by, 72, bh, shuffle ? accent : btn, "Shuffle");
    draw_btn(bx_quit, dh - BAR_H + by, 56, bh, dim, "Quit");
    XFlush(dpy);
}

static void read_pos(void)
{
    FILE *f;
    char line[160];

    if (mpid <= 0 || paused)
        return;
    send_cmd("pausing_keep_force get_time_pos\n");
    send_cmd("pausing_keep_force get_time_length\n");
    send_cmd("pausing_keep_force get_percent_pos\n");
    f = fopen(OUT, "r");
    if (!f)
        return;
    if (fseek(f, -1200, SEEK_END) != 0)
        rewind(f);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "ANS_PERCENT_POSITION=", 21) == 0) {
            pct = atoi(line + 21);
            if (pct < 0)
                pct = 0;
            if (pct > 100)
                pct = 100;
        } else if (strncmp(line, "ANS_TIME_POSITION=", 18) == 0) {
            tpos = atoi(line + 18);
            if (tpos < 0)
                tpos = 0;
        } else if (strncmp(line, "ANS_LENGTH=", 11) == 0) {
            tlen = atoi(line + 11);
            if (tlen < 0)
                tlen = 0;
        }
    }
    fclose(f);
}

static void apply_vol(void)
{
    char buf[48];

    if (mpid <= 0 || cmdfd < 0)
        return;
    snprintf(buf, sizeof(buf), "pausing_keep_force volume %d 1\n", vol);
    send_cmd(buf);
}

static int click_list(int x, int y)
{
    int vis, si, row, top;

    (void)x;
    if (nsongs <= 0)
        return 0;
    top = list_top() - 16;
    if (y < top || y >= dh - BAR_H - 8)
        return 0;
    vis = list_vis();
    si = (cur >= vis) ? cur - vis + 1 : 0;
    row = (y - top) / ROW;
    if (row < 0 || row >= vis || si + row >= nsongs)
        return 0;
    start_play(si + row);
    return 1;
}

static void click(int x, int y)
{
    char buf[48];
    int p, bar0 = dh - BAR_H;

    if (click_list(x, y))
        return;
    if (y < bar0 || y > dh - 2)
        return;
    if (y <= bar0 + 24 && x >= bx_seek && x < bx_seek + bw_seek && mpid > 0) {
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
        return;
    }
    if (y < bar0 + by - 2)
        return;
    if (x >= bx_tog && x < bx_tog + 44) {
        if (mpid <= 0)
            start_play(cur < 0 ? 0 : cur);
        else {
            send_cmd("pause\n");
            paused = !paused;
            quiet_until = now_ms() + 1500;
        }
    } else if (x >= bx_prev && x < bx_prev + 40)
        play_next(-1);
    else if (x >= bx_next && x < bx_next + 40)
        play_next(1);
    else if (x >= bx_auto && x < bx_auto + 48)
        autoplay = !autoplay;
    else if (x >= bx_loop && x < bx_loop + 48)
        loop_mode = (loop_mode + 1) % 3;
    else if (x >= bx_add && x < bx_add + 48) {
        scan_all();
        shuffle_order();
        snprintf(status, sizeof(status), "Found %d songs", nsongs);
    }
    else if (x >= bx_vm && x < bx_vm + 36) {
        vol -= 10;
        if (vol < 0)
            vol = 0;
        apply_vol();
    } else if (x >= bx_vp && x < bx_vp + 36) {
        vol += 10;
        if (vol > 100)
            vol = 100;
        apply_vol();
    } else if (x >= bx_shuf && x < bx_shuf + 72) {
        shuffle = !shuffle;
        shuffle_order();
        if (cur >= 0) {
            int song = order[cur], i;
            for (i = 0; i < nsongs; i++)
                if (order[i] == song) {
                    cur = i;
                    break;
                }
        }
    } else if (x >= bx_quit && x < bx_quit + 56) {
        stop_play();
        reclaim_audio();
        exit(0);
    }
}

static int mplayer_alive(void)
{
    int st;

    if (mpid <= 0)
        return 0;
    if (waitpid(mpid, &st, WNOHANG) == mpid) {
        mpid = 0;
        return 0;
    }
    if (kill(mpid, 0) == 0)
        return 1;
    waitpid(mpid, NULL, WNOHANG);
    mpid = 0;
    return 0;
}

static void note_dead(void)
{
    FILE *f;
    char line[160], last[160];

    last[0] = 0;
    f = fopen(OUT, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            size_t L = strlen(line);
            while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r'))
                line[--L] = 0;
            if (L)
                snprintf(last, sizeof(last), "%s", line);
        }
        fclose(f);
    }
    if (last[0]) {
        snprintf(status, sizeof(status), "%.90s", last);
        logm(last);
    } else {
        snprintf(status, sizeof(status), "Playback stopped.");
    }
}

int main(void)
{
    XSetWindowAttributes swa;
    XColor col, dummy;
    Colormap cmap;
    XEvent ev;
    XFontStruct *fs;
    int xfd;
    unsigned long last = 0;
    struct timeval tv, nowtv;

    signal(SIGPIPE, SIG_IGN);
    srand((unsigned)getpid() ^ (unsigned)time(NULL));
    scan_all();
    shuffle_order();
    snprintf(status, sizeof(status),
             nsongs ? "Select a song" : "No songs found");
    logm("===== minios-mp3 start =====");

    dpy = XOpenDisplay(NULL);
    if (!dpy)
        return 1;
    scr = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, scr);
    bg = BlackPixel(dpy, scr);
    fg = WhitePixel(dpy, scr);
    btn = accent = dim = track = fillc = hi = WhitePixel(dpy, scr);
    if (XAllocNamedColor(dpy, cmap, "#303030", &col, &dummy))
        btn = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#c8a000", &col, &dummy))
        fillc = accent = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#202020", &col, &dummy))
        track = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#505050", &col, &dummy))
        dim = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#4a4a4a", &col, &dummy))
        hi = col.pixel;

    dw = DisplayWidth(dpy, scr);
    dh = DisplayHeight(dpy, scr);
    layout();

    swa.override_redirect = True;
    swa.background_pixel = bg;
    swa.border_pixel = bg;
    swa.backing_store = WhenMapped;
    swa.save_under = False;
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | PointerMotionMask;
    win = XCreateWindow(dpy, RootWindow(dpy, scr), 0, 0, dw, dh, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWOverrideRedirect | CWBackPixel | CWBorderPixel |
                        CWBackingStore | CWEventMask, &swa);
    XStoreName(dpy, win, "MiniOS Music v4");
    gc = XCreateGC(dpy, win, 0, NULL);
    fs = XLoadQueryFont(dpy, "10x20");
    if (!fs)
        fs = XLoadQueryFont(dpy, "9x15");
    if (!fs)
        fs = XLoadQueryFont(dpy, "fixed");
    if (fs)
        XSetFont(dpy, gc, fs->fid);
    XMapRaised(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    paint(1);
    xfd = ConnectionNumber(dpy);

    while (1) {
        fd_set rfds;
        gettimeofday(&nowtv, NULL);
        if ((unsigned long)nowtv.tv_sec * 1000UL + nowtv.tv_usec / 1000UL - last >= 800) {
            if (mpid > 0 && !mplayer_alive()) {
                if (loop_mode == 1 && cur >= 0)
                    start_play(cur);
                else if ((autoplay || loop_mode == 2) && nsongs > 0) {
                    play_next(1);
                    if (mpid <= 0) {
                        paused = 1;
                        snprintf(status, sizeof(status), "Select a song");
                    }
                } else {
                    paused = 1;
                    snprintf(status, sizeof(status), "Select a song");
                }
            }
            if (mpid > 0 && now_ms() >= quiet_until) {
                if (need_vol) {
                    apply_vol();
                    need_vol = 0;
                }
                if (!paused)
                    read_pos();
            }
            paint(0);
            last = (unsigned long)nowtv.tv_sec * 1000UL + nowtv.tv_usec / 1000UL;
        }
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        FD_ZERO(&rfds);
        FD_SET(xfd, &rfds);
        select(xfd + 1, &rfds, NULL, NULL, &tv);
        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == Expose)
                paint(1);
            else if (ev.type == ButtonPress) {
                tip_on = 0;
                click(ev.xbutton.x, ev.xbutton.y);
                paint(1);
            } else if (ev.type == MotionNotify) {
                int mx = ev.xmotion.x, my = ev.xmotion.y, bar0 = dh - BAR_H;
                const char *s = NULL;
                if (my >= bar0 + by - 2) {
                    if (mx >= bx_tog && mx < bx_tog + 44)
                        s = "Play or pause";
                    else if (mx >= bx_prev && mx < bx_prev + 40)
                        s = "Previous song";
                    else if (mx >= bx_next && mx < bx_next + 40)
                        s = "Next song";
                    else if (mx >= bx_auto && mx < bx_auto + 48)
                        s = "Play the next song when this one ends";
                    else if (mx >= bx_loop && mx < bx_loop + 48)
                        s = "Off, then this song (One), then whole list (All)";
                    else if (mx >= bx_add && mx < bx_add + 48)
                        s = "Look again for songs on the USB";
                    else if (mx >= bx_vm && mx < bx_vm + 36)
                        s = "Volume down. Keyboard - also works.";
                    else if (mx >= bx_vp && mx < bx_vp + 36)
                        s = "Volume up. Keyboard + also works.";
                    else if (mx >= bx_shuf && mx < bx_shuf + 72)
                        s = "Shuffle the list";
                    else if (mx >= bx_quit && mx < bx_quit + 56)
                        s = "Close the player";
                }
                if (s) {
                    int tw = (int)strlen(s) * 10 + 16, tx = mx;
                    if (tx + tw > dw)
                        tx = dw - tw;
                    if (tx < 0)
                        tx = 0;
                    XSetForeground(dpy, gc, bg);
                    XFillRectangle(dpy, win, gc, tx, bar0 - 28, tw, 26);
                    XSetForeground(dpy, gc, fg);
                    XDrawRectangle(dpy, win, gc, tx, bar0 - 28, tw - 1, 25);
                    XDrawString(dpy, win, gc, tx + 8, bar0 - 10, s, (int)strlen(s));
                    tip_on = 1;
                    XFlush(dpy);
                } else if (tip_on) {
                    paint(1);
                    tip_on = 0;
                }
            } else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_space)
                    click(bx_tog + 4, dh - 20);
                else if (ks == XK_Right || ks == XK_n || ks == XK_N)
                    play_next(1);
                else if (ks == XK_Left || ks == XK_p || ks == XK_P)
                    play_next(-1);
                else if (ks == XK_s || ks == XK_S)
                    click(bx_shuf + 4, dh - 20);
                else if (ks == XK_a || ks == XK_A)
                    autoplay = !autoplay;
                else if (ks == XK_l || ks == XK_L)
                    loop_mode = (loop_mode + 1) % 3;
                else if (ks == XK_Down || ks == XK_9 || ks == XK_minus ||
                         ks == XK_KP_Subtract || ks == XK_underscore) {
                    vol -= 10;
                    if (vol < 0)
                        vol = 0;
                    apply_vol();
                } else if (ks == XK_Up || ks == XK_0 || ks == XK_plus ||
                           ks == XK_KP_Add || ks == XK_equal) {
                    vol += 10;
                    if (vol > 100)
                        vol = 100;
                    apply_vol();
                } else if (ks == XK_q || ks == XK_Q || ks == XK_Escape) {
                    stop_play();
                    reclaim_audio();
                    return 0;
                }
                paint(1);
            }
        }
    }
}
