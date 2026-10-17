/* MiniOS TikTok search. Layout follows FLTube (YouTube): tabs, Term/URL,
 * Go search!, result rows with a play tile, views, duration, uploader. */
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
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#define MAXR 120
#define MAXL 220
#define PAGE 5

static Display *dpy;
static Window win;
static GC gc;
static int dw, dh, scr;
static unsigned long fg, bg, btn, accent, dim, field, panel, tile, playc;
static char query[96];
static char tt_listed_query[96];
static int qlen;
static char titles[MAXR][96];
static char urls[MAXR][MAXL];
static char durs[MAXR][16];
static char views[MAXR][24];
static char ups[MAXR][48];
static int nres, hover = -1, page, searching;
static char status[140];
static pid_t search_pid;
static pid_t thumbs_pid;
static int thumbs_ticks;
static int play_wait;
static int tt_bar_seen;
static int tt_cool_shown = -1;
static int tt_thumbs_shown = -1;
static time_t cool_until;
static int tt_ask_more;
static int tt_covered;
static unsigned long tt_row_paint_ms;
static unsigned long grey;
static Atom wm_delete;
static XFontStruct *fs;

static int x_error(Display *d, XErrorEvent *e)
{
    FILE *f = fopen("/tmp/minios-tiktok.log", "a");

    /* The default handler exits. A focus or font error used to map the
     * window and then close it immediately. */
    if (f) {
        fprintf(f, "xerr code=%u req=%u minor=%u\n",
                e->error_code, e->request_code, e->minor_code);
        fclose(f);
    }
    (void)d;
    return 0;
}

static void text_at(unsigned long c, int x, int y, const char *s);
static void fill_rect(unsigned long c, int x, int y, int w, int h);
static void box(unsigned long c, int x, int y, int w, int h);
static unsigned long rgb_to_pixel(unsigned r, unsigned g, unsigned b);

static int cool_left(void)
{
    time_t now = time(NULL);
    if (cool_until <= now)
        return 0;
    return (int)(cool_until - now);
}

static int can_search(void)
{
    return !searching && cool_left() == 0;
}

static int can_next(void)
{
    return nres > 0 && (page + 1) * PAGE < nres;
}

static int can_prev(void)
{
    return page > 0;
}

static int term_x, term_y, term_w, term_h;
static int go_x, go_y, go_w, go_h;
static int tt_stop_x, tt_stop_y, tt_stop_w, tt_stop_h;
static int tt_time_x, tt_time_y, tt_time_w, tt_time_h;
static int tt_qual_x, tt_qual_y, tt_qual_w, tt_qual_h;
static int tt_high_quality;
static int tt_search_secs = 30;
static int tt_user_stopped;
static int prev_x, next_x, nav_y, nav_w, nav_h;
static int list_x, list_y, list_w, list_h, row_h;
static int close_x, close_y, close_w, close_h;

static int text_w(const char *s)
{
    if (!s || !s[0])
        return 0;
    if (fs)
        return XTextWidth(fs, s, (int)strlen(s));
    return (int)strlen(s) * 9;
}

static int is_canonical_video(const char *url)
{
    const char *p, *at, *slash;
    static const char *bad[] = {
        "photo", "tag", "music", "live", "explore", "login", "discover", "search", NULL
    };
    int i;

    if (!url || strncmp(url, "https://", 8) != 0)
        return 0;
    p = url + 8;
    if (strncmp(p, "www.", 4) == 0)
        p += 4;
    if (strncmp(p, "tiktok.com/@", 12) != 0)
        return 0;
    at = p + 12;
    slash = strstr(at, "/video/");
    if (!slash || slash == at)
        return 0;
    for (p = at; p < slash; p++) {
        unsigned char c = (unsigned char)*p;
        if (!isalnum(c) && c != '.' && c != '_')
            return 0;
    }
    p = slash + 7;
    if (!isdigit((unsigned char)*p))
        return 0;
    while (isdigit((unsigned char)*p))
        p++;
    if (*p == '/')
        p++;
    if (*p != 0)
        return 0;
    p = url;
    while (*p) {
        if (*p == '/') {
            const char *seg = p + 1;
            for (i = 0; bad[i]; i++) {
                size_t n = strlen(bad[i]);
                if (strncmp(seg, bad[i], n) == 0 &&
                    (seg[n] == '/' || seg[n] == 0))
                    return 0;
            }
        }
        p++;
    }
    return 1;
}

static void video_label(const char *url, char *dst, size_t n)
{
    const char *p = strstr(url ? url : "", "tiktok.com/");

    if (!p)
        p = url ? url : "";
    snprintf(dst, n, "%s", p);
}

static void text_ellip(unsigned long c, int x, int y, int maxw, const char *s)
{
    char line[MAXL];
    int n, full;

    if (!s || !s[0] || maxw < 20)
        return;
    full = (int)strlen(s);
    n = full;
    if (n >= (int)sizeof(line))
        n = (int)sizeof(line) - 1;
    memcpy(line, s, n);
    line[n] = 0;
    while (n > 1 && text_w(line) > maxw)
        line[--n] = 0;
    if (n < full && n > 3) {
        line[n - 1] = '.';
        line[n - 2] = '.';
        line[n - 3] = '.';
    }
    text_at(c, x, y, line);
}

static unsigned long rgb_to_pixel(unsigned r, unsigned g, unsigned b)
{
    Visual *v = DefaultVisual(dpy, scr);
    unsigned long m, rp, gp, bp;
    int rs = 0, gs = 0, bs = 0;

    m = v->red_mask;
    while (m && (m & 1) == 0) {
        m >>= 1;
        rs++;
    }
    rp = (m ? ((unsigned long)r * m) / 255 : 0) << rs;
    m = v->green_mask;
    while (m && (m & 1) == 0) {
        m >>= 1;
        gs++;
    }
    gp = (m ? ((unsigned long)g * m) / 255 : 0) << gs;
    m = v->blue_mask;
    while (m && (m & 1) == 0) {
        m >>= 1;
        bs++;
    }
    bp = (m ? ((unsigned long)b * m) / 255 : 0) << bs;
    return rp | gp | bp;
}

static void draw_ppm(int x, int y, const char *path)
{
    FILE *f;
    char hdr[32];
    int w, h, maxv, i, j;
    XImage *img;
    unsigned char *pix;

    f = fopen(path, "rb");
    if (!f)
        return;
    if (!fgets(hdr, sizeof(hdr), f) || strncmp(hdr, "P6", 2) != 0) {
        fclose(f);
        return;
    }
    do {
        if (!fgets(hdr, sizeof(hdr), f)) {
            fclose(f);
            return;
        }
    } while (hdr[0] == '#');
    if (sscanf(hdr, "%d %d", &w, &h) != 2 || w < 8 || h < 8 || w > 120 || h > 80) {
        fclose(f);
        return;
    }
    if (!fgets(hdr, sizeof(hdr), f)) {
        fclose(f);
        return;
    }
    maxv = atoi(hdr);
    if (maxv != 255) {
        fclose(f);
        return;
    }
    pix = malloc((size_t)w * (size_t)h * 3);
    if (!pix) {
        fclose(f);
        return;
    }
    if (fread(pix, 3, (size_t)w * (size_t)h, f) != (size_t)w * (size_t)h) {
        free(pix);
        fclose(f);
        return;
    }
    fclose(f);
    img = XCreateImage(dpy, DefaultVisual(dpy, scr), DefaultDepth(dpy, scr),
                       ZPixmap, 0, NULL, w, h, 32, 0);
    if (!img) {
        free(pix);
        return;
    }
    img->data = calloc(1, img->bytes_per_line * h);
    if (!img->data) {
        XDestroyImage(img);
        free(pix);
        return;
    }
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            unsigned char *p = pix + (j * w + i) * 3;
            XPutPixel(img, i, j, rgb_to_pixel(p[0], p[1], p[2]));
        }
    }
    XPutImage(dpy, win, gc, img, 0, 0, x, y, w, h);
    XDestroyImage(img);
    free(pix);
}

/* Stop sits immediately left of Go search / Searching. Its own box. */
static void tt_place_stop_button(void)
{
    tt_stop_w = 72;
    tt_stop_h = go_h;
    tt_stop_y = go_y;
    tt_stop_x = go_x - 8 - tt_stop_w;
    term_w = tt_stop_x - term_x - 10;
    if (term_w < 120)
        term_w = 120;
}

/* Seconds beside Stop. Each click steps 15, 30, 45, 60. */
static void tt_place_time_button(void)
{
    tt_time_w = 58;
    tt_time_h = go_h;
    tt_time_y = go_y;
    tt_time_x = tt_stop_x - 8 - tt_time_w;
    term_w = tt_time_x - term_x - 10;
    if (term_w < 80)
        term_w = 80;
}

static int tt_time_clicked(int x, int y)
{
    return x >= tt_time_x && x <= tt_time_x + tt_time_w &&
           y >= tt_time_y && y <= tt_time_y + tt_time_h;
}

static void tt_cycle_search_time(void)
{
    if (tt_search_secs < 30)
        tt_search_secs = 30;
    else if (tt_search_secs < 45)
        tt_search_secs = 45;
    else if (tt_search_secs < 60)
        tt_search_secs = 60;
    else
        tt_search_secs = 15;
    snprintf(status, sizeof(status),
             "Search runs for %d seconds, then shows the list.",
             tt_search_secs);
}

/* Fit is the picture this laptop can keep with the sound.
   High keeps the full file for a faster computer. */
static void tt_save_quality(void)
{
    FILE *f;

    f = fopen("/tmp/minios-tt-quality", "w");
    if (!f)
        return;
    fputs(tt_high_quality ? "high\n" : "fit\n", f);
    fclose(f);
}

static void tt_load_quality(void)
{
    FILE *f;
    char line[16];

    tt_high_quality = 0;
    f = fopen("/tmp/minios-tt-quality", "r");
    if (!f)
        return;
    line[0] = 0;
    if (fgets(line, sizeof line, f) && strncmp(line, "high", 4) == 0)
        tt_high_quality = 1;
    fclose(f);
}

static void tt_place_quality_button(void)
{
    tt_qual_w = 58;
    tt_qual_h = go_h;
    tt_qual_y = go_y;
    tt_qual_x = tt_time_x - 8 - tt_qual_w;
    term_w = tt_qual_x - term_x - 10;
    if (term_w < 80)
        term_w = 80;
}

static int tt_quality_clicked(int x, int y)
{
    return x >= tt_qual_x && x <= tt_qual_x + tt_qual_w &&
           y >= tt_qual_y && y <= tt_qual_y + tt_qual_h;
}

static void tt_toggle_quality(void)
{
    FILE *lf;

    tt_high_quality = !tt_high_quality;
    tt_save_quality();
    if (tt_high_quality)
        snprintf(status, sizeof(status),
                 "High. Full file. A slow laptop can fall behind the sound.");
    else
        snprintf(status, sizeof(status),
                 "Fit. Pictures sized for this laptop.");
    lf = fopen("/tmp/minios-tiktok.log", "a");
    if (lf) {
        fputs("search-v235 quality\n", lf);
        fclose(lf);
    }
}

static void tt_paint_quality_button(void)
{
    const char *lab = tt_high_quality ? "High" : "Fit";
    int tx;

    fill_rect(btn, tt_qual_x, tt_qual_y, tt_qual_w, tt_qual_h);
    box(fg, tt_qual_x, tt_qual_y, tt_qual_w, tt_qual_h);
    tx = tt_qual_x + (tt_qual_w - text_w(lab)) / 2;
    if (tx < tt_qual_x + 2)
        tx = tt_qual_x + 2;
    text_at(fg, tx, tt_qual_y + 16, lab);
}

static int tt_stop_ready(void)
{
    return searching || search_pid > 0 || cool_left() > 0;
}

static int tt_stop_clicked(int x, int y)
{
    return x >= tt_stop_x && x <= tt_stop_x + tt_stop_w &&
           y >= tt_stop_y && y <= tt_stop_y + tt_stop_h;
}

static void layout(void)
{
    close_w = 72;
    close_h = 22;
    close_x = dw - 16 - close_w;
    close_y = 6;
    term_x = 100;
    term_y = 44;
    term_h = 24;
    go_w = 130;
    go_h = 24;
    go_x = dw - 16 - go_w;
    go_y = term_y;
    term_w = go_x - term_x - 10;
    if (term_w < 180)
        term_w = 180;
    tt_place_stop_button();
    tt_place_time_button();
    tt_place_quality_button();
    list_x = 12;
    list_y = 84;
    list_w = dw - 24;
    list_h = dh - list_y - 78;
    row_h = 72;
    nav_w = 115;
    nav_h = 23;
    nav_y = dh - 32;
    prev_x = 16;
    next_x = dw - 16 - nav_w;
}

static void fill_rect(unsigned long c, int x, int y, int w, int h)
{
    XSetForeground(dpy, gc, c);
    XFillRectangle(dpy, win, gc, x, y, w, h);
}

static void box(unsigned long c, int x, int y, int w, int h)
{
    XSetForeground(dpy, gc, c);
    XDrawRectangle(dpy, win, gc, x, y, w, h);
}

static void text_at(unsigned long c, int x, int y, const char *s)
{
    if (!s || !s[0])
        return;
    XSetForeground(dpy, gc, c);
    XDrawString(dpy, win, gc, x, y, s, (int)strlen(s));
}

/* The digits after /video/ are this row's own picture name. */
static int tt_clip_id(const char *url, char *dst, int n)
{
    const char *p;
    int i = 0;

    dst[0] = 0;
    if (!url || n < 2)
        return 0;
    p = strstr(url, "/video/");
    if (!p)
        return 0;
    p += 7;
    while (p[i] >= '0' && p[i] <= '9' && i < n - 1) {
        dst[i] = p[i];
        i++;
    }
    dst[i] = 0;
    return i > 0;
}

static int tt_thumb_file(int idx, char *dst, int n)
{
    char id[32];

    if (idx < 0 || idx >= nres || n < 16)
        return 0;
    if (!tt_clip_id(urls[idx], id, (int)sizeof id))
        return 0;
    snprintf(dst, (size_t)n, "/tmp/minios-tt-thumbs/%s.ppm", id);
    return 1;
}

static void draw_thumb(int x, int y, int on, int idx)
{
    char pth[80];
    int px[3], py[3];
    XPoint pts[3];

    pth[0] = 0;
    if (tt_thumb_file(idx, pth, (int)sizeof pth) && access(pth, R_OK) == 0) {
        fill_rect(tile, x, y, 88, 56);
        draw_ppm(x, y, pth);
        box(dim, x, y, 88, 56);
        return;
    }
    fill_rect(on ? accent : tile, x, y, 88, 56);
    box(dim, x, y, 88, 56);
    px[0] = x + 34;
    py[0] = y + 14;
    px[1] = x + 34;
    py[1] = y + 42;
    px[2] = x + 62;
    py[2] = y + 28;
    pts[0].x = (short)px[0];
    pts[0].y = (short)py[0];
    pts[1].x = (short)px[1];
    pts[1].y = (short)py[1];
    pts[2].x = (short)px[2];
    pts[2].y = (short)py[2];
    XSetForeground(dpy, gc, playc);
    XFillPolygon(dpy, win, gc, pts, 3, Convex, CoordModeOrigin);
}

static const char *search_wait_text(void)
{
    if (status[0])
        return status;
    return "Searching Startpage. About 30 seconds.";
}

static void tt_paint_stop_button(void)
{
    unsigned long face, ink;
    const char *lab = "Stop";
    int tx;

    if (tt_stop_ready()) {
        face = btn;
        ink = fg;
    } else {
        face = grey;
        ink = dim;
    }
    fill_rect(face, tt_stop_x, tt_stop_y, tt_stop_w, tt_stop_h);
    box(ink, tt_stop_x, tt_stop_y, tt_stop_w, tt_stop_h);
    tx = tt_stop_x + (tt_stop_w - text_w(lab)) / 2;
    if (tx < tt_stop_x + 2)
        tx = tt_stop_x + 2;
    text_at(ink, tx, tt_stop_y + 16, lab);
}

static void tt_paint_time_button(void)
{
    char lab[12];
    int tx;

    snprintf(lab, sizeof(lab), "%ds", tt_search_secs);
    fill_rect(btn, tt_time_x, tt_time_y, tt_time_w, tt_time_h);
    box(fg, tt_time_x, tt_time_y, tt_time_w, tt_time_h);
    tx = tt_time_x + (tt_time_w - text_w(lab)) / 2;
    if (tx < tt_time_x + 2)
        tx = tt_time_x + 2;
    text_at(fg, tx, tt_time_y + 16, lab);
}

static void paint(void)
{
    int i, y, shown, start;
    char lab[160];

    fill_rect(bg, 0, 0, dw, dh);
    fill_rect(panel, 0, 0, dw, 76);
    box(dim, 0, 0, dw - 1, 76);
    text_at(fg, 16, 22, "MiniOS TikTok");
    fill_rect(btn, close_x, close_y, close_w, close_h);
    box(fg, close_x, close_y, close_w, close_h);
    text_at(fg, close_x + 16, close_y + 15, "Close");
    text_at(fg, 16, term_y + 16, "Term/URL");
    fill_rect(field, term_x, term_y, term_w, term_h);
    box(dim, term_x, term_y, term_w, term_h);
    snprintf(lab, sizeof(lab), "%s", query);
    text_at(fg, term_x + 6, term_y + 16, lab);
    text_at(dim, term_x + 6 + text_w(lab), term_y + 16, "_");
    {
        int left = cool_left();
        char golab[32];
        unsigned long gbg = btn, gfg = fg;
        if (searching) {
            snprintf(golab, sizeof(golab), "Searching...");
            gbg = grey;
            gfg = dim;
        } else if (left > 0) {
            snprintf(golab, sizeof(golab), "Wait %ds", left);
            gbg = grey;
            gfg = dim;
        } else {
            snprintf(golab, sizeof(golab), "Go search!");
        }
        fill_rect(gbg, go_x, go_y, go_w, go_h);
        box(gfg, go_x, go_y, go_w, go_h);
        text_at(gfg, go_x + 18, go_y + 16, golab);
        tt_paint_stop_button();
        tt_paint_time_button();
        tt_paint_quality_button();
    }

    fill_rect(panel, list_x, list_y, list_w, list_h);
    box(dim, list_x, list_y, list_w, list_h);

    start = page * PAGE;
    shown = nres - start;
    if (shown > PAGE)
        shown = PAGE;
    if (nres == 0 && searching) {
        text_ellip(dim, list_x + 24, list_y + 36, list_w - 48, search_wait_text());
        text_ellip(dim, list_x + 24, list_y + 58, list_w - 48,
                   "Videos will appear here as they are found.");
    }
    for (i = 0; i < shown; i++) {
        int idx = start + i;
        y = list_y + 8 + i * row_h;
        if (y + row_h > list_y + list_h)
            break;
        fill_rect((idx == hover) ? accent : bg, list_x + 6, y, list_w - 12, row_h - 6);
        box(dim, list_x + 6, y, list_w - 12, row_h - 6);
        draw_thumb(list_x + 14, y + 6, idx == hover, idx);
        text_ellip(fg, list_x + 114, y + 22, list_w - 130, titles[idx]);
        {
            char shown[MAXL];
            video_label(urls[idx], shown, sizeof(shown));
            text_ellip(dim, list_x + 114, y + 44, list_w - 130, shown);
        }
    }

    /* Status strip sits above Previous/Next. Never draw into the buttons. */
    fill_rect(panel, 0, nav_y - 26, dw, 24);
    {
        int pages = nres > 0 ? (nres + PAGE - 1) / PAGE : 0;
        if (play_wait && status[0])
            snprintf(lab, sizeof(lab), "%s", status);
        else if (searching && nres > 0)
            snprintf(lab, sizeof(lab), "%.*s  Click one to play.",
                     (int)sizeof(lab) - 21, search_wait_text());
        else if (searching)
            snprintf(lab, sizeof(lab), "%s", search_wait_text());
        else if (nres > 0)
            snprintf(lab, sizeof(lab), "Page %d of %d  (%d videos)",
                     page + 1, pages, nres);
        else if (status[0])
            snprintf(lab, sizeof(lab), "%s", status);
        else
            snprintf(lab, sizeof(lab), "Type a word, then Go search!");
        text_ellip(dim, list_x + 8, nav_y - 10, dw - list_x - 16, lab);
    }

    fill_rect(can_prev() ? btn : grey, prev_x, nav_y, nav_w, nav_h);
    box(can_prev() ? fg : dim, prev_x, nav_y, nav_w, nav_h);
    text_at(can_prev() ? fg : dim, prev_x + 16, nav_y + 16, "< Previous");
    fill_rect(can_next() ? btn : grey, next_x, nav_y, nav_w, nav_h);
    box(can_next() ? fg : dim, next_x, nav_y, nav_w, nav_h);
    text_at(can_next() ? fg : dim, next_x + 32, nav_y + 16, "Next >");
    XFlush(dpy);
}

static void stop_thumbs(void)
{
    if (thumbs_pid > 0) {
        kill(thumbs_pid, SIGTERM);
        waitpid(thumbs_pid, NULL, WNOHANG);
        thumbs_pid = 0;
    }
    thumbs_ticks = 0;
}

/* Drop the last search's pictures before a new list is drawn. */
static void tt_clear_thumbs(void)
{
    pid_t p;

    stop_thumbs();
    p = fork();
    if (p < 0)
        return;
    if (p == 0) {
        execl("/bin/sh", "sh", "-c",
              "rm -rf /tmp/minios-tt-thumbs /tmp/minios-tt-thumb-page.txt; "
              "mkdir -p /tmp/minios-tt-thumbs",
              (char *)0);
        _exit(0);
    }
    waitpid(p, NULL, 0);
}

/* The visible rows, in the order on screen. The saver names each picture
   by the video id, so a later row cannot pick up an earlier picture. */
static void tt_write_thumb_page(int start, int count)
{
    FILE *f;
    int i;

    f = fopen("/tmp/minios-tt-thumb-page.txt", "w");
    if (!f)
        return;
    for (i = 0; i < count && start + i < nres; i++) {
        if (!urls[start + i][0])
            continue;
        fprintf(f, "TikTok video\t%s\n", urls[start + i]);
    }
    fclose(f);
}

static void start_thumbs(int start, int count)
{
    char a[8], b[8];
    const char *bin = "/opt/minios/minios-tiktok-thumbs";

    if (nres < 1 || count < 1)
        return;
    if (access(bin, X_OK) != 0)
        bin = "/usr/local/bin/minios-tiktok-thumbs";
    if (access(bin, X_OK) != 0)
        return;
    stop_thumbs();
    tt_write_thumb_page(start, count);
    snprintf(a, sizeof(a), "0");
    snprintf(b, sizeof(b), "%d", count);
    thumbs_pid = fork();
    if (thumbs_pid == 0) {
        execl(bin, "minios-tiktok-thumbs", a, b, (char *)0);
        _exit(127);
    }
    thumbs_ticks = 40;
}

static void thumbs_for_page(void)
{
    char pth[80];
    int i, start = page * PAGE, need = 0;

    for (i = 0; i < PAGE && start + i < nres; i++) {
        if (!tt_thumb_file(start + i, pth, (int)sizeof pth) || access(pth, R_OK) != 0) {
            need = 1;
            break;
        }
    }
    if (need)
        start_thumbs(start, PAGE);
}

static void release_search_ram(void)
{
    if (search_pid > 0) {
        kill(-search_pid, SIGKILL);
        waitpid(search_pid, NULL, WNOHANG);
        search_pid = 0;
    }
    stop_thumbs();
    searching = 0;
    /* Drop search temps. Do not kill the MiniOS player if a clip is playing. */
    unlink("/tmp/minios-tt-search.html");
    unlink("/tmp/minios-tt-raw.txt");
    unlink("/tmp/minios-tt-rows.txt");
    unlink("/tmp/minios-tiktok.ready");
    unlink("/tmp/minios-tt-play.pid");
    unlink("/tmp/minios-tt-last.txt");
    unlink("/tmp/minios-tt-reason.txt");
    unlink("/tmp/minios-tt-cookies");
    unlink("/tmp/minios-tt-thumbs.pid");
    unlink("/tmp/minios-tt-thumb-page.txt");
    if (fork() == 0) {
        execl("/bin/sh", "sh", "-c",
              "pkill -f /opt/minios/minios-tiktok-search >/dev/null 2>&1 || true; "
              "pkill -f /usr/local/bin/minios-tiktok-search >/dev/null 2>&1 || true; "
              "pkill -f /opt/minios/minios-tiktok-thumbs >/dev/null 2>&1 || true; "
              "pkill -f /usr/local/bin/minios-tiktok-thumbs >/dev/null 2>&1 || true; "
              "if ! pgrep -f /opt/minios/minios-tt-player-bar >/dev/null 2>&1 && "
              "! pgrep -f /usr/local/bin/minios-tt-player-bar >/dev/null 2>&1; then "
              "  rm -f /tmp/minios-tt-flag /tmp/minios-tt-search.html "
              "/tmp/minios-tt-raw.txt /tmp/minios-tt-rows.txt "
              "/tmp/minios-tt-last.txt /tmp/minios-tt-cookies; "
              "  rm -rf /tmp/minios-tt-dl /tmp/minios-tt-thumbs; "
              "fi",
              (char *)0);
        _exit(0);
    }
}

/* The window goes away before any wait. The old close waited on the
 * search child first, so the click looked ignored. */
static void tt_close_search_now(void)
{
    if (dpy) {
        XUnmapWindow(dpy, win);
        XFlush(dpy);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        dpy = NULL;
    }
    release_search_ram();
    unlink("/tmp/minios-apps/tiktok.pid");
    unlink("/tmp/minios-tiktok.pid");
    unlink("/tmp/minios-tiktok.ready");
    if (fork() == 0) {
        execl("/opt/minios/minios-tt-cleanup", "minios-tt-cleanup", (char *)0);
        execl("/usr/local/bin/minios-tt-cleanup", "minios-tt-cleanup", (char *)0);
        _exit(0);
    }
}

static void finish_app(void)
{
    tt_close_search_now();
}

static const char *search_bin(void)
{
    if (access("/opt/minios/minios-tiktok-search", X_OK) == 0)
        return "/opt/minios/minios-tiktok-search";
    if (access("/usr/local/bin/minios-tiktok-search", X_OK) == 0)
        return "/usr/local/bin/minios-tiktok-search";
    return NULL;
}

static void clear_rows(void)
{
    int i;

    nres = 0;
    hover = -1;
    for (i = 0; i < MAXR; i++) {
        titles[i][0] = 0;
        urls[i][0] = 0;
        durs[i][0] = 0;
        views[i][0] = 0;
        ups[i][0] = 0;
    }
}

static char search_acc[1024];
static int search_acc_n;

static void play_item(int i);
static void tt_paint_load_line(void);
static int in_box(int x, int y, int bx, int by, int bw, int bh);

static void stop_search_child(void)
{
    if (search_pid <= 0)
        return;
    kill(-search_pid, SIGKILL);
    kill(search_pid, SIGKILL);
    waitpid(search_pid, NULL, 0);
    search_pid = 0;
}

/* One sentence, kept whole so the window can show every word. */
static const char tt_stopped_msg[] __attribute__((used)) =
    "Search stopped. Type a word, then Go search!";

static void tt_say_search_stopped(void)
{
    snprintf(status, sizeof(status), "%s", tt_stopped_msg);
}

/* Stop the search that is running, or clear the wait, so a new word
   can be searched immediately. The videos already listed stay. */
static void tt_stop_search_now(void)
{
    FILE *lf;

    lf = fopen("/tmp/minios-tiktok.log", "a");
    if (lf) {
        fprintf(lf, "stop search pid=%d\n", (int)search_pid);
        fclose(lf);
    }
    if (search_pid > 0) {
        kill(-search_pid, SIGKILL);
        kill(search_pid, SIGKILL);
        waitpid(search_pid, NULL, 0);
        search_pid = 0;
        tt_user_stopped = 1;
    }
    searching = 0;
    cool_until = 0;
    tt_say_search_stopped();
    paint();
    XFlush(dpy);
}

static int tt_finish_if_stopped(void)
{
    if (!tt_user_stopped)
        return 0;
    tt_user_stopped = 0;
    searching = 0;
    search_pid = 0;
    cool_until = 0;
    tt_say_search_stopped();
    paint();
    return 1;
}

/* A new hit is the first row. The same video moves to the front. */
static void tt_show_newest_hit(const char *title, const char *url,
                               const char *dur, const char *views_s,
                               const char *up)
{
    int i, found = -1;

    if (!url || !url[0])
        return;
    for (i = 0; i < nres; i++) {
        if (strcmp(urls[i], url) == 0) {
            found = i;
            break;
        }
    }
    if (found == 0) {
        snprintf(titles[0], sizeof(titles[0]), "%s", title ? title : "");
        snprintf(durs[0], sizeof(durs[0]), "%s", dur ? dur : "");
        snprintf(views[0], sizeof(views[0]), "%s", views_s ? views_s : "");
        snprintf(ups[0], sizeof(ups[0]), "%s", up ? up : "");
        return;
    }
    if (found > 0) {
        memmove(titles[1], titles[0], (size_t)found * sizeof(titles[0]));
        memmove(urls[1], urls[0], (size_t)found * sizeof(urls[0]));
        memmove(durs[1], durs[0], (size_t)found * sizeof(durs[0]));
        memmove(views[1], views[0], (size_t)found * sizeof(views[0]));
        memmove(ups[1], ups[0], (size_t)found * sizeof(ups[0]));
        snprintf(titles[0], sizeof(titles[0]), "%s", title ? title : "");
        snprintf(urls[0], sizeof(urls[0]), "%s", url);
        snprintf(durs[0], sizeof(durs[0]), "%s", dur ? dur : "");
        snprintf(views[0], sizeof(views[0]), "%s", views_s ? views_s : "");
        snprintf(ups[0], sizeof(ups[0]), "%s", up ? up : "");
        return;
    }
    if (nres >= MAXR)
        nres = MAXR - 1;
    if (nres > 0) {
        memmove(titles[1], titles[0], (size_t)nres * sizeof(titles[0]));
        memmove(urls[1], urls[0], (size_t)nres * sizeof(urls[0]));
        memmove(durs[1], durs[0], (size_t)nres * sizeof(durs[0]));
        memmove(views[1], views[0], (size_t)nres * sizeof(views[0]));
        memmove(ups[1], ups[0], (size_t)nres * sizeof(ups[0]));
    }
    snprintf(titles[0], sizeof(titles[0]), "%s", title ? title : "");
    snprintf(urls[0], sizeof(urls[0]), "%s", url);
    snprintf(durs[0], sizeof(durs[0]), "%s", dur ? dur : "");
    snprintf(views[0], sizeof(views[0]), "%s", views_s ? views_s : "");
    snprintf(ups[0], sizeof(ups[0]), "%s", up ? up : "");
    nres++;
    page = 0;
}

static int tt_video_id(const char *u, char *id, int n)
{
    const char *p;
    int i = 0;

    if (!u || !id || n < 2)
        return 0;
    p = strstr(u, "/video/");
    if (!p)
        return 0;
    p += 7;
    while (p[i] >= '0' && p[i] <= '9' && i + 1 < n) {
        id[i] = p[i];
        i++;
    }
    id[i] = 0;
    return i > 0;
}

/* A continue must not put a video back on screen that this search already listed. */
static int tt_row_was_shown(const char *url)
{
    FILE *f;
    char line[MAXL];
    char want[32], have[32];
    size_t n;

    if (!tt_ask_more || !tt_video_id(url, want, (int)sizeof want))
        return 0;
    f = fopen("/tmp/minios-tt-shown.txt", "r");
    if (!f)
        return 0;
    while (fgets(line, sizeof line, f)) {
        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        if (tt_video_id(line, have, (int)sizeof have) && strcmp(have, want) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int tt_row_was_watched(const char *url)
{
    FILE *f;
    char line[MAXL];
    char want[32], have[32];
    size_t n;

    if (!tt_ask_more || !tt_video_id(url, want, (int)sizeof want))
        return 0;
    f = fopen("/tmp/minios-tt-watched.txt", "r");
    if (!f)
        return 0;
    while (fgets(line, sizeof line, f)) {
        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        if (tt_video_id(line, have, (int)sizeof have) && strcmp(have, want) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

/* More videos keeps the list still. A full redraw on each new line
   cleared the window and looked like blinking. */
static void tt_paint_more_row(void)
{
    if (!tt_ask_more)
        return;
    tt_paint_load_line();
}

static void search_take_line(char *line)
{
    char *a, *b, *c, *d, *e;
    size_t L;

    if (!line)
        return;
    L = strlen(line);
    while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r'))
        line[--L] = 0;
    if (L == 0)
        return;
    if (strncmp(line, "STATUS\t", 7) == 0) {
        snprintf(status, sizeof(status), "%.*s", (int)sizeof(status) - 1, line + 7);
        if (tt_ask_more)
            tt_paint_load_line();
        else {
            paint();
            XFlush(dpy);
        }
        return;
    }
    a = line;
    b = strchr(a, '\t');
    if (!b)
        return;
    *b++ = 0;
    c = strchr(b, '\t');
    if (c)
        *c++ = 0;
    d = c ? strchr(c, '\t') : NULL;
    if (d)
        *d++ = 0;
    e = d ? strchr(d, '\t') : NULL;
    if (e)
        *e++ = 0;
    if (!is_canonical_video(b))
        return;
    if (tt_row_was_watched(b) || tt_row_was_shown(b))
        return;
    tt_show_newest_hit(a[0] ? a : b, b, c ? c : "", d ? d : "", e ? e : "");
    if (tt_ask_more)
        tt_paint_more_row();
    else {
        paint();
        XFlush(dpy);
    }
}

static void search_eat(const char *tmp, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        char ch = tmp[i];
        if (ch == '\n') {
            search_acc[search_acc_n] = 0;
            search_take_line(search_acc);
            search_acc_n = 0;
            continue;
        }
        if (ch == '\r')
            continue;
        if (search_acc_n < (int)sizeof(search_acc) - 1)
            search_acc[search_acc_n++] = ch;
    }
}

/* Stop the search, then play this row. The playlist is only the videos
   already on screen. */
static int play_shown_result(int index)
{
    FILE *lf;

    if (index < 0 || index >= nres || urls[index][0] == 0)
        return 0;
    if (!is_canonical_video(urls[index]))
        return 0;
    lf = fopen("/tmp/minios-tiktok.log", "a");
    if (lf) {
        fprintf(lf, "pick %d of %d\n", index, nres);
        fclose(lf);
    }
    stop_search_child();
    play_item(index);
    return 1;
}

/* Returns 1 when the user picks a video. Close and Escape leave the program. */
static int search_handle_event(XEvent *ev)
{
    if (ev->type == MapNotify) {
        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
        return 0;
    }
    if (ev->type == ClientMessage &&
        (Atom)ev->xclient.data.l[0] == wm_delete) {
        finish_app();
        exit(0);
    }
    if (ev->type == Expose) {
        paint();
        return 0;
    }
    if (ev->type == ConfigureNotify) {
        int nw = ev->xconfigure.width, nh = ev->xconfigure.height;
        if (nw > 200 && nh > 200 && (nw != dw || nh != dh)) {
            dw = nw;
            dh = nh;
            layout();
            paint();
        }
        return 0;
    }
    if (ev->type == KeyPress) {
        KeySym ks;
        char buf[8];
        XLookupString(&ev->xkey, buf, sizeof(buf), &ks, NULL);
        if (ks == XK_Escape) {
            finish_app();
            exit(0);
        }
        return 0;
    }
    if (ev->type == MotionNotify) {
        int y = ev->xmotion.y, i, nh = -1, start = page * PAGE;
        int shown = nres - start;
        if (shown > PAGE)
            shown = PAGE;
        for (i = 0; i < shown; i++) {
            int top = list_y + 8 + i * row_h;
            if (y >= top && y < top + row_h - 6)
                nh = start + i;
        }
        if (nh != hover) {
            hover = nh;
            paint();
        }
        return 0;
    }
    if (ev->type == ButtonPress) {
        int x = ev->xbutton.x, y = ev->xbutton.y, i;
        if (tt_stop_clicked(x, y)) {
            if (tt_stop_ready())
                tt_stop_search_now();
            return 0;
        }
        if (in_box(x, y, close_x, close_y, close_w, close_h)) {
            finish_app();
            exit(0);
        }
        if (in_box(x, y, prev_x, nav_y, nav_w, nav_h)) {
            if (can_prev()) {
                page--;
                paint();
            }
            return 0;
        }
        if (in_box(x, y, next_x, nav_y, nav_w, nav_h)) {
            if ((page + 1) * PAGE < nres) {
                page++;
                paint();
            }
            return 0;
        }
        {
            int start = page * PAGE;
            int shown = nres - start;
            if (shown > PAGE)
                shown = PAGE;
            for (i = 0; i < shown; i++) {
                int top = list_y + 8 + i * row_h;
                if (in_box(x, y, list_x + 6, top, list_w - 12, row_h - 6))
                    return play_shown_result(start + i);
            }
        }
    }
    return 0;
}

/* Read every X event already waiting. A click has to be removed from the
   queue or it sits there until the search program exits. */
static int search_handle_queued(void)
{
    XEvent ev;
    int n = 0;

    while (XPending(dpy) > 0 && n < 64) {
        XNextEvent(dpy, &ev);
        n++;
        if (search_handle_event(&ev))
            return 1;
    }
    return 0;
}

/* Read search output without freezing the window. */
static int search_pump(int fd)
{
    int xfd = ConnectionNumber(dpy);
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    search_acc_n = 0;
    while (searching && search_pid > 0) {
        fd_set fds;
        struct timeval tv;
        int maxfd, ready, nread;
        char tmp[256];

        if (search_handle_queued())
            return 1;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        FD_SET(xfd, &fds);
        maxfd = fd > xfd ? fd : xfd;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        ready = select(maxfd + 1, &fds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ready > 0 && FD_ISSET(xfd, &fds)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (search_handle_event(&ev))
                return 1;
            if (search_handle_queued())
                return 1;
        }
        if (ready == 0 || !FD_ISSET(fd, &fds))
            continue;
        nread = (int)read(fd, tmp, sizeof(tmp));
        if (nread == 0)
            break;
        if (nread < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            break;
        }
        search_eat(tmp, nread);
    }
    return 0;
}

static void do_search(void)
{
    int pfd[2];
    pid_t pid;
    const char *bin;
    int st;
    int picked;

    if (qlen < 1)
        return;
    if (!can_search())
        return;
    tt_user_stopped = 0;
    bin = search_bin();
    if (!bin) {
        snprintf(status, sizeof(status), "Search tool is missing.");
        paint();
        return;
    }
    searching = 1;
    page = 0;
    tt_clear_thumbs();
    clear_rows();
    snprintf(status, sizeof(status), "Searching. Up to %d seconds.", tt_search_secs);
    paint();
    XFlush(dpy);
    if (pipe(pfd) != 0) {
        searching = 0;
        snprintf(status, sizeof(status), "Search failed.");
        paint();
        return;
    }
    pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        dup2(pfd[1], 1);
        close(pfd[0]);
        close(pfd[1]);
        {
            char secbuf[8];

            snprintf(secbuf, sizeof(secbuf), "%d", tt_search_secs);
            execl(bin, "minios-tiktok-search", query, secbuf, (char *)0);
        }
        _exit(127);
    }
    search_pid = pid;
    setpgid(pid, pid);
    close(pfd[1]);
    picked = search_pump(pfd[0]);
    close(pfd[0]);
    if (tt_finish_if_stopped())
        return;
    if (picked) {
        searching = 0;
        if (!tt_covered) {
            cool_until = time(NULL) + 8;
            paint();
        }
        return;
    }
    if (search_pid > 0) {
        waitpid(search_pid, &st, 0);
        search_pid = 0;
    }
    searching = 0;
    cool_until = time(NULL) + 8;
    if (nres == 0) {
        FILE *rf = fopen("/tmp/minios-tt-reason.txt", "r");
        char reason[80];
        reason[0] = 0;
        if (rf) {
            if (fgets(reason, sizeof(reason), rf)) {
                size_t L = strlen(reason);
                while (L > 0 && (reason[L - 1] == '\n' || reason[L - 1] == '\r'))
                    reason[--L] = 0;
            }
            fclose(rf);
        }
        if (strcmp(reason, "blocked") == 0)
            snprintf(status, sizeof(status),
                     "Search sites hid the results. Try another word.");
        else if (strcmp(reason, "not-video") == 0)
            snprintf(status, sizeof(status),
                     "That link is not a TikTok video.");
        else
            snprintf(status, sizeof(status), "No TikTok results. Try another word.");
        /* Empty search: short pause only. A DuckDuckGo block lasts hours. */
        cool_until = time(NULL) + 15;
    } else {
        snprintf(status, sizeof(status), "%d TikTok posts. Click one to play.", nres);
        thumbs_for_page();
        /* Short pause so a second search for more words is possible. */
        cool_until = time(NULL) + 8;
    }
    paint();
}

/* Focus only after the window is already on screen. Before the first map,
   this call used to close the search window. */
static void tt_focus_query(void)
{
    XWindowAttributes wa;

    if (!dpy || !win)
        return;
    XMapRaised(dpy, win);
    memset(&wa, 0, sizeof wa);
    if (XGetWindowAttributes(dpy, win, &wa) && wa.map_state == IsViewable)
        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    XFlush(dpy);
}

static void tt_clear_ask_file(void)
{
    unlink("/tmp/minios-tt-ask");
    unlink("/tmp/minios-tt-ask.pid");
}

static void tt_run_more_search(void)
{
    tt_covered = 0;
    tt_row_paint_ms = 0;
    tt_focus_query();
    if (qlen < 1) {
        snprintf(status, sizeof(status), "Type a word, then Go search!");
        paint();
        tt_focus_query();
        return;
    }
    if (search_pid > 0)
        stop_search_child();
    searching = 0;
    cool_until = 0;
    tt_ask_more = 1;
    setenv("MINIOS_TT_CONTINUE", "1", 1);
    setenv("MINIOS_TT_WATCHED", "/tmp/minios-tt-watched.txt", 1);
    do_search();
    unsetenv("MINIOS_TT_CONTINUE");
    unsetenv("MINIOS_TT_WATCHED");
    tt_ask_more = 0;
    if (nres < 1) {
        cool_until = 0;
        fprintf(stderr, "search-v238 more\n");
        fprintf(stderr, "search-v245 go\n");
        snprintf(status, sizeof(status),
                 "No new video came back in time. Press Go search! again.");
        paint();
        tt_focus_query();
    } else {
        snprintf(tt_listed_query, sizeof(tt_listed_query), "%s", query);
    }
}

/* Same words again show links that were not on screen. A different word
   starts a new search. */
static void tt_go_search(void)
{
    int same;

    /* An empty screen still continues. More videos clears the list when
       nothing new arrived, and the same words must not start over. */
    same = (tt_listed_query[0] && strcmp(query, tt_listed_query) == 0);
    if (same)
        cool_until = 0;
    if (!can_search())
        return;
    if (same) {
        fprintf(stderr, "search-v244 same\n");
        if (nres < 1)
            fprintf(stderr, "search-v246 same\n");
        tt_ask_more = 1;
        setenv("MINIOS_TT_CONTINUE", "1", 1);
        if (access("/tmp/minios-tt-watched.txt", R_OK) == 0)
            setenv("MINIOS_TT_WATCHED", "/tmp/minios-tt-watched.txt", 1);
        do_search();
        unsetenv("MINIOS_TT_CONTINUE");
        unsetenv("MINIOS_TT_WATCHED");
        tt_ask_more = 0;
        if (nres < 1) {
            cool_until = 0;
            fprintf(stderr, "search-v245 go\n");
            snprintf(status, sizeof(status),
                     "No new video came back in time. Press Go search! again.");
            paint();
            tt_focus_query();
        }
    } else {
        do_search();
    }
    if (nres > 0)
        snprintf(tt_listed_query, sizeof(tt_listed_query), "%s", query);
}

static void tt_begin_fresh_search(void)
{
    tt_covered = 0;
    tt_focus_query();
    if (search_pid > 0)
        stop_search_child();
    searching = 0;
    cool_until = 0;
    page = 0;
    hover = -1;
    query[0] = 0;
    qlen = 0;
    clear_rows();
    tt_clear_thumbs();
    snprintf(status, sizeof(status), "Type a search and click Go search!");
    paint();
    tt_focus_query();
}

/* The player writes the ask file, then sends an X event. This reads that
   file once and removes it, including the pid file the player wrote. */
static void tt_take_end_ask(void)
{
    FILE *f;
    char line[32];
    size_t n;
    FILE *lf;

    if (access("/tmp/minios-tt-ask", F_OK) != 0)
        return;
    line[0] = 0;
    f = fopen("/tmp/minios-tt-ask", "r");
    if (f) {
        if (fgets(line, sizeof line, f)) {
            n = strlen(line);
            while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
                line[--n] = 0;
        }
        fclose(f);
    }
    tt_clear_ask_file();
    lf = fopen("/tmp/minios-tiktok.log", "a");
    if (lf) {
        fprintf(lf, "player-v233 ask %s\n", line);
        fclose(lf);
    }
    if (strcmp(line, "more") == 0)
        tt_run_more_search();
    else if (strcmp(line, "fresh") == 0)
        tt_begin_fresh_search();
}

/* The player writes this pid file and removes it when Close is clicked. */
static int tt_bar_alive(void)
{
    FILE *f;
    int pid = 0;
    char comm[32];
    char path[40];

    f = fopen("/tmp/minios-tt-player-bar.pid", "r");
    if (!f)
        return 0;
    if (fscanf(f, "%d", &pid) != 1)
        pid = 0;
    fclose(f);
    if (pid <= 1)
        return 0;
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    f = fopen(path, "r");
    if (!f)
        return 0;
    comm[0] = 0;
    if (!fgets(comm, sizeof(comm), f))
        comm[0] = 0;
    fclose(f);
    /* The name is longer than the 15 letters Linux keeps. */
    if (strncmp(comm, "minios-tt-playe", 15) != 0)
        return 0;
    tt_bar_seen = 1;
    return 1;
}

static int tt_opening_line(const char *s)
{
    return s && strncmp(s, "Opening the video", 17) == 0;
}

/* The load sentence is temporary. Clear it when the picture is up,
 * Close was clicked, or the player window has gone. */
static int tt_take_load_line(void)
{
    FILE *pf;
    char pcts[24];
    char lab[120];
    char next[140];
    int got = 0;
    int picture, quit, bar, gone;
    size_t L;

    if (!play_wait)
        return 0;
    next[0] = 0;
    pcts[0] = 0;
    lab[0] = 0;
    pf = fopen("/tmp/minios-tt-dl.progress", "r");
    if (pf) {
        if (fgets(pcts, sizeof(pcts), pf) && fgets(lab, sizeof(lab), pf)) {
            L = strlen(lab);
            while (L > 0 && (lab[L - 1] == '\n' || lab[L - 1] == '\r'))
                lab[--L] = 0;
            if (lab[0]) {
                int pct = atoi(pcts);
                if (pct > 0 && pct < 100)
                    snprintf(next, sizeof(next), "%s %d%%", lab, pct);
                else
                    snprintf(next, sizeof(next), "%s", lab);
                got = 1;
            }
        }
        fclose(pf);
    }
    if (got && strstr(next, "did not start") != NULL) {
        play_wait = 0;
        snprintf(status, sizeof(status), "%s", next);
        return 1;
    }
    picture = access("/tmp/minios-tt-picture.up", F_OK) == 0;
    quit = access("/tmp/minios-tt-mp.quit", F_OK) == 0;
    bar = tt_bar_alive();
    gone = tt_bar_seen && !bar;
    /* The player window is up. Stop the 400ms wake. That wake was
     * still running during the clip and took the only core. */
    if (picture || quit || gone || bar) {
        play_wait = 0;
        if (tt_opening_line(status) || tt_opening_line(next) || tt_opening_line(lab))
            status[0] = 0;
        else if (got)
            snprintf(status, sizeof(status), "%s", next);
        return 1;
    }
    if (!got)
        return 0;
    if (strcmp(status, next) == 0)
        return 0;
    snprintf(status, sizeof(status), "%s", next);
    return 1;
}

static int tt_visible_thumbs(void)
{
    char pth[80];
    int i, start = page * PAGE, n = 0;

    for (i = 0; i < PAGE && start + i < nres; i++) {
        if (tt_thumb_file(start + i, pth, (int)sizeof pth) && access(pth, R_OK) == 0)
            n++;
    }
    return n;
}

/* Returns 2 when a result picture changed, 1 when only the load line or
 * the Wait count changed, and 0 when nothing on screen changed. */
static int tt_on_timer(void)
{
    int full = 0;
    int part = 0;
    int cool, thumbs;

    if (tt_take_load_line())
        part = 1;
    if (thumbs_pid > 0 && kill(thumbs_pid, 0) != 0) {
        waitpid(thumbs_pid, NULL, WNOHANG);
        thumbs_pid = 0;
        thumbs_ticks = 0;
    }
    cool = cool_left();
    thumbs = tt_visible_thumbs();
    if (tt_thumbs_shown < 0)
        tt_thumbs_shown = thumbs;
    else if (thumbs != tt_thumbs_shown) {
        tt_thumbs_shown = thumbs;
        full = 1;
    }
    if (tt_cool_shown < 0)
        tt_cool_shown = cool;
    else if (cool != tt_cool_shown) {
        tt_cool_shown = cool;
        part = 1;
    }
    if (full)
        return 2;
    if (part)
        return 1;
    return 0;
}

/* Redraw the search button and the load line. Do not redraw the whole window. */
static void tt_paint_load_line(void)
{
    char lab[160];
    char golab[32];
    int left, pages;
    unsigned long gbg = btn, gfg = fg;

    if (!dpy)
        return;
    left = cool_left();
    if (searching) {
        snprintf(golab, sizeof(golab), "Searching...");
        gbg = grey;
        gfg = dim;
    } else if (left > 0) {
        snprintf(golab, sizeof(golab), "Wait %ds", left);
        gbg = grey;
        gfg = dim;
    } else {
        snprintf(golab, sizeof(golab), "Go search!");
    }
    fill_rect(gbg, go_x, go_y, go_w, go_h);
    box(gfg, go_x, go_y, go_w, go_h);
    text_at(gfg, go_x + 18, go_y + 16, golab);
    tt_paint_stop_button();
    tt_paint_time_button();
    tt_paint_quality_button();
    fill_rect(panel, 0, nav_y - 26, dw, 24);
    pages = nres > 0 ? (nres + PAGE - 1) / PAGE : 0;
    if (play_wait && status[0])
        snprintf(lab, sizeof(lab), "%s", status);
    else if (searching && nres > 0)
        snprintf(lab, sizeof(lab), "%.*s  Click one to play.",
                 (int)sizeof(lab) - 21, search_wait_text());
    else if (searching)
        snprintf(lab, sizeof(lab), "%s", search_wait_text());
    else if (nres > 0)
        snprintf(lab, sizeof(lab), "Page %d of %d  (%d videos)",
                 page + 1, pages, nres);
    else if (status[0])
        snprintf(lab, sizeof(lab), "%s", status);
    else
        snprintf(lab, sizeof(lab), "Type a word, then Go search!");
    text_ellip(dim, list_x + 8, nav_y - 10, dw - list_x - 16, lab);
    XFlush(dpy);
}

static int tt_comm_is(int pid, const char *prefix)
{
    char path[40];
    char comm[32];
    FILE *f;
    size_t n;

    if (pid <= 1 || !prefix)
        return 0;
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    f = fopen(path, "r");
    if (!f)
        return 0;
    comm[0] = 0;
    if (!fgets(comm, sizeof(comm), f))
        comm[0] = 0;
    fclose(f);
    n = strlen(prefix);
    return strncmp(comm, prefix, n) == 0;
}

static int tt_parent_of(int pid)
{
    char path[40];
    char line[64];
    FILE *f;
    int ppid = 0;

    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    f = fopen(path, "r");
    if (!f)
        return 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "PPid: %d", &ppid) == 1)
            break;
    }
    fclose(f);
    return ppid;
}

/* A paused clip is still a player. Leave it and the next clip shares
 * the one core, so the new picture is slower. Stop that player first. */
static void tt_stop_previous_clip(void)
{
    FILE *f;
    int pid = 0;
    int ppid;

    f = fopen("/tmp/minios-tt-mp.quit", "w");
    if (f) {
        fputs("1\n", f);
        fclose(f);
    }
    f = fopen("/tmp/minios-tt-mp.pid", "r");
    if (!f)
        return;
    if (fscanf(f, "%d", &pid) != 1)
        pid = 0;
    fclose(f);
    if (!tt_comm_is(pid, "mplayer"))
        return;
    ppid = tt_parent_of(pid);
    kill(pid, SIGKILL);
    if (tt_comm_is(ppid, "minios-tt-mplay"))
        kill(ppid, SIGKILL);
}

/* A new click must not inherit the previous Close or the previous picture. */
static void tt_arm_play(void)
{
    tt_stop_previous_clip();
    unlink("/tmp/minios-tt-mp.quit");
    unlink("/tmp/minios-tt-picture.up");
    tt_bar_seen = 0;
}

static int pgrep_player(void)
{
    if (system("pgrep -f /opt/minios/minios-tt-player-bar >/dev/null 2>&1") == 0)
        return 1;
    if (system("pgrep -f /usr/local/bin/minios-tt-player-bar >/dev/null 2>&1") == 0)
        return 1;
    return 0;
}

/* The list stays in memory. The window leaves the screen so the player
   is what the user sees. More videos and New search map it again. */
static void tt_hide_for_play(void)
{
    FILE *lf;

    tt_covered = 1;
    play_wait = 0;
    cool_until = 0;
    stop_thumbs();
    lf = fopen("/tmp/minios-tiktok.log", "a");
    if (lf) {
        fputs("search-v234 hide\n", lf);
        fclose(lf);
    }
    if (dpy && win) {
        XUnmapWindow(dpy, win);
        XFlush(dpy);
    }
}

static void play_item(int i)
{
    FILE *f;
    int n;

    if (i < 0 || i >= nres || urls[i][0] == 0) {
        snprintf(status, sizeof(status), "Select a TikTok result first.");
        paint();
        return;
    }
    if (!is_canonical_video(urls[i])) {
        snprintf(status, sizeof(status), "That result is not a TikTok video link.");
        paint();
        return;
    }
    tt_arm_play();
    f = fopen("/tmp/minios-tt-flag", "w");
    if (f) {
        fputs("1\n", f);
        fclose(f);
    }
    f = fopen("/tmp/minios-tt-playlist", "w");
    if (f) {
        fprintf(f, "%d\n", i);
        for (n = 0; n < nres; n++)
            fprintf(f, "%s\t%s\t0\n", urls[n],
                    titles[n][0] ? titles[n] : urls[n]);
        fclose(f);
    }
    f = fopen("/tmp/minios-tt-player-mode", "w");
    if (f) {
        fputs("tiktok\n", f);
        fclose(f);
    }
    f = fopen("/tmp/minios-tt-dl.progress", "w");
    if (f) {
        fputs("0\nDownloading TikTok video\n", f);
        fclose(f);
    }
    play_wait = 1;
    {
        char clean[81];
        const char *src = titles[i][0] ? titles[i] : "Selected video";
        int k, nch = 0;

        for (k = 0; src[k] && nch < 80; k++) {
            char ch = src[k];
            if (ch == '"')
                ch = '\'';
            else if (ch == '\n' || ch == '\r' || ch == '\t')
                ch = ' ';
            clean[nch++] = ch;
        }
        clean[nch] = 0;
        snprintf(status, sizeof(status), "Finding \"%.70s\"", clean);
    }
    unlink("/tmp/minios-tt-mp.fail");
    /* The player starts the clip itself, the same way its playlist button
       does. Also starting minios-tiktok-play here made the two loads cancel
       each other, so the player opened and the video failed. */
    if (!pgrep_player()) {
        if (fork() == 0) {
            execl("/opt/minios/minios-tt-player-bar", "minios-tt-player-bar",
                  "--youtube", (char *)0);
            execl("/usr/local/bin/minios-tt-player-bar", "minios-tt-player-bar",
                  "--youtube", (char *)0);
            _exit(127);
        }
    } else {
        f = fopen("/tmp/minios-tt-mp.reload", "w");
        if (f) {
            fputs("1\n", f);
            fclose(f);
        }
    }
    tt_hide_for_play();
}

static int in_box(int x, int y, int bx, int by, int bw, int bh)
{
    return x >= bx && x <= bx + bw && y >= by && y <= by + bh;
}

/* Match the YouTube search window. 40 pixels on the left keeps the
 * window name on screen. The dock stays clear at the bottom. */
static void tt_place_like_youtube(int *x, int *y, int *w, int *h)
{
    int sw = DisplayWidth(dpy, scr);
    int sh = DisplayHeight(dpy, scr);

    *x = 40;
    *y = 4;
    *w = sw - 40 - 4;
    *h = sh - 56 - 4;
    if (*w < 640)
        *w = 640;
    if (*h < 420)
        *h = 420;
}

int main(void)
{
    XColor col, dummy;
    int wx, wy;
    Colormap cmap;
    XEvent ev;
    KeySym ks;
    char buf[8];

    dpy = XOpenDisplay(NULL);
    if (!dpy)
        return 1;
    XSetErrorHandler(x_error);
    scr = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, scr);
    bg = WhitePixel(dpy, scr);
    fg = BlackPixel(dpy, scr);
    btn = dim = accent = field = panel = tile = playc = WhitePixel(dpy, scr);
    if (XAllocNamedColor(dpy, cmap, "#d4d0c8", &col, &dummy))
        bg = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#000000", &col, &dummy))
        fg = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#c0c0c0", &col, &dummy))
        btn = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#b8d4dc", &col, &dummy))
        accent = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#404040", &col, &dummy))
        dim = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#ffffff", &col, &dummy))
        field = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#e8e8e8", &col, &dummy))
        panel = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#303030", &col, &dummy))
        tile = col.pixel;
    if (XAllocNamedColor(dpy, cmap, "#00b4d8", &col, &dummy))
        playc = col.pixel;
    grey = btn;
    if (XAllocNamedColor(dpy, cmap, "#9a9a9a", &col, &dummy))
        grey = col.pixel;
    /* flwm draws the window name on the left of the frame. YouTube
     * starts 40 pixels in so that name stays on screen. A window at
     * x=0 with the screen width hides "MiniOS TikTok". */
    tt_place_like_youtube(&wx, &wy, &dw, &dh);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), wx, wy, dw, dh, 1, fg, bg);
    {
        XSizeHints sh;
        memset(&sh, 0, sizeof(sh));
        sh.flags = USPosition | PPosition | PSize | PMinSize;
        sh.x = wx;
        sh.y = wy;
        sh.width = dw;
        sh.height = dh;
        sh.min_width = 640;
        sh.min_height = 400;
        XSetWMNormalHints(dpy, win, &sh);
    }
    XSelectInput(dpy, win, ExposureMask | ButtonPressMask | KeyPressMask |
                 PointerMotionMask | StructureNotifyMask);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XStoreName(dpy, win, "MiniOS TikTok");
    gc = XCreateGC(dpy, win, 0, NULL);
    fs = XLoadQueryFont(dpy, "9x15");
    if (!fs)
        fs = XLoadQueryFont(dpy, "fixed");
    if (fs)
        XSetFont(dpy, gc, fs->fid);
    layout();
    tt_load_quality();
    snprintf(status, sizeof(status), "Type a search and click Go search!");
    XMapRaised(dpy, win);
    /* Focus after MapNotify. Calling it here races the map and the
     * X server returns BadMatch, which used to exit the program. */
    paint();
    {
        FILE *rf;
        rf = fopen("/tmp/minios-tiktok.ready", "w");
        if (rf) {
            fputs("tiktok-stays-open\n", rf);
            fclose(rf);
        }
        mkdir("/tmp/minios-apps", 0755);
        rf = fopen("/tmp/minios-apps/tiktok.pid", "w");
        if (rf) {
            fprintf(rf, "%d\n", (int)getpid());
            fclose(rf);
        }
        rf = fopen("/tmp/minios-tiktok.pid", "w");
        if (rf) {
            fprintf(rf, "%d\n", (int)getpid());
            fclose(rf);
        }
        if (fork() == 0) {
            execl("/opt/minios/minios-loading", "minios-loading", "stop", (char *)0);
            execl("/usr/local/bin/minios-loading", "minios-loading", "stop", (char *)0);
            _exit(0);
        }
    }
    while (1) {
        if (!tt_covered && (thumbs_pid > 0 || cool_left() > 0 || play_wait) && !XPending(dpy)) {
            fd_set fds;
            struct timeval tv;
            int xfd = ConnectionNumber(dpy);
            int why;
            FD_ZERO(&fds);
            FD_SET(xfd, &fds);
            tv.tv_sec = 0;
            tv.tv_usec = 400000;
            if (select(xfd + 1, &fds, NULL, NULL, &tv) == 0) {
                why = tt_on_timer();
                if (why == 2)
                    paint();
                else if (why == 1)
                    tt_paint_load_line();
                continue;
            }
        }
        XNextEvent(dpy, &ev);
        tt_take_end_ask();
        if (ev.type == MapNotify) {
            /* Quit and More videos map this window again. The hide flag
               must drop or the list stops updating after the player closes. */
            tt_covered = 0;
            XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
        } else if (ev.type == ClientMessage &&
            (Atom)ev.xclient.data.l[0] == wm_delete) {
            finish_app();
            return 0;
        } else if (ev.type == Expose)
            paint();
        else if (ev.type == ConfigureNotify) {
            int nw = ev.xconfigure.width, nh = ev.xconfigure.height;
            if (nw > 200 && nh > 200 && (nw != dw || nh != dh)) {
                dw = nw;
                dh = nh;
                layout();
                paint();
            }
        } else if (ev.type == KeyPress) {
            int n = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL);
            if (ks == XK_Return) {
                tt_go_search();
            } else if (ks == XK_BackSpace && qlen > 0) {
                query[--qlen] = 0;
                paint();
            } else if (ks == XK_Escape) {
                finish_app();
                return 0;
            }
            else if (n == 1 && isprint((unsigned char)buf[0]) && qlen < 80) {
                query[qlen++] = buf[0];
                query[qlen] = 0;
                paint();
            }
        } else if (ev.type == MotionNotify) {
            int y = ev.xmotion.y, i, nh = -1, start = page * PAGE;
            int shown = nres - start;
            if (shown > PAGE)
                shown = PAGE;
            for (i = 0; i < shown; i++) {
                int top = list_y + 8 + i * row_h;
                if (y >= top && y < top + row_h - 6)
                    nh = start + i;
            }
            if (nh != hover) {
                hover = nh;
                paint();
            }
        } else if (ev.type == ButtonPress) {
            int x = ev.xbutton.x, y = ev.xbutton.y, i;
            if (in_box(x, y, close_x, close_y, close_w, close_h)) {
                finish_app();
                return 0;
            } else if (tt_time_clicked(x, y)) {
                if (!searching)
                    tt_cycle_search_time();
                paint();
            } else if (tt_quality_clicked(x, y)) {
                if (!searching)
                    tt_toggle_quality();
                paint();
            } else if (tt_stop_clicked(x, y)) {
                if (tt_stop_ready())
                    tt_stop_search_now();
            } else if (in_box(x, y, go_x, go_y, go_w, go_h)) {
                tt_go_search();
            } else if (in_box(x, y, prev_x, nav_y, nav_w, nav_h)) {
                if (can_prev()) {
                    page--;
                    thumbs_for_page();
                    paint();
                }
            } else if (in_box(x, y, next_x, nav_y, nav_w, nav_h)) {
                if ((page + 1) * PAGE < nres) {
                    page++;
                    thumbs_for_page();
                    paint();
                }
            } else {
                int start = page * PAGE;
                int shown = nres - start;
                if (shown > PAGE)
                    shown = PAGE;
                for (i = 0; i < shown; i++) {
                    int top = list_y + 8 + i * row_h;
                    if (y >= top && y < top + row_h - 6) {
                        play_item(start + i);
                        break;
                    }
                }
            }
        }
    }
}
