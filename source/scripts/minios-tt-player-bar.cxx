/* MiniOS-Player-v39 MiniOS Media Player
 * Loop button is Off / One / All only. Do not send mplayer "loop N"
 * (that paints Loop: 4 on the picture).
 */
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <FL/x.H>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifndef XF86XK_AudioLowerVolume
#define XF86XK_AudioLowerVolume 0x1008FF11
#define XF86XK_AudioMute        0x1008FF12
#define XF86XK_AudioRaiseVolume 0x1008FF13
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMD "/tmp/minios-tt-mp.cmd"
#define OUT "/tmp/minios-tt-mp.out"
#define PIDF "/tmp/minios-tt-mp.pid"
#define FAILF "/tmp/minios-tt-mp.fail"
#define SUBF "/tmp/minios-ttplay.sub"
#define GEOMF "/tmp/minios-tt-mp.geom"
#define PROGF "/tmp/minios-tt-dl.progress"
#define PLF "/tmp/minios-tt-playlist"
#define LOCALPL "/home/tc/minios-local.m3u"
#define MODEFILE "/tmp/minios-tt-player-mode"
#define SHORTF "/tmp/minios-tt-shorts"
#define LIST_FT 32
#define MENU_H 0
#define BAR_H 84
#define DECO_H 22
#define TITLE_TOP 36
#define TITLE_H 56
#define CARD_PAD 8
#define LIST_W 300
#define LIST_HD 26
#define TITLE "MiniOS TikTok Player"
#define TIKTOKF "/tmp/minios-tt-flag"
#define TTWATCH "/tmp/minios-tt-watched.txt"
#define TTASK "/tmp/minios-tt-ask"
#define TTASKPID "/tmp/minios-tt-ask.pid"
#define QUITF "/tmp/minios-tt-mp.quit"
#define BARPID "/tmp/minios-tt-player-bar.pid"
#define GENF "/tmp/minios-tt-play-gen"
#define DLPID "/tmp/minios-tt-dl.pid"
#define DONEF "/tmp/minios-tt-mp.done"
#define RELOADF "/tmp/minios-tt-mp.reload"
#define MAXPL 40
#define IC_PLAY 1
#define IC_PAUSE 2
#define IC_PREV 3
#define IC_NEXT 4
#define IC_MIN 5
#define IC_MAX 6
#define IC_CLOSE 7

/* Navy card + cyan seek. White text. Yellow Tab ring. No images. */
static const Fl_Color C_CYAN = fl_rgb_color(0, 200, 220);
static const Fl_Color C_CYAN_DIM = fl_rgb_color(120, 190, 200);
static const Fl_Color C_NAVY = fl_rgb_color(14, 20, 34);
static const Fl_Color C_NAVY_HI = fl_rgb_color(22, 30, 48);
static const Fl_Color C_FACE = fl_rgb_color(22, 30, 48);
static const Fl_Color C_FACE_ON = fl_rgb_color(0, 70, 82);
static const Fl_Color C_TRACK = fl_rgb_color(36, 46, 64);
static const Fl_Color C_QUIT = fl_rgb_color(120, 28, 28);
static const Fl_Color C_FOCUS = fl_rgb_color(255, 220, 0);
static const Fl_Color C_TEXT = fl_rgb_color(255, 255, 255);

static Display *dpy;
static Window mpwin;
static Window shield;
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
static unsigned long tt_pause_at;
static int tt_bar_hold = -1;
static int tt_slave_tpos = -1;
static unsigned long tt_bar_ms;
static int tt_bar_follow;
static int tt_vol_push;
static int tt_bar_frozen = -1;
static int tt_pct_frozen = -1;
static int tt_spot = -1;
static int tt_spot_pct = -1;
static int tt_play_at = -1;
static unsigned long tt_play_ms;
static int tt_load_gave_up;
static unsigned long tt_load_since;
static unsigned long tt_key_hold;
static char tt_clip_shown[160];
static char tt_row_shown[160];
static int have_playback;
static int load_failed;
static int dl_pct;
static char dl_lab[48];
static pid_t mpid;
static int dw, dh;
static unsigned long quiet_until;
static int shorts_on, queue_on, pl_count, pl_cur, user_quit, skip_busy;
static int really_played, play_gen, saw_start, clip_done;
static int tt_end_asked;
static int tt_end_hold;
static Fl_Window *tt_endw;
static unsigned char tt_row_started[MAXPL];
static unsigned long play_alive_since;
static int max_tpos;
static int last_ui_dl = -1;
static char last_ui_lab[48];
static char pl_urls[MAXPL][160];
static char pl_titles[MAXPL][120];
static char local_path[256];
static int bar_hidden;
static unsigned long last_mouse;
static int last_ptr_x = -1, last_ptr_y = -1;
static int card_x, card_y, card_w, card_h;
static int win_max = 1;
static int win_min;
static int drag_on, drag_ox, drag_oy;
static int chooser_open;
static int vol_grabbed;
static int local_mode = 1;

static void start_pl_item(int idx);
static int local_ready(void);
static void start_local_file(const char *path);
static void restack_play(void);
static void pin_stage(void);
static void place_shield(void);
static void tt_show_stage(void);
static void drop_pl_item(int idx);
static void save_pl_index(void);
static void raise_resmon(void);
static void note_mouse(void);
static void show_bar(void);
static void hide_bar(void);
static void open_audio_folder(void);
static void hide_add_list(void);
static void open_playlist(void);
static void hide_playlist(void);
static void cache_store(const char *url);
static int cache_play(const char *url);
static void mark_seen(void);
static void read_progress(void);
static void blank_load(void);
static void request_quit(void);
static void grab_keys(void);
static void ungrab_keys(void);
static void show_playlist_panel(void);
static void listen_video_clicks(Window w);
static int signal_mplayers(int sig);
static int send_cmd(const char *s);
static int is_local_url(const char *u);
static void write_player_mode(void);
static void read_player_mode(void);
static void load_local_playlist(void);
static void keep_only_local_items(void);
static void save_local_playlist(void);
static void clear_playlist(void);
static void clear_youtube_media(void);
static void apply_loop_to_player(void);
static void tt_drop_bar_clock(void);
static void tt_commit_volume(void);
static void tt_begin_bar_clip(void);
static void tt_mark_bar_pause(int now_paused);
static void tt_note_bar_seek(int sec);
static void tt_show_bar_time(unsigned long now);
static void tt_note_play_started(void);
static void tt_note_watched(void);
static void tt_hide_end_popup(void);
static void tt_offer_next_list(void);
static void tt_offer_if_last(void);
static void tt_resume_after_more(void);
static void tt_hold_list_end(void);
static void tt_keep_end_popup(void);
static void tt_show_search_after_play(void);
static void refresh_ui(void);
static void toggle_pause(const char *why);
static void card_defaults(void);
static void video_rect(int *vx, int *vy, int *vw, int *vh);
static void apply_card(int move_video);
static void toggle_max(void);
static void toggle_min(void);
static void layout_bar(void);

class BarWin;
class LoadWin;
class TitleWin;
class IconBtn;
class TextBtn;
class SeekBar;

static Fl_Window *barw;
static Fl_Window *loadw;
static Fl_Window *titlew;
static IconBtn *btn_tog, *btn_prev, *btn_next, *btn_min, *btn_max, *btn_close;
static TextBtn *btn_auto, *btn_loop, *btn_add, *btn_list, *btn_vm, *btn_vp, *btn_cc, *btn_quit;
static TextBtn *btn_pl_save, *btn_pl_del, *btn_pl_clear;
static Fl_Window *listw;
static Fl_Hold_Browser *list_br;
static Fl_Box *list_hd;
static Fl_Window *addw;
static Fl_Hold_Browser *add_br;
static char add_paths[MAXPL][256];
static int add_count;
static SeekBar *seek_bar;
static Fl_Box *lab_t0, *lab_t1, *lab_st, *lab_vol;

static unsigned long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)tv.tv_sec * 1000UL + (unsigned long)tv.tv_usec / 1000UL;
}

static void tt_read_clip_name(char *dst, int n)
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
        if (c == '\n' || c == '\r')
            break;
        if (c == '\t')
            c = ' ';
        dst[i++] = (char)c;
    }
    while (i > 0 && dst[i - 1] == ' ')
        i--;
    dst[i] = 0;
    fclose(f);
}

static void tt_fit_clip_line(char *s, int room)
{
    int len;

    if (!s)
        return;
    if (room < 24) {
        s[0] = 0;
        return;
    }
    len = (int)strlen(s);
    while (len > 1 && (int)fl_width(s) > room) {
        len--;
        while (len > 0 && ((unsigned char)s[len] & 0xc0) == 0x80)
            len--;
        if (len >= 3) {
            s[len - 3] = '.';
            s[len - 2] = '.';
            s[len - 1] = '.';
        }
        s[len] = 0;
    }
}

static const char *tt_playlist_row_title(void)
{
    if (pl_cur >= 0 && pl_cur < pl_count) {
        if (pl_titles[pl_cur][0])
            return pl_titles[pl_cur];
        if (pl_urls[pl_cur][0])
            return pl_urls[pl_cur];
    }
    return "";
}

static void tt_draw_playlist_title(int wide, int top, int tall)
{
    const char *name;
    char file_name[160];
    char line[280];
    int room;

    name = tt_playlist_row_title();
    file_name[0] = 0;
    if (!name[0]) {
        tt_read_clip_name(file_name, (int)sizeof file_name);
        name = file_name;
    }
    if (!name[0])
        return;
    snprintf(line, sizeof line, "Now playing - %s", name);
    room = wide - 24;
    if (room < 36)
        return;
    fl_font(FL_HELVETICA_BOLD, 13);
    fl_color(C_TEXT);
    tt_fit_clip_line(line, room);
    fl_draw(line, 12, top, room, tall, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
}

/* The photo puts this line on the lower right of the control bar.
   Shorten it when the space beside the play buttons runs out. */
static void tt_draw_title_lower_right(int x, int y, int wide, int tall)
{
    const char *name;
    char file_name[160];
    char line[280];
    int room;
    static int tt_title_logged;

    name = tt_playlist_row_title();
    file_name[0] = 0;
    if (!name[0]) {
        tt_read_clip_name(file_name, (int)sizeof file_name);
        name = file_name;
    }
    if (!name[0] || wide < 36)
        return;
    snprintf(line, sizeof line, "Now playing - %s", name);
    room = wide - 4;
    fl_font(FL_HELVETICA_BOLD, 13);
    fl_color(C_TEXT);
    tt_fit_clip_line(line, room);
    fl_draw(line, x, y, room, tall, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    if (!tt_title_logged) {
        tt_title_logged = 1;
        fprintf(stderr, "player-v241 title\n");
    }
}

static void tt_note_playlist_title(void)
{
    const char *name;
    char file_name[160];

    if (!titlew || !titlew->shown())
        return;
    name = tt_playlist_row_title();
    file_name[0] = 0;
    if (!name[0]) {
        tt_read_clip_name(file_name, (int)sizeof file_name);
        name = file_name;
    }
    if (strcmp(name, tt_row_shown) == 0)
        return;
    snprintf(tt_row_shown, sizeof tt_row_shown, "%s", name);
    titlew->redraw();
    if (barw && barw->shown())
        barw->redraw();
}

static int file_exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

static int is_local_url(const char *u)
{
    if (!u || !u[0])
        return 0;
    if (u[0] == '/')
        return 1;
    if (strncmp(u, "file:", 5) == 0)
        return 1;
    return 0;
}

static void write_player_mode(void)
{
    FILE *f = fopen(MODEFILE, "w");
    if (!f)
        return;
    fputs(local_mode ? "local\n" : "youtube\n", f);
    fclose(f);
}

static void read_player_mode(void)
{
    FILE *f;
    char buf[24];

    local_mode = 1;
    f = fopen(MODEFILE, "r");
    if (!f)
        return;
    if (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, "youtube", 7) == 0)
            local_mode = 0;
    }
    fclose(f);
}

static void fill_round_rect(int x, int y, int w, int h, int r)
{
    if (r < 2)
        r = 2;
    if (r * 2 > w)
        r = w / 2;
    if (r * 2 > h)
        r = h / 2;
    fl_rectf(x + r, y, w - 2 * r, h);
    fl_rectf(x, y + r, w, h - 2 * r);
    fl_pie(x, y, 2 * r, 2 * r, 90, 180);
    fl_pie(x + w - 2 * r, y, 2 * r, 2 * r, 0, 90);
    fl_pie(x, y + h - 2 * r, 2 * r, 2 * r, 180, 270);
    fl_pie(x + w - 2 * r, y + h - 2 * r, 2 * r, 2 * r, 270, 360);
}

static void card_defaults(void)
{
    if (win_max) {
        card_x = 0;
        card_y = 0;
        card_w = dw;
        card_h = dh;
        return;
    }
    if (win_min) {
        card_w = 300;
        card_h = TITLE_H;
        card_x = 16;
        card_y = dh - TITLE_H - 58;
        if (card_y < 8)
            card_y = 8;
        return;
    }
    card_w = dw - 72;
    if (card_w > 920)
        card_w = 920;
    if (card_w < 480)
        card_w = (dw > 32) ? dw - 16 : dw;
    card_h = card_w * 9 / 16 + TITLE_H + 10;
    if (card_h > dh - 88)
        card_h = dh - 88;
    if (card_h < 280)
        card_h = (dh > 56) ? dh - 48 : dh;
    card_x = (dw - card_w) / 2;
    card_y = (dh - card_h - 52) / 2;
    if (card_y < 8)
        card_y = 8;
}

static void video_rect(int *vx, int *vy, int *vw, int *vh)
{
    int x, y, w, h;

    x = card_x + CARD_PAD;
    y = card_y + TITLE_H;
    w = card_w - 2 * CARD_PAD;
    h = card_h - TITLE_H - CARD_PAD;
    if (w < 80)
        w = 80;
    if (h < 40)
        h = 40;
    if ((shorts_on || file_exists(SHORTF) || file_exists(TIKTOKF)) && h > 40) {
        int sw = h * 9 / 16;
        int sh = h;
        if (sw < 120)
            sw = 120;
        if (sw > w) {
            sw = w;
            sh = w * 16 / 9;
            if (sh > h)
                sh = h;
            y += (h - sh) / 2;
            h = sh;
        }
        x += (w - sw) / 2;
        w = sw;
    }
    *vx = x;
    *vy = y;
    *vw = w;
    *vh = h;
}

static void dismiss_popup(void)
{
    if (file_exists("/opt/minios/minios-loading"))
        (void)system("/opt/minios/minios-loading stop >/dev/null 2>&1");
    (void)system("pkill -x xmessage >/dev/null 2>&1; rm -f /tmp/minios-run-alert.lock");
}

static void bump_gen(void)
{
    FILE *f;

    play_gen++;
    if (play_gen < 1)
        play_gen = 1;
    f = fopen(GENF, "w");
    if (f) {
        fprintf(f, "%d\n", play_gen);
        fclose(f);
    }
}

static void stop_download(void)
{
    (void)system("true >/dev/null 2>&1");
    (void)system("pkill -f minios-tiktok-play >/dev/null 2>&1");
    (void)system("pkill -f minios-yt-resolve >/dev/null 2>&1");
    (void)system("pkill -f yt-dlp-2026 >/dev/null 2>&1");
    rmdir("/tmp/minios-ttplay.lock");
    unlink(DLPID);
}

static int download_alive(void)
{
    FILE *f;
    pid_t p = 0;

    f = fopen(DLPID, "r");
    if (!f)
        return 0;
    if (fscanf(f, "%d", &p) != 1)
        p = 0;
    fclose(f);
    if (p > 1 && kill(p, 0) == 0)
        return 1;
    unlink(DLPID);
    return 0;
}

static void ungrab_vol_keys(void)
{
    Window root;
    KeySym syms[] = {
        XK_9, XK_KP_9, XK_0, XK_KP_0,
        XK_minus, XK_underscore, XK_KP_Subtract,
        XK_plus, XK_equal, XK_KP_Add,
        XK_m, XK_M, XK_space,
        XF86XK_AudioLowerVolume, XF86XK_AudioRaiseVolume, XF86XK_AudioMute
    };
    unsigned mods[4];
    unsigned i, k;
    KeyCode kc;

    if (!dpy || !vol_grabbed)
        return;
    root = DefaultRootWindow(dpy);
    mods[0] = 0;
    mods[1] = LockMask;
    mods[2] = Mod2Mask;
    mods[3] = LockMask | Mod2Mask;
    for (k = 0; k < sizeof(syms) / sizeof(syms[0]); k++) {
        kc = XKeysymToKeycode(dpy, syms[k]);
        if (!kc)
            continue;
        for (i = 0; i < 4; i++)
            XUngrabKey(dpy, kc, mods[i], root);
    }
    vol_grabbed = 0;
}

static void grab_vol_keys(void)
{
    Window root;
    KeySym syms[] = {
        XK_9, XK_KP_9, XK_0, XK_KP_0,
        XK_minus, XK_underscore, XK_KP_Subtract,
        XK_plus, XK_equal, XK_KP_Add,
        XK_m, XK_M, XK_space,
        XF86XK_AudioLowerVolume, XF86XK_AudioRaiseVolume, XF86XK_AudioMute
    };
    unsigned mods[4];
    unsigned i, k;
    KeyCode kc;

    if (!dpy || chooser_open)
        return;
    if (vol_grabbed)
        return;
    root = DefaultRootWindow(dpy);
    mods[0] = 0;
    mods[1] = LockMask;
    mods[2] = Mod2Mask;
    mods[3] = LockMask | Mod2Mask;
    for (k = 0; k < sizeof(syms) / sizeof(syms[0]); k++) {
        kc = XKeysymToKeycode(dpy, syms[k]);
        if (!kc)
            continue;
        for (i = 0; i < 4; i++)
            XGrabKey(dpy, kc, mods[i], root, False, GrabModeAsync, GrabModeAsync);
    }
    vol_grabbed = 1;
}

static void ungrab_keys(void)
{
    ungrab_vol_keys();
}

static void grab_keys(void)
{
    if (tt_end_hold)
        return;
    if (!dpy || chooser_open)
        return;
    grab_vol_keys();
    /* Do not focus mplayer or restack the picture. XSetInputFocus on
     * mpwin blacks -vo x11. Volume keys arrive via the root grab. */
    if (have_playback && mpid > 0)
        return;
    /* The end-of-list choice is on screen. Leave the keyboard there. */
    if (tt_endw && tt_endw->shown())
        return;
    if (barw && fl_xid(barw))
        XSetInputFocus(dpy, fl_xid(barw), RevertToParent, CurrentTime);
}

/* Re-grabbing every tick kept the one core busy while a clip was paused. */
static void tt_hold_player_keys(unsigned long now)
{
    /* More videos has the screen. Raising the player here flashed it
       through the search list on every tick. */
    if (tt_end_hold)
        return;
    if (!have_playback || mpid <= 0)
        return;
    if (now - tt_key_hold > 5000UL) {
        vol_grabbed = 0;
        tt_key_hold = now;
    }
    grab_keys();
    if (paused)
        return;
    if (titlew && titlew->shown() && fl_xid(titlew))
        XRaiseWindow(dpy, fl_xid(titlew));
    if (!bar_hidden && barw && fl_xid(barw))
        XRaiseWindow(dpy, fl_xid(barw));
}

static void tt_note_clip_title(void)
{
    char clip[160];

    if (!titlew || !titlew->shown())
        return;
    tt_read_clip_name(clip, (int)sizeof clip);
    if (strcmp(clip, tt_clip_shown) == 0)
        return;
    snprintf(tt_clip_shown, sizeof tt_clip_shown, "%s", clip);
    titlew->redraw();
}

static int tt_pid_is_ours(int pid)
{
    FILE *f;
    char path[64], comm[32], raw[512];
    int n, i, ppid = 0;

    if (pid <= 1)
        return 0;
    snprintf(path, sizeof path, "/proc/%d/comm", pid);
    f = fopen(path, "r");
    if (!f)
        return 0;
    if (!fgets(comm, sizeof comm, f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    if (strncmp(comm, "minios-tt-", 10) == 0)
        return 1;
    snprintf(path, sizeof path, "/proc/%d/cmdline", pid);
    f = fopen(path, "rb");
    if (!f)
        return 0;
    n = (int)fread(raw, 1, sizeof raw - 1, f);
    fclose(f);
    if (n < 1)
        return 0;
    for (i = 0; i < n; i++) {
        if (raw[i] == 0)
            raw[i] = ' ';
    }
    raw[n] = 0;
    if (strstr(raw, "minios-tt") != NULL)
        return 1;
    if (strncmp(comm, "python", 6) != 0)
        return 0;
    snprintf(path, sizeof path, "/proc/%d/status", pid);
    f = fopen(path, "r");
    if (!f)
        return 0;
    while (fgets(raw, sizeof raw, f)) {
        if (sscanf(raw, "PPid: %d", &ppid) == 1)
            break;
    }
    fclose(f);
    if (ppid <= 1)
        return 0;
    snprintf(path, sizeof path, "/proc/%d/comm", ppid);
    f = fopen(path, "r");
    if (!f)
        return 0;
    if (!fgets(comm, sizeof comm, f))
        comm[0] = 0;
    fclose(f);
    return strncmp(comm, "minios-tiktok-p", 15) == 0;
}

static void tt_kill_pidfile(const char *path)
{
    FILE *f;
    int pid = 0;

    f = fopen(path, "r");
    if (!f)
        return;
    if (fscanf(f, "%d", &pid) != 1)
        pid = 0;
    fclose(f);
    if (tt_pid_is_ours(pid))
        kill(pid, SIGKILL);
    unlink(path);
}

static void tt_stop_saved_work(void)
{
    tt_kill_pidfile("/tmp/minios-tt-mp.pid");
    tt_kill_pidfile("/tmp/minios-tt-sync.pid");
    tt_kill_pidfile("/tmp/minios-tt-dl.pid");
    tt_kill_pidfile("/tmp/minios-tt-now.pid");
    tt_kill_pidfile("/tmp/minios-tt-guard.pid");
}

static void listen_video_clicks(Window w)
{
    /* Remember the picture window only. Do not select ButtonPress on it:
     * a click on mplayer -vo x11 blanks the picture and drops the navy sides. */
    if (!dpy || w == None)
        return;
    if (barw && w == fl_xid(barw))
        return;
    if (loadw && w == fl_xid(loadw))
        return;
    mpwin = w;
}

static void place_shield(void)
{
    int vx, vy, vw, vh;
    XSetWindowAttributes swa;

    if (!dpy)
        return;
    video_rect(&vx, &vy, &vw, &vh);
    if (vw < 8 || vh < 8)
        return;
    if (!shield) {
        swa.event_mask = ButtonPressMask;
        swa.override_redirect = True;
        shield = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                               vx, vy, (unsigned)vw, (unsigned)vh, 0, 0,
                               InputOnly, CopyFromParent,
                               CWEventMask | CWOverrideRedirect, &swa);
    }
    XMoveResizeWindow(dpy, shield, vx, vy, (unsigned)vw, (unsigned)vh);
    XMapWindow(dpy, shield);
}

static void pin_stage(void)
{
    /* Do not XConfigureWindow against mpwin. That restacks mplayer and
     * blanks -vo x11 (black picture, audio still playing). */
    if (!dpy)
        return;
    if (have_playback && mpid > 0) {
        tt_show_stage();
        place_shield();
        if (titlew && titlew->shown() && fl_xid(titlew))
            XRaiseWindow(dpy, fl_xid(titlew));
        if (!bar_hidden && barw && fl_xid(barw))
            XRaiseWindow(dpy, fl_xid(barw));
        XFlush(dpy);
        return;
    }
    tt_show_stage();
    place_shield();
    if (titlew && titlew->shown() && fl_xid(titlew))
        XRaiseWindow(dpy, fl_xid(titlew));
    if (!bar_hidden && barw && fl_xid(barw))
        XRaiseWindow(dpy, fl_xid(barw));
    raise_resmon();
    XFlush(dpy);
}

static void drop_pl_item(int idx)
{
    int i;

    if (idx < 0 || idx >= pl_count)
        return;
    for (i = idx; i < pl_count - 1; i++) {
        memcpy(pl_urls[i], pl_urls[i + 1], sizeof(pl_urls[0]));
        memcpy(pl_titles[i], pl_titles[i + 1], sizeof(pl_titles[0]));
        tt_row_started[i] = tt_row_started[i + 1];
    }
    if (pl_count > 0)
        tt_row_started[pl_count - 1] = 0;
    pl_count--;
    if (pl_count < 0)
        pl_count = 0;
    if (pl_cur >= pl_count)
        pl_cur = pl_count - 1;
    queue_on = (pl_count > 1);
    save_pl_index();
}

static void request_quit(void)
{
    FILE *f;

    tt_note_watched();
    tt_hide_end_popup();
    user_quit = 1;
    skip_busy = 0;
    autoplay = 0;
    ungrab_keys();
    if (loadw)
        loadw->hide();
    if (titlew)
        titlew->hide();
    if (barw)
        barw->hide();
    hide_playlist();
    hide_add_list();
    tt_show_search_after_play();
    unlink(BARPID);
    f = fopen(QUITF, "w");
    if (f) {
        fputs("1\n", f);
        fclose(f);
    }
    dismiss_popup();
    bump_gen();
    signal_mplayers(SIGCONT);
    send_cmd("quit\n");
    signal_mplayers(SIGTERM);
    tt_stop_saved_work();
    stop_download();
    load_failed = 1;
    pl_count = 0;
    pl_cur = 0;
    queue_on = 0;
    shorts_on = 0;
    unlink(PLF);
    unlink(SHORTF);
    unlink(TIKTOKF);
    unlink(MODEFILE);
    unlink(GEOMF);
    if (fork() == 0) {
        execl("/opt/minios/minios-after-close", "minios-after-close", "player",
              (char *)0);
        execl("/usr/local/bin/minios-after-close", "minios-after-close", "player",
              (char *)0);
        _exit(0);
    }
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

static int send_cmd(const char *s)
{
    int fd, n, left;
    const char *p;

    if (!s || !s[0])
        return 0;
    /* Never block the UI. A fifo with no mplayer reader used to freeze
     * Play/Pause after an MP3 jumped into a YouTube download. */
    fd = open(CMD, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
        return 0;
    p = s;
    left = (int)strlen(s);
    while (left > 0) {
        n = (int)write(fd, p, (size_t)left);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            close(fd);
            return 0;
        }
        p += n;
        left -= n;
    }
    close(fd);
    return 1;
}

static void read_answers(void)
{
    FILE *f;
    char line[192];

    /* Ignore leftover answers from the previous clip. */
    if (skip_busy || mpid <= 0)
        return;
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
            if (tpos > max_tpos)
                max_tpos = tpos;
            have_playback = 1;
            if (max_tpos >= 2)
                really_played = 1;
        } else if (strncmp(line, "ANS_LENGTH=", 11) == 0) {
            tlen = atoi(line + 11);
            if (tlen < 0)
                tlen = 0;
        } else if (strstr(line, "Starting playback") != NULL ||
                   strstr(line, "VO: [x11]") != NULL) {
            have_playback = 1;
            saw_start = 1;
        }
    }
    fclose(f);
}

static int take_clip_done(void)
{
    FILE *f;
    int g = -1;

    if (!file_exists(DONEF))
        return 0;
    f = fopen(DONEF, "r");
    if (!f)
        return 0;
    if (fscanf(f, "%d", &g) != 1)
        g = -1;
    fclose(f);
    unlink(DONEF);
    /* Only this clip's mplayer may end the item. */
    if (g != play_gen)
        return 0;
    return 1;
}

static void finish_current_clip(void)
{
    if (user_quit || file_exists(QUITF) || skip_busy || clip_done)
        return;
    clip_done = 1;
    mpid = 0;
    have_playback = 0;
    fprintf(stderr, "player-v21 clip ended idx=%d gen=%d played=%d tpos=%d tlen=%d\n",
            pl_cur, play_gen, really_played, max_tpos, tlen);
    /* Loop this file even if time_pos never reached 2s (short MP3s). */
    if (loop_mode == 1) {
        if (mpid > 0 && kill(mpid, 0) == 0) {
            send_cmd("seek 0 2\n");
            tt_drop_bar_clock();
            clip_done = 0;
            have_playback = 1;
            paused = 0;
            return;
        }
        if (local_path[0])
            start_local_file(local_path);
        else if (pl_count > 0)
            start_pl_item(pl_cur);
        return;
    }
    if (!really_played && !saw_start) {
        snprintf(dl_lab, sizeof(dl_lab), "Could not play this clip");
        dl_pct = 100;
        last_ui_dl = -1;
        if (loadw)
            loadw->redraw();
        if (pl_count <= 0 || pl_cur + 1 >= pl_count) {
            fprintf(stderr, "player-v244 last\n");
            tt_offer_next_list();
        }
        return;
    }
    if (queue_on && (autoplay || loop_mode == 2)) {
        int next = pl_cur + 1;
        int from_local = is_local_url(pl_urls[pl_cur]);

        if (from_local || local_mode) {
            while (next < pl_count && !is_local_url(pl_urls[next]))
                next++;
            if (next >= pl_count && loop_mode == 2) {
                next = 0;
                while (next < pl_count && !is_local_url(pl_urls[next]))
                    next++;
            }
            if (next >= pl_count || !is_local_url(pl_urls[next])) {
                snprintf(dl_lab, sizeof(dl_lab), "End of list");
                dl_pct = 100;
                last_ui_dl = -1;
                if (loadw)
                    loadw->redraw();
                tt_offer_next_list();
                return;
            }
            start_pl_item(next);
            return;
        }
        if (next >= pl_count && loop_mode != 2) {
            snprintf(dl_lab, sizeof(dl_lab), "End of list");
            dl_pct = 100;
            last_ui_dl = -1;
            if (loadw)
                loadw->redraw();
            tt_offer_next_list();
            return;
        }
        start_pl_item(next);
        return;
    }
    snprintf(dl_lab, sizeof(dl_lab), "Clip finished");
    dl_pct = 100;
    last_ui_dl = -1;
    if (loadw)
        loadw->redraw();
    tt_offer_if_last();
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
                tt_begin_bar_clip();
                fprintf(stderr, "player-v17 wrapper_pid=%d\n", (int)mpid);
                if (!play_alive_since)
                    play_alive_since = now_ms();
                if (file_exists("/tmp/minios-ttplay.mp4"))
                    snprintf(local_path, sizeof(local_path),
                             "/tmp/minios-ttplay.mp4");
                if (pl_count > 0)
                    cache_store(pl_urls[pl_cur]);
                dismiss_popup();
                blank_load();
                restack_play();
                grab_keys();
                tt_note_play_started();
                tt_commit_volume();
                refresh_ui();
            }
        }
    }
    if (file_exists(FAILF))
        load_failed = 1;
    if (!have_sub) {
        if (file_exists(SUBF) ||
            file_exists("/tmp/minios-ttplay.en.srt") ||
            file_exists("/tmp/minios-ttplay.en.vtt") ||
            file_exists("/tmp/minios-ttplay.srt")) {
            have_sub = 1;
            if (cc_fetching) {
                cc_on = 1;
                cc_user = 1;
                cc_fetching = 0;
                send_cmd("pausing_keep_force sub_load /tmp/minios-ttplay.en.srt\n");
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
    unlink("/tmp/minios-ttplay.en.srt");
    unlink("/tmp/minios-ttplay.en.vtt");
    unlink("/tmp/minios-ttplay.srt");
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

static void load_local_playlist(void)
{
    shorts_on = 0;
    pl_count = 0;
    pl_cur = 0;
    queue_on = 0;
    autoplay = 0;
    /* Dock is a new session. Never import the last YouTube search. */
    fprintf(stderr, "player-v37 local playlist empty\n");
}

static void keep_only_local_items(void)
{
    int i, j = 0;

    for (i = 0; i < pl_count; i++) {
        if (!is_local_url(pl_urls[i]))
            continue;
        if (j != i) {
            memcpy(pl_urls[j], pl_urls[i], sizeof(pl_urls[0]));
            memcpy(pl_titles[j], pl_titles[i], sizeof(pl_titles[0]));
        }
        j++;
    }
    pl_count = j;
    if (pl_cur >= pl_count)
        pl_cur = pl_count - 1;
    if (pl_cur < 0)
        pl_cur = 0;
    local_mode = 1;
    shorts_on = 0;
    queue_on = (pl_count > 1);
    write_player_mode();
}

static void save_local_playlist(void)
{
    FILE *f;
    int i, n = 0;

    f = fopen(LOCALPL, "w");
    if (!f) {
        snprintf(dl_lab, sizeof(dl_lab), "Could not save playlist");
        dl_pct = 100;
        last_ui_dl = -1;
        return;
    }
    fprintf(f, "0\n");
    for (i = 0; i < pl_count; i++) {
        if (!is_local_url(pl_urls[i]))
            continue;
        fprintf(f, "%s\t%s\n", pl_urls[i],
                pl_titles[i][0] ? pl_titles[i] : pl_urls[i]);
        n++;
    }
    fclose(f);
    snprintf(dl_lab, sizeof(dl_lab), n ? "Playlist saved" : "Nothing to save");
    dl_pct = 100;
    last_ui_dl = -1;
    fprintf(stderr, "player-v36 saved local n=%d\n", n);
}

static void clear_playlist(void)
{
    pl_count = 0;
    pl_cur = 0;
    queue_on = 0;
    if (local_mode)
        unlink(LOCALPL);
    hide_playlist();
    snprintf(dl_lab, sizeof(dl_lab), "Playlist cleared");
    dl_pct = 100;
    last_ui_dl = -1;
}

static void clear_youtube_media(void)
{
    unlink("/tmp/minios-ttplay.mp4");
    unlink("/tmp/minios-ttplay.audio");
    unlink("/tmp/minios-ttplay.a.m4a");
    unlink("/tmp/minios-ttplay.a.aac");
    unlink("/tmp/minios-ttplay.sub");
    unlink("/tmp/minios-ttplay.en.srt");
    unlink("/tmp/minios-ttplay.en.vtt");
    unlink("/tmp/minios-ttplay.srt");
    unlink(SHORTF);
    unlink(TIKTOKF);
    rmdir("/tmp/minios-ttplay.lock");
    (void)system("pkill -9 -f /tmp/tcloop/mplayer-cli/usr/local/bin/mplayer >/dev/null 2>&1");
    (void)system("pkill -9 -x mplayer >/dev/null 2>&1");
}

static void apply_loop_to_player(void)
{
    /* mplayer: -1 = no loop, 0 = forever, N = play N times.
     * Off / One / All are handled in finish_current_clip. */
    send_cmd("osd 0\n");
    send_cmd("loop -1\n");
    send_cmd("osd 1\n");
}

static void load_playlist(void)
{
    FILE *f;
    char line[400];

    if (local_mode) {
        load_local_playlist();
        return;
    }
    shorts_on = file_exists(SHORTF) || file_exists(TIKTOKF);
    pl_count = 0;
    pl_cur = 0;
    memset(tt_row_started, 0, sizeof tt_row_started);
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
        char *tab;
        while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r'))
            line[--L] = 0;
        if (!L)
            continue;
        tab = strchr(line, '\t');
        if (tab) {
            char *tab2;
            int secs = -1;
            *tab = 0;
            tab2 = strchr(tab + 1, '\t');
            if (tab2) {
                *tab2 = 0;
                secs = atoi(tab2 + 1);
            }
            if (shorts_on && !file_exists(TIKTOKF) && secs > 180)
                continue;
            snprintf(pl_urls[pl_count], sizeof(pl_urls[0]), "%s", line);
            snprintf(pl_titles[pl_count], sizeof(pl_titles[0]), "%s", tab + 1);
        } else {
            const char *id = strrchr(line, '/');
            snprintf(pl_urls[pl_count], sizeof(pl_urls[0]), "%s", line);
            snprintf(pl_titles[pl_count], sizeof(pl_titles[0]), "%s",
                     id ? id + 1 : line);
        }
        pl_count++;
    }
    fclose(f);
    if (pl_cur < 0)
        pl_cur = 0;
    if (pl_count > 0 && pl_cur >= pl_count)
        pl_cur = 0;
    queue_on = (pl_count > 1);
    if (pl_count > 0)
        autoplay = 1;
    fprintf(stderr, "player-v37 youtube playlist n=%d cur=%d shorts=%d\n",
            pl_count, pl_cur, shorts_on);
}

static void clip_id_from_url(const char *u, char *id, size_t n)
{
    const char *v, *p, *q;
    size_t i;

    if (!u || !id || n < 12) {
        if (id && n)
            id[0] = 0;
        return;
    }
    v = strstr(u, "v=");
    if (v) {
        v += 2;
        for (i = 0; i < 11 && v[i] && v[i] != '&'; i++)
            id[i] = v[i];
        id[i] = 0;
        return;
    }
    p = strrchr(u, '/');
    p = p ? p + 1 : u;
    q = strchr(p, '?');
    i = 0;
    while (p[i] && p + i != q && i < n - 1 && i < 20) {
        id[i] = p[i];
        i++;
    }
    id[i] = 0;
}

static const char *cache_root(void)
{
    FILE *df;
    char line[160], mp[64];

    if (access("/mnt/internal", W_OK) == 0)
        return "/mnt/internal/minios-vcache";
    df = fopen("/proc/mounts", "r");
    if (!df)
        return NULL;
    while (fgets(line, sizeof(line), df)) {
        if (sscanf(line, "%*s %63s", mp) != 1)
            continue;
        if (strncmp(mp, "/mnt/", 5) != 0)
            continue;
        {
            char probe[128];
            snprintf(probe, sizeof(probe), "%s/cde/optional", mp);
            if (file_exists(probe)) {
                static char usb[96];
                snprintf(usb, sizeof(usb), "%s/minios-vcache", mp);
                fclose(df);
                return usb;
            }
        }
    }
    fclose(df);
    return NULL;
}

static void cache_store(const char *url)
{
    const char *root;
    char id[24], dest[192], cmd[400], idxp[192], ids[2][24];
    FILE *f;

    if (!url || !url[0] || !file_exists("/tmp/minios-ttplay.mp4"))
        return;
    clip_id_from_url(url, id, sizeof(id));
    if (!id[0])
        return;
    root = cache_root();
    if (!root)
        return;
    mkdir(root, 0755);
    snprintf(dest, sizeof(dest), "%s/%s.mp4", root, id);
    snprintf(cmd, sizeof(cmd), "cp -f /tmp/minios-ttplay.mp4 '%s'", dest);
    (void)system(cmd);
    if (file_exists("/tmp/minios-ttplay.a.m4a")) {
        snprintf(dest, sizeof(dest), "%s/%s.m4a", root, id);
        snprintf(cmd, sizeof(cmd), "cp -f /tmp/minios-ttplay.a.m4a '%s'", dest);
        (void)system(cmd);
    }
    ids[0][0] = ids[1][0] = 0;
    snprintf(idxp, sizeof(idxp), "%s/last.txt", root);
    f = fopen(idxp, "r");
    if (f) {
        if (fgets(ids[0], sizeof(ids[0]), f)) {
            size_t L = strlen(ids[0]);
            while (L && (ids[0][L - 1] == '\n' || ids[0][L - 1] == '\r'))
                ids[0][--L] = 0;
        }
        if (fgets(ids[1], sizeof(ids[1]), f)) {
            size_t L = strlen(ids[1]);
            while (L && (ids[1][L - 1] == '\n' || ids[1][L - 1] == '\r'))
                ids[1][--L] = 0;
        }
        fclose(f);
    }
    if (strcmp(ids[0], id) != 0) {
        if (ids[1][0] && strcmp(ids[1], id) != 0) {
            snprintf(dest, sizeof(dest), "%s/%s.mp4", root, ids[1]);
            unlink(dest);
            snprintf(dest, sizeof(dest), "%s/%s.m4a", root, ids[1]);
            unlink(dest);
        }
        snprintf(ids[1], sizeof(ids[1]), "%s", ids[0]);
        snprintf(ids[0], sizeof(ids[0]), "%s", id);
    }
    f = fopen(idxp, "w");
    if (f) {
        fprintf(f, "%s\n", ids[0]);
        if (ids[1][0])
            fprintf(f, "%s\n", ids[1]);
        fclose(f);
    }
    fprintf(stderr, "player-v24 cache store %s\n", id);
}

static int cache_play(const char *url)
{
    const char *root;
    char id[24], dest[192], aud[192], cmd[400];

    if (!url)
        return 0;
    clip_id_from_url(url, id, sizeof(id));
    if (!id[0])
        return 0;
    root = cache_root();
    if (!root)
        return 0;
    snprintf(dest, sizeof(dest), "%s/%s.mp4", root, id);
    if (!file_exists(dest))
        return 0;
    snprintf(aud, sizeof(aud), "%s/%s.m4a", root, id);
    if (file_exists(aud)) {
        snprintf(cmd, sizeof(cmd), "cp -f '%s' /tmp/minios-ttplay.a.m4a", aud);
        (void)system(cmd);
        {
            FILE *af = fopen("/tmp/minios-ttplay.audio", "w");
            if (af) {
                fputs("/tmp/minios-ttplay.a.m4a\n", af);
                fclose(af);
            }
        }
    }
    snprintf(cmd, sizeof(cmd), "cp -f '%s' /tmp/minios-ttplay.mp4", dest);
    (void)system(cmd);
    fprintf(stderr, "player-v24 cache hit %s\n", dest);
    start_local_file("/tmp/minios-ttplay.mp4");
    return 1;
}

static void save_pl_index(void)
{
    FILE *f;
    int i;

    /* Dock / local list must never overwrite the YouTube search file. */
    if (local_mode)
        return;
    f = fopen(PLF, "w");
    if (!f)
        return;
    fprintf(f, "%d\n", pl_cur);
    for (i = 0; i < pl_count; i++)
        fprintf(f, "%s\t%s\n", pl_urls[i], pl_titles[i][0] ? pl_titles[i] : pl_urls[i]);
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
    sf = fopen("/tmp/minios-tt-seen", "a");
    if (!sf)
        return;
    fprintf(sf, "%s|%s\n", id ? id + 1 : u, u);
    fclose(sf);
}

static int tt_url_id(const char *u, char *id, size_t n)
{
    const char *p;
    size_t i = 0;

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

/* Clips that actually started. mark_seen skips tiktok.com, so this file is separate. */
static void tt_note_watched(void)
{
    FILE *f;
    char line[200];
    char id[32], have[32];
    const char *u;
    size_t L;

    if (pl_count <= 0 || pl_cur < 0 || pl_cur >= pl_count)
        return;
    if (!saw_start && !really_played)
        return;
    u = pl_urls[pl_cur];
    if (!u[0] || !strstr(u, "tiktok.com"))
        return;
    if (!tt_url_id(u, id, sizeof id))
        return;
    tt_row_started[pl_cur] = 1;
    f = fopen(TTWATCH, "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            L = strlen(line);
            while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r'))
                line[--L] = 0;
            if (tt_url_id(line, have, sizeof have) && strcmp(have, id) == 0) {
                fclose(f);
                return;
            }
        }
        fclose(f);
    }
    f = fopen(TTWATCH, "a");
    if (!f)
        return;
    fprintf(f, "%s\n", u);
    fclose(f);
    fprintf(stderr, "player-v233 watched %s\n", id);
}

static void tt_hide_end_popup(void)
{
    if (tt_endw && tt_endw->shown())
        tt_endw->hide();
}

static int tt_viewable(Window w)
{
    XWindowAttributes wa;

    memset(&wa, 0, sizeof wa);
    if (!w || !XGetWindowAttributes(dpy, w, &wa))
        return 0;
    return wa.map_state == IsViewable;
}

static Window tt_top_frame(Window w)
{
    Window root, parent, *kids = NULL;
    unsigned n = 0;
    Window cur = w;
    Window screen_root;
    int guard = 0;

    screen_root = RootWindow(dpy, DefaultScreen(dpy));
    while (cur && cur != screen_root && guard < 6) {
        kids = NULL;
        n = 0;
        if (!XQueryTree(dpy, cur, &root, &parent, &kids, &n))
            break;
        if (kids)
            XFree(kids);
        if (parent == screen_root || parent == 0)
            return cur;
        cur = parent;
        guard++;
    }
    return w;
}

static void tt_raise_window(Window w)
{
    Window top;

    if (!dpy || !w)
        return;
    top = tt_top_frame(w);
    XMapRaised(dpy, top);
    if (top != w)
        XRaiseWindow(dpy, w);
}

/* The search window sleeps until an X event arrives. A file by itself is not seen.
   The name match is exact so the player window is not the one that wakes. */
static Window tt_find_search(Window w, int depth)
{
    Window root, parent, *kids = NULL;
    unsigned n = 0, i;
    Window found = 0;
    char *name = NULL;

    if (!dpy || !w || depth > 4)
        return 0;
    if (XFetchName(dpy, w, &name) && name) {
        if (strcmp(name, "MiniOS TikTok") == 0)
            found = w;
        XFree(name);
        if (found)
            return found;
    }
    if (!XQueryTree(dpy, w, &root, &parent, &kids, &n))
        return 0;
    for (i = 0; i < n && !found; i++)
        found = tt_find_search(kids[i], depth + 1);
    if (kids)
        XFree(kids);
    return found;
}

static void tt_unmap_search(void)
{
    Window w;

    if (!dpy || tt_end_hold)
        return;
    w = tt_find_search(RootWindow(dpy, DefaultScreen(dpy)), 0);
    if (!w)
        return;
    XUnmapWindow(dpy, w);
    XFlush(dpy);
    fprintf(stderr, "player-v234 unmap\n");
}

static void tt_show_search_after_play(void)
{
    Window w;

    if (!dpy)
        return;
    w = tt_find_search(RootWindow(dpy, DefaultScreen(dpy)), 0);
    if (!w)
        return;
    XMapRaised(dpy, w);
    XFlush(dpy);
}

static int tt_wake_search(void)
{
    Window w;
    XEvent ev;
    Atom msg;

    if (!dpy)
        return 0;
    w = tt_find_search(RootWindow(dpy, DefaultScreen(dpy)), 0);
    if (!w)
        return 0;
    tt_raise_window(w);
    if (tt_viewable(w))
        XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
    memset(&ev, 0, sizeof ev);
    msg = XInternAtom(dpy, "MINIOS_TT_ASK", False);
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = msg;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1;
    XSendEvent(dpy, w, False, 0, &ev);
    XSync(dpy, False);
    return 1;
}

static void tt_write_ask(const char *word)
{
    FILE *f;

    f = fopen(TTASK, "w");
    if (!f)
        return;
    fprintf(f, "%s\n", word);
    fclose(f);
    f = fopen(TTASKPID, "w");
    if (!f)
        return;
    fprintf(f, "%d\n", (int)getpid());
    fclose(f);
}

static void tt_clear_ask(void)
{
    unlink(TTASK);
    unlink(TTASKPID);
}

static void tt_choose_more(Fl_Widget *, void *)
{
    fprintf(stderr, "player-v233 more\n");
    tt_note_watched();
    tt_write_ask("more");
    tt_end_hold = 1;
    tt_hold_list_end();
    ungrab_keys();
    if (!tt_wake_search()) {
        tt_clear_ask();
        tt_end_hold = 0;
        fprintf(stderr, "player-v233 more missed\n");
        if (loadw)
            loadw->show();
        if (titlew && !win_min)
            titlew->show();
        if (barw)
            barw->show();
        return;
    }
    tt_hide_end_popup();
}

static void tt_choose_fresh(Fl_Widget *, void *)
{
    int woke;

    fprintf(stderr, "player-v233 fresh\n");
    tt_write_ask("fresh");
    tt_hide_end_popup();
    woke = tt_wake_search();
    request_quit();
    if (woke)
        tt_wake_search();
    else
        tt_clear_ask();
}

static void tt_show_end_popup(void)
{
    int w = 460, h = 168, x, y;
    Fl_Box *lab;
    Fl_Button *more, *fresh;

    if (tt_end_asked || user_quit || !file_exists(TIKTOKF))
        return;
    tt_end_asked = 1;
    tt_note_watched();
    fprintf(stderr, "player-v233 list-end\n");
    if (dw > 40 && w > dw - 24)
        w = dw - 24;
    if (w < 300)
        w = 300;
    x = (dw - w) / 2;
    y = (dh - h) / 2;
    if (x < 8)
        x = 8;
    if (y < 8)
        y = 8;
    if (!tt_endw) {
        tt_endw = new Fl_Window(x, y, w, h, "List finished");
        tt_endw->color(C_NAVY);
        tt_endw->begin();
        lab = new Fl_Box(12, 10, w - 24, 78,
            "This list is finished.\nMore videos skips what you already watched.\nNew search closes the player and clears the box.");
        lab->box(FL_NO_BOX);
        lab->labelsize(14);
        lab->labelcolor(C_TEXT);
        lab->align(FL_ALIGN_WRAP | FL_ALIGN_INSIDE | FL_ALIGN_CENTER);
        more = new Fl_Button(16, 108, (w / 2) - 24, 40, "More videos");
        more->callback(tt_choose_more);
        fresh = new Fl_Button((w / 2) + 8, 108, (w / 2) - 24, 40, "New search");
        fresh->callback(tt_choose_fresh);
        tt_endw->end();
    }
    tt_endw->show();
    Fl::flush();
    tt_keep_end_popup();
}

static int tt_every_row_watched(void)
{
    int i;

    if (pl_count <= 0)
        return 0;
    for (i = 0; i < pl_count; i++) {
        if (!strstr(pl_urls[i], "tiktok.com"))
            continue;
        if (!tt_row_started[i])
            return 0;
    }
    return 1;
}

static void tt_offer_next_list(void)
{
    if (loop_mode == 1)
        return;
    tt_show_end_popup();
}

static void tt_offer_if_last(void)
{
    int at_end;

    tt_note_watched();
    at_end = (pl_count <= 0) || (pl_cur + 1 >= pl_count);
    if (!at_end && !tt_every_row_watched())
        return;
    tt_offer_next_list();
}

static void tt_resume_after_more(void)
{
    tt_end_hold = 0;
    tt_end_asked = 0;
    tt_hide_end_popup();
}

/* More videos hides the navy window. The next clip has to put it back,
   or the desktop shows through and the download words are invisible. */
static void tt_show_stage(void)
{
    static int tt_stage_logged;

    if (tt_end_hold || win_min || !loadw)
        return;
    if (!loadw->shown()) {
        loadw->show();
        loadw->redraw();
        if (!tt_stage_logged) {
            tt_stage_logged = 1;
            fprintf(stderr, "player-v240 stage\n");
        }
    }
    if (dpy && fl_xid(loadw))
        XMapWindow(dpy, fl_xid(loadw));
    if (have_playback && mpid > 0 && dpy && mpwin)
        XRaiseWindow(dpy, mpwin);
    if (titlew && !titlew->shown())
        titlew->show();
    if (barw && !bar_hidden && !barw->shown())
        barw->show();
}

static void tt_hold_list_end(void)
{
    if (!tt_end_hold)
        return;
    if (loadw && loadw->shown())
        loadw->hide();
    if (titlew && titlew->shown())
        titlew->hide();
    if (barw && barw->shown())
        barw->hide();
    hide_playlist();
    hide_add_list();
    if (dpy && shield)
        XUnmapWindow(dpy, shield);
    if (dpy && mpwin)
        XUnmapWindow(dpy, mpwin);
}

/* Hiding the stage on every timer tick is the blink. Hide once per hold. */
static void tt_hold_list_quiet(void)
{
    static int tt_held_once;

    if (!tt_end_hold) {
        tt_held_once = 0;
        return;
    }
    if (tt_held_once)
        return;
    tt_held_once = 1;
    tt_hold_list_end();
}

static void tt_keep_end_popup(void)
{
    if (tt_end_hold || !tt_endw || !tt_endw->shown() || !dpy)
        return;
    /* The click shield sits above ordinary windows, so it would swallow the choice. */
    if (shield)
        XUnmapWindow(dpy, shield);
    if (mpwin)
        XUnmapWindow(dpy, mpwin);
    if (fl_xid(tt_endw))
        tt_raise_window(fl_xid(tt_endw));
}

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

static int resmon_seen;
static int resmon_tried;

static void raise_resmon(void)
{
    Window root, parent, *kids = NULL;
    unsigned n = 0, i;
    char *name = NULL;

    if (!dpy)
        return;
    if (!XQueryTree(dpy, RootWindow(dpy, DefaultScreen(dpy)), &root, &parent, &kids, &n))
        return;
    for (i = 0; i < n; i++) {
        name = NULL;
        if (XFetchName(dpy, kids[i], &name) && name) {
            if (strstr(name, "MiniOS resources")) {
                resmon_seen = 1;
                XRaiseWindow(dpy, kids[i]);
            }
            XFree(name);
        }
    }
    if (kids)
        XFree(kids);
    if (!resmon_seen && !resmon_tried) {
        resmon_tried = 1;
        if (fork() == 0) {
            execl("/opt/minios/minios-resmon", "minios-resmon", (char *)0);
            execl("/usr/local/bin/minios-resmon", "minios-resmon", (char *)0);
            _exit(127);
        }
    }
}

static void restack_play(void)
{
    Window root, parent, *kids = NULL;
    if (tt_end_hold)
        return;
    unsigned n = 0, i;
    int found = 0;

    if (!dpy)
        return;
    if (XQueryTree(dpy, RootWindow(dpy, DefaultScreen(dpy)), &root, &parent, &kids, &n)) {
        for (i = 0; i < n; i++) {
            if (loadw && kids[i] == fl_xid(loadw))
                continue;
            if (barw && kids[i] == fl_xid(barw))
                continue;
            if (is_mplayer_window(kids[i])) {
                found = 1;
                listen_video_clicks(kids[i]);
                break;
            }
        }
        if (found) {
            for (i = 0; i < n; i++) {
                if (is_mplayer_window(kids[i]))
                    listen_video_clicks(kids[i]);
            }
        }
        if (kids)
            XFree(kids);
    }
    pin_stage();
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

    video_rect(&vx, &vy, &vw, &vh);
    gf = fopen(GEOMF, "w");
    if (gf) {
        fprintf(gf, "%dx%d+%d+%d\n", vw, vh, vx, vy);
        fclose(gf);
    }
    fprintf(stderr, "player-v26 geom=%dx%d+%d+%d shorts=%d card=%dx%d+%d+%d\n",
            vw, vh, vx, vy, shorts_on, card_w, card_h, card_x, card_y);
}

static void apply_card(int move_video)
{
    int vx, vy, vw, vh;

    if (card_w < 80)
        card_defaults();
    if (loadw) {
        /* Fullscreen navy stays under the picture. Never shrink it. */
        loadw->resize(0, 0, dw, dh);
        if (dpy && fl_xid(loadw))
            XMoveResizeWindow(dpy, fl_xid(loadw), 0, 0, dw, dh);
        if (!(have_playback && mpid > 0))
            loadw->redraw();
    }
    if (titlew) {
        titlew->resize(0, 0, dw, TITLE_H);
        if (dpy && fl_xid(titlew))
            XMoveResizeWindow(dpy, fl_xid(titlew), 0, 0, dw, TITLE_H);
        if (btn_min)
            ((Fl_Widget *)btn_min)->resize(dw - 132, 3, 32, 30);
        if (btn_max)
            ((Fl_Widget *)btn_max)->resize(dw - 96, 3, 32, 30);
        if (btn_close)
            ((Fl_Widget *)btn_close)->resize(dw - 58, 3, 50, 30);
        if (!titlew->shown() && !win_min)
            titlew->show();
        titlew->redraw();
    }
    write_geom();
    if (win_min) {
        if (barw && barw->shown())
            barw->hide();
        if (move_video && dpy && mpwin)
            XUnmapWindow(dpy, mpwin);
    } else {
        if (barw && !bar_hidden) {
            int bx = card_x + CARD_PAD;
            int by = card_y + card_h - BAR_H - 4;
            int bw = card_w - 2 * CARD_PAD;
            if (by < card_y + TITLE_H)
                by = card_y + TITLE_H;
            barw->resize(bx, by, bw, BAR_H);
            if (dpy && fl_xid(barw))
                XMoveResizeWindow(dpy, fl_xid(barw), bx, by, bw, BAR_H);
            if (!barw->shown())
                barw->show();
        }
        layout_bar();
        if (move_video && dpy && mpwin) {
            video_rect(&vx, &vy, &vw, &vh);
            XMapWindow(dpy, mpwin);
            XMoveResizeWindow(dpy, mpwin, vx, vy, vw, vh);
        }
    }
    if (listw && listw->shown() && list_br) {
        int px = card_x + card_w - LIST_W;
        int py = card_y + TITLE_H;
        int ph = card_h - TITLE_H;
        if (ph < 80)
            ph = 80;
        if (px < 0)
            px = 0;
        listw->resize(px, py, LIST_W, ph);
        if (list_hd)
            list_hd->resize(8, 4, LIST_W - 16, LIST_HD - 6);
        list_br->resize(8, LIST_HD, LIST_W - 16, ph - LIST_HD - LIST_FT - 8);
        if (btn_pl_save)
            ((Fl_Widget *)btn_pl_save)->resize(8, ph - LIST_FT, 88, 26);
        if (btn_pl_del)
            ((Fl_Widget *)btn_pl_del)->resize(104, ph - LIST_FT, 88, 26);
        if (btn_pl_clear)
            ((Fl_Widget *)btn_pl_clear)->resize(200, ph - LIST_FT, 88, 26);
        if (dpy && fl_xid(listw))
            XMoveResizeWindow(dpy, fl_xid(listw), px, py, LIST_W, ph);
    }
    if (!win_min && mpwin)
        XMapWindow(dpy, mpwin);
    pin_stage();
}

static void toggle_max(void)
{
    win_max = !win_max;
    win_min = 0;
    card_defaults();
    apply_card(1);
    fprintf(stderr, "player-v25 max=%d\n", win_max);
}

static void toggle_min(void)
{
    win_min = !win_min;
    if (win_min)
        win_max = 0;
    card_defaults();
    apply_card(1);
    fprintf(stderr, "player-v25 min=%d\n", win_min);
}

static void tt_drop_bar_clock(void)
{
    tt_bar_follow = 0;
    tt_bar_hold = -1;
    tt_slave_tpos = -1;
}

/* After pause, the player's time answer can stay on the pause moment.
   The bar then keeps that moment while the picture moves. Count seconds
   from the pause moment until a later answer moves forward. */
static void tt_arm_bar_clock(void)
{
    tt_bar_hold = tpos;
    if (tt_bar_hold < 0)
        tt_bar_hold = 0;
    tt_bar_ms = now_ms();
    tt_slave_tpos = tt_bar_hold;
    tt_bar_follow = 1;
    fprintf(stderr, "player-v230 bar\n");
}

static void tt_apply_bar_clock(unsigned long now)
{
    int shown;

    if (!tt_bar_follow || paused || skip_busy || mpid <= 0)
        return;
    if (tt_bar_hold < 0)
        return;
    if (tpos > tt_slave_tpos) {
        tt_slave_tpos = tpos;
        tt_bar_hold = tpos;
        tt_bar_ms = now;
        return;
    }
    shown = tt_bar_hold + (int)((now - tt_bar_ms) / 1000UL);
    if (shown < tt_bar_hold)
        shown = tt_bar_hold;
    if (tlen > 0 && shown > tlen)
        shown = tlen;
    tpos = shown;
    if (tlen > 0) {
        pct = tpos * 100 / tlen;
        if (pct > 100)
            pct = 100;
    }
}

/* The number between minus and plus is the level the sound uses.
   One absolute set stops pause from dropping to half and resume from
   jumping to full. The older double set stays in this file and is not called. */
static void tt_commit_volume(void)
{
    char buf[72];

    if (vol < 0)
        vol = 0;
    if (vol > 100)
        vol = 100;
    snprintf(buf, sizeof buf, "pausing_keep_force volume %d 1\n", vol);
    send_cmd(buf);
    fprintf(stderr, "player-v231 vol %d\n", vol);
}

static void tt_remember_bar(void)
{
    tt_bar_frozen = tpos;
    if (tt_bar_frozen < 0)
        tt_bar_frozen = 0;
    tt_pct_frozen = pct;
    if (tt_pct_frozen < 0)
        tt_pct_frozen = 0;
    tt_drop_bar_clock();
    fprintf(stderr, "player-v231 hold\n");
}

static void tt_hold_paused_bar(void)
{
    if (!paused || tt_bar_frozen < 0)
        return;
    tpos = tt_bar_frozen;
    pct = tt_pct_frozen;
}

static void tt_pause_volume_and_bar(int now_paused)
{
    if (now_paused) {
        tt_remember_bar();
        tt_vol_push = 0;
        tt_commit_volume();
        return;
    }
    tt_bar_frozen = -1;
    tt_arm_bar_clock();
    tt_vol_push = 0;
    tt_commit_volume();
}

/* A new picture starts playing. The pause flag from the previous clip
   must not stay on, or the bar stays still while this picture moves. */
static void tt_begin_bar_clip(void)
{
    tt_spot = -1;
    tt_spot_pct = -1;
    tt_play_at = -1;
    paused = 0;
    fprintf(stderr, "player-v232 clip-bar\n");
}

static void tt_mark_bar_pause(int now_paused)
{
    if (now_paused) {
        tt_spot = tpos;
        if (tt_spot < 0)
            tt_spot = 0;
        tt_spot_pct = pct;
        if (tt_spot_pct < 0)
            tt_spot_pct = 0;
        tt_play_at = -1;
        fprintf(stderr, "player-v232 pause-bar\n");
        return;
    }
    tt_play_at = tt_spot;
    if (tt_play_at < 0)
        tt_play_at = tpos;
    if (tt_play_at < 0)
        tt_play_at = 0;
    tt_play_ms = now_ms();
    fprintf(stderr, "player-v232 play-bar\n");
}

static void tt_note_bar_seek(int sec)
{
    if (sec < 0)
        sec = 0;
    if (tlen > 0 && sec > tlen)
        sec = tlen;
    if (paused) {
        tpos = sec;
        if (tlen > 0) {
            pct = tpos * 100 / tlen;
            if (pct > 100)
                pct = 100;
        }
        tt_spot = tpos;
        tt_spot_pct = pct < 0 ? 0 : pct;
        tt_play_at = -1;
        return;
    }
    /* Before the first pause, the player's answers still own the bar. */
    if (tt_play_at < 0)
        return;
    tpos = sec;
    if (tlen > 0) {
        pct = tpos * 100 / tlen;
        if (pct > 100)
            pct = 100;
    }
    tt_play_at = tpos;
    tt_play_ms = now_ms();
}

/* Last writer of the bar. Pause pins the saved second. Play counts
   forward from it. A stuck player answer must not hold the bar, and a
   far-ahead leftover answer must not jump it after a backward seek. */
static void tt_show_bar_time(unsigned long now)
{
    int shown;
    int slave;
    int elapsed;

    if (skip_busy || mpid <= 0)
        return;
    if (paused) {
        if (tt_spot < 0) {
            tt_spot = tpos < 0 ? 0 : tpos;
            tt_spot_pct = pct < 0 ? 0 : pct;
        }
        tpos = tt_spot;
        pct = tt_spot_pct;
        if (pct < 0)
            pct = 0;
        return;
    }
    if (tt_play_at < 0)
        return;
    if (now < tt_play_ms)
        tt_play_ms = now;
    elapsed = (int)((now - tt_play_ms) / 1000UL);
    if (elapsed < 0)
        elapsed = 0;
    shown = tt_play_at + elapsed;
    if (shown < tt_play_at)
        shown = tt_play_at;
    slave = tpos;
    if (slave > shown) {
        /* Just after a backward seek, an old answer is still ahead.
           Ignore that for two seconds. A stuck answer is not ahead,
           so it cannot stop the count. */
        if (!((now - tt_play_ms) < 2000UL && slave > shown + 3)) {
            tt_play_at = slave;
            tt_play_ms = now;
            shown = slave;
        }
    }
    if (tlen > 0 && shown > tlen)
        shown = tlen;
    tpos = shown;
    if (tlen > 0) {
        pct = tpos * 100 / tlen;
        if (pct > 100)
            pct = 100;
    }
}

static void tt_send_heard_volume(void)
{
    char buf[72];

    if (vol < 0)
        vol = 0;
    if (vol > 100)
        vol = 100;
    snprintf(buf, sizeof buf, "pausing_keep_force volume %d 1\n", vol);
    send_cmd(buf);
    snprintf(buf, sizeof buf, "pausing_keep_force set_property volume %d\n", vol);
    send_cmd(buf);
}

/* The hold clears the grab flag and does not release the keys.
   Mark them grabbed so the release runs, then take them again. */
static void tt_regrab_volume_keys(void)
{
    vol_grabbed = 1;
    ungrab_vol_keys();
    grab_vol_keys();
}

static void tt_push_volume_after_pause(void)
{
    if (tt_vol_push <= 0 || paused || skip_busy || mpid <= 0)
        return;
    tt_vol_push--;
    tt_send_heard_volume();
}

static void tt_after_pause_toggle(int now_paused)
{
    if (now_paused) {
        tt_drop_bar_clock();
        tt_vol_push = 0;
        return;
    }
    tt_arm_bar_clock();
    tt_regrab_volume_keys();
    tt_vol_push = 3;
    tt_send_heard_volume();
    fprintf(stderr, "player-v230 vol\n");
}

static void tt_mark_load_start(void)
{
    load_failed = 0;
    tt_load_gave_up = 0;
    tt_load_since = now_ms();
    tt_vol_push = 0;
    tt_drop_bar_clock();
    unlink(FAILF);
    fprintf(stderr, "player-v230 load\n");
}

/* A real failure leaves the fail file and stops the download.
   A new download has no fail file, so the old sentence must not stay. */
static void tt_hide_false_load(void)
{
    if (user_quit || tt_load_gave_up)
        return;
    if (file_exists(FAILF))
        return;
    if (download_alive() || (mpid > 0 && kill(mpid, 0) == 0))
        load_failed = 0;
}

static void tt_note_play_started(void)
{
    if (user_quit || file_exists(FAILF))
        return;
    if (mpid > 0 && kill(mpid, 0) == 0) {
        load_failed = 0;
        tt_load_gave_up = 0;
    }
}

static int local_ready(void)
{
    struct stat st;

    if (local_path[0] && stat(local_path, &st) == 0 && st.st_size > 64)
        return 1;
    if (!local_mode && stat("/tmp/minios-ttplay.mp4", &st) == 0 && st.st_size > 64) {
        snprintf(local_path, sizeof(local_path), "/tmp/minios-ttplay.mp4");
        return 1;
    }
    return 0;
}

static void start_local_file(const char *path)
{
    const char *bin;
    char gen[16];

    if (!path || !path[0] || user_quit)
        return;
    unlink(QUITF);
    tt_commit_volume();
    snprintf(local_path, sizeof(local_path), "%s", path);
    if (strncmp(path, "/tmp/minios-ttplay", 18) != 0)
        clear_youtube_media();
    tt_mark_load_start();
    skip_busy = 1;
    have_playback = 0;
    really_played = 0;
    play_alive_since = 0;
    saw_start = 0;
    clip_done = 0;
    max_tpos = 0;
    pct = 0;
    tpos = 0;
    dismiss_popup();
    bump_gen();
    stop_current_mplayer();
    unlink(PIDF);
    unlink(FAILF);
    unlink(OUT);
    unlink(DONEF);
    write_geom();
    snprintf(dl_lab, sizeof(dl_lab), "Loading media");
    dl_pct = 100;
    last_ui_dl = -1;
    bin = "/opt/minios/minios-tt-mplayer";
    if (access(bin, X_OK) != 0)
        bin = "/usr/local/bin/minios-tt-mplayer";
    snprintf(gen, sizeof(gen), "%d", play_gen);
    if (fork() == 0) {
        setenv("MINIOS_QUEUE_PLAY", "1", 1);
        setenv("MINIOS_PLAY_GEN", gen, 1);
        unsetenv("MINIOS_LOOP");
        execl(bin, "minios-mplayer", local_path, (char *)0);
        _exit(127);
    }
    fprintf(stderr, "player-v38 replay local %s gen=%d loop=%d\n",
            local_path, play_gen, loop_mode);
    refresh_ui();
    show_bar();
}

static void start_pl_item(int idx)
{
    const char *bin;

    tt_note_watched();
    if (pl_count <= 0 || user_quit)
        return;
    unlink(QUITF);
    tt_commit_volume();
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
    tt_end_asked = 0;
    tt_end_hold = 0;
    tt_hide_end_popup();
    tt_unmap_search();
    tt_mark_load_start();
    skip_busy = 1;
    have_playback = 0;
    really_played = 0;
    play_alive_since = 0;
    saw_start = 0;
    clip_done = 0;
    max_tpos = 0;
    dismiss_popup();
    bump_gen();
    stop_download();
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
    unlink(OUT);
    unlink(DONEF);
    hide_playlist();
    hide_add_list();
    if (pl_urls[pl_cur][0] == '/' || strncmp(pl_urls[pl_cur], "file:", 5) == 0) {
        const char *lp = pl_urls[pl_cur];
        if (strncmp(lp, "file:", 5) == 0)
            lp += 5;
        start_local_file(lp);
        return;
    }
    if (cache_play(pl_urls[pl_cur])) {
        fprintf(stderr, "player-v24 next clip cached %d %s\n", pl_cur, pl_urls[pl_cur]);
        return;
    }
    {
        FILE *pf = fopen(PROGF, "w");
        if (pf) {
            fprintf(pf, "0\nLoading media\n");
            fclose(pf);
        }
    }
    write_geom();
    if (strstr(pl_urls[pl_cur], "tiktok.com")) {
        FILE *tf = fopen(PROGF, "w");
        if (tf) {
            fputs("0\nDownloading TikTok video\n", tf);
            fclose(tf);
        }
        bin = "/opt/minios/minios-tiktok-play";
        if (access(bin, X_OK) != 0)
            bin = "/usr/local/bin/minios-tiktok-play";
        if (fork() == 0) {
            execl(bin, "minios-tiktok-play", pl_urls[pl_cur], (char *)0);
            _exit(127);
        }
    } else {
        bin = "/opt/minios/minios-tiktok-play";
        if (access(bin, X_OK) != 0)
            bin = "/usr/local/bin/minios-tiktok-play";
        if (fork() == 0) {
            execl(bin, "minios-tiktok-play", pl_urls[pl_cur], (char *)0);
            _exit(127);
        }
    }
    tt_show_stage();
    if (loadw)
        loadw->redraw();
    refresh_ui();
    if (barw && fl_xid(barw)) {
        XRaiseWindow(dpy, fl_xid(barw));
        XSetInputFocus(dpy, fl_xid(barw), RevertToParent, CurrentTime);
    }
    fprintf(stderr, "player-v21 next clip %d %s\n", pl_cur, pl_urls[pl_cur]);
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

static void toggle_pause(const char *why)
{
    char buf[48];
    int resume_at = -1;

    if (paused && tt_pause_at && tpos > 0 &&
        now_ms() - tt_pause_at >= 20000UL)
        resume_at = tpos;
    if (resume_at >= 0) {
        snprintf(buf, sizeof buf, "pausing_keep_force seek %d 2\n", resume_at);
        send_cmd(buf);
    }
    if (!send_cmd("pause\n"))
        return;
    paused = !paused;
    pause_user = 1;
    quiet_until = now_ms() + 1500;
    if (paused)
        tt_pause_at = now_ms();
    else
        tt_pause_at = 0;
    tt_mark_bar_pause(paused);
    tt_commit_volume();
    fprintf(stderr, "player-v8 %s wrote pause now=%d\n", why, paused);
}

static void set_vol(int v)
{
    if (v < 0)
        v = 0;
    if (v > 100)
        v = 100;
    vol = v;
    tt_commit_volume();
    fprintf(stderr, "player-v7 vol %d\n", vol);
}

static void seek_pct(int p)
{
    char buf[48];

    if (p < 0)
        p = 0;
    if (p > 100)
        p = 100;
    tt_drop_bar_clock();
    snprintf(buf, sizeof(buf), "pausing_keep_force seek %d 1\n", p);
    send_cmd(buf);
    pct = p;
    if (tlen > 0)
        tpos = tlen * p / 100;
    if (paused) {
        tt_bar_frozen = tpos;
        tt_pct_frozen = pct;
    }
    tt_note_bar_seek(tpos);
}

static void do_cc(void)
{
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
}

static void key_cmd(int ks)
{
    if (ks == ' ' || ks == XK_space)
        toggle_pause("space");
    else if ((ks == 'n' || ks == 'N' || ks == FL_Down) && queue_on) {
        start_pl_item(pl_cur + 1);
    } else if ((ks == 'p' || ks == 'P') && queue_on) {
        start_pl_item(pl_cur - 1);
    } else if ((ks == 'a' || ks == 'A') && queue_on)
        autoplay = !autoplay;
    else if (ks == 'l' || ks == 'L') {
        loop_mode = (loop_mode + 1) % 3;
        apply_loop_to_player();
    }
    else if (ks == FL_Left) {
        tt_drop_bar_clock();
        send_cmd("seek -10 0\n");
        tt_note_bar_seek(tpos - 10);
    } else if (ks == FL_Right) {
        tt_drop_bar_clock();
        send_cmd("seek 10 0\n");
        tt_note_bar_seek(tpos + 10);
    }
    else if (ks == '-' || ks == '_' || ks == '9')
        set_vol(vol - 10);
    else if (ks == '+' || ks == '=' || ks == '0' || ks == FL_Up)
        set_vol(vol + 10);
    else if (ks == 'm' || ks == 'M')
        send_cmd("mute\n");
    else if (ks == 'c' || ks == 'C')
        do_cc();
    else if (ks == FL_Escape && listw && listw->shown()) {
        hide_playlist();
        return;
    } else if (ks == FL_F + 11) {
        toggle_max();
        return;
    } else if (ks == 'q' || ks == 'Q' || ks == FL_Escape) {
        request_quit();
        return;
    }
    refresh_ui();
}

class IconBtn : public Fl_Button {
public:
    int kind;
    IconBtn(int X, int Y, int W, int H, int k) : Fl_Button(X, Y, W, H, 0), kind(k)
    {
        box(FL_FLAT_BOX);
        color(C_FACE);
        down_color(C_FACE_ON);
        labelsize(1);
    }
    void draw()
    {
        int cx = x() + w() / 2;
        int cy = y() + h() / 2;
        Fl_Color face = value() ? down_color() : color();
        fl_color(face);
        fl_rectf(x(), y(), w(), h());
        fl_color(Fl::focus() == this ? C_FOCUS : C_TEXT);
        fl_rect(x(), y(), w() - 1, h() - 1);
        if (Fl::focus() == this)
            fl_rect(x() + 1, y() + 1, w() - 3, h() - 3);
        fl_color(C_TEXT);
        if (kind == IC_PLAY)
            fl_polygon(cx - 5, cy - 8, cx - 5, cy + 8, cx + 9, cy);
        else if (kind == IC_PAUSE) {
            fl_rectf(cx - 7, cy - 8, 5, 16);
            fl_rectf(cx + 3, cy - 8, 5, 16);
        } else if (kind == IC_PREV) {
            fl_polygon(cx + 2, cy - 7, cx + 2, cy + 7, cx - 8, cy);
            fl_rectf(cx - 10, cy - 7, 3, 14);
        } else if (kind == IC_NEXT) {
            fl_polygon(cx - 2, cy - 7, cx - 2, cy + 7, cx + 8, cy);
            fl_rectf(cx + 8, cy - 7, 3, 14);
        } else if (kind == IC_MIN) {
            fl_rectf(cx - 6, cy + 3, 12, 2);
        } else if (kind == IC_MAX) {
            if (win_max) {
                fl_rect(cx - 4, cy - 6, 10, 10);
                fl_rect(cx - 7, cy - 3, 10, 10);
            } else {
                fl_rect(cx - 6, cy - 6, 12, 12);
            }
        } else if (kind == IC_CLOSE) {
            fl_line_style(FL_SOLID, 2);
            fl_line(cx - 6, cy - 6, cx + 6, cy + 6);
            fl_line(cx + 6, cy - 6, cx - 6, cy + 6);
            fl_line_style(0);
        }
    }
};

class TextBtn : public Fl_Button {
public:
    int lit;
    TextBtn(int X, int Y, int W, int H, const char *L) : Fl_Button(X, Y, W, H, L), lit(0)
    {
        box(FL_FLAT_BOX);
        color(C_FACE);
        down_color(C_FACE_ON);
        labelcolor(C_TEXT);
        labelfont(FL_HELVETICA_BOLD);
        labelsize(12);
        align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    }
    void draw()
    {
        Fl_Color face = (value() || lit) ? down_color() : color();
        fl_color(face);
        fl_rectf(x(), y(), w(), h());
        fl_color(Fl::focus() == this ? C_FOCUS : (lit ? C_FOCUS : C_TEXT));
        fl_rect(x(), y(), w() - 1, h() - 1);
        if (Fl::focus() == this)
            fl_rect(x() + 1, y() + 1, w() - 3, h() - 3);
        fl_font(labelfont(), labelsize());
        fl_color(C_TEXT);
        fl_draw(label(), x(), y(), w(), h(), FL_ALIGN_CENTER);
    }
};

class SeekBar : public Fl_Widget {
public:
    SeekBar(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H) {}
    void draw()
    {
        int fill = 0;
        int ky = y() + 3;
        int kh = h() - 6;
        if (kh < 6)
            kh = h();
        fl_color(C_TRACK);
        fl_rectf(x(), ky, w(), kh);
        if (have_playback)
            fill = w() * pct / 100;
        if (tlen > 0)
            fill = w() * tpos / tlen;
        if (fill > w())
            fill = w();
        if (fill > 0) {
            fl_color(C_CYAN);
            fl_rectf(x(), ky, fill, kh);
            int kx = x() + fill - 6;
            if (kx < x())
                kx = x();
            fl_color(C_CYAN);
            fl_pie(kx, y(), h(), h(), 0, 360);
        }
        fl_color(C_CYAN_DIM);
        fl_rect(x(), ky, w() - 1, kh - 1);
        if (Fl::focus() == this) {
            fl_color(C_FOCUS);
            fl_rect(x(), y(), w() - 1, h() - 1);
        }
    }
    int handle(int ev)
    {
        if (ev == FL_FOCUS || ev == FL_UNFOCUS) {
            redraw();
            return 1;
        }
        if ((ev == FL_PUSH || ev == FL_DRAG) && (have_playback || mpid > 0) && w() > 0) {
            take_focus();
            seek_pct((Fl::event_x() - x()) * 100 / w());
            refresh_ui();
            return 1;
        }
        return 0;
    }
};

/* A moving download keeps its own words. The failure sentence stays
   only when the progress line is not a live download step. */
static int tt_label_is_live(const char *s)
{
    static const char *live[] = {
        "Downloading", "Optimizing", "Opening", "Loading",
        "Finding", "Saving", "Trying", "Preparing", NULL
    };
    int i;

    if (!s || !s[0])
        return 0;
    for (i = 0; live[i]; i++) {
        size_t n = strlen(live[i]);
        if (strncmp(s, live[i], n) == 0)
            return 1;
    }
    return 0;
}

static int tt_last_pct = -1;
static unsigned long tt_pct_move_ms;

static void tt_watch_progress(void)
{
    if (dl_pct != tt_last_pct) {
        tt_last_pct = dl_pct;
        tt_pct_move_ms = now_ms();
    }
}

/* The bar is a real download while its pid is alive, or while the
   percent changed in the last few seconds. A stuck bar is a failure. */
static int tt_download_on_screen(void)
{
    if (!tt_label_is_live(dl_lab))
        return 0;
    if (download_alive())
        return 1;
    if (tt_pct_move_ms && now_ms() - tt_pct_move_ms < 3000UL)
        return 1;
    return 0;
}

static const char *tt_load_sentence(void)
{
    static int tt_live_logged;

    tt_watch_progress();
    if (tt_download_on_screen()) {
        if (load_failed && !tt_live_logged) {
            tt_live_logged = 1;
            fprintf(stderr, "player-v237 load-line\n");
        }
        return dl_lab;
    }
    if (load_failed)
        return "Could not load the video.";
    if (dl_lab[0])
        return dl_lab;
    return "Loading media. Please wait.";
}

class LoadWin : public Fl_Window {
public:
    LoadWin(int X, int Y, int W, int H) : Fl_Window(X, Y, W, H, TITLE)
    {
        color(C_NAVY);
        box(FL_FLAT_BOX);
        set_override();
        border(0);
        end();
    }
    void draw()
    {
        const char *msg;
        char extra[80], pcts[16];
        int x, bar_w, bar_x, bar_y, fill, mid;
        int playing = (have_playback && !skip_busy && mpid > 0 && kill(mpid, 0) == 0);

        fl_color(C_NAVY);
        fl_rectf(0, 0, w(), h());
        if (win_min)
            return;
        if (playing)
            return;
        read_progress();
        /* Dock / idle: navy only. Show text when a download label exists. */
        if (!skip_busy && !download_alive() && mpid <= 0 && !load_failed &&
            !dl_lab[0])
            return;
        msg = tt_load_sentence();
        mid = TITLE_H + (h() - TITLE_H) / 2;
        fl_font(FL_HELVETICA_BOLD, 16);
        fl_color(C_TEXT);
        x = (w() - (int)fl_width(msg)) / 2;
        if (x < 16)
            x = 16;
        fl_draw(msg, x, mid - 36);
        if (pl_count > 0) {
            snprintf(extra, sizeof(extra), "Search result %d of %d",
                     pl_cur + 1, pl_count);
            x = (w() - (int)fl_width(extra)) / 2;
            if (x < 16)
                x = 16;
            fl_draw(extra, x, mid - 14);
        }
        bar_w = w() * 2 / 3;
        if (bar_w < 160)
            bar_w = w() - 40;
        bar_x = (w() - bar_w) / 2;
        bar_y = mid + 12;
        fl_color(C_TRACK);
        fl_rectf(bar_x, bar_y, bar_w, 18);
        fill = bar_w * dl_pct / 100;
        if (fill > 0) {
            fl_color(C_CYAN);
            fl_rectf(bar_x, bar_y, fill, 18);
        }
        fl_color(C_CYAN);
        fl_rect(bar_x, bar_y, bar_w, 18);
        snprintf(pcts, sizeof(pcts), "%d%%", dl_pct);
        fl_draw(pcts, bar_x + bar_w + 8, bar_y + 14);
    }
    int handle(int ev)
    {
        if (ev == FL_PUSH || ev == FL_DRAG || ev == FL_RELEASE || ev == FL_FOCUS)
            note_mouse();
        if (ev == FL_PUSH && Fl::event_y() < TITLE_H) {
            if (Fl_Window::handle(ev))
                return 1;
            if (Fl::event_clicks()) {
                toggle_max();
                return 1;
            }
            if (win_min) {
                toggle_min();
                return 1;
            }
            drag_on = 1;
            drag_ox = Fl::event_x_root() - card_x;
            drag_oy = Fl::event_y_root() - card_y;
            return 1;
        }
        if (ev == FL_DRAG && drag_on && !win_max) {
            card_x = Fl::event_x_root() - drag_ox;
            card_y = Fl::event_y_root() - drag_oy;
            if (card_x < 0)
                card_x = 0;
            if (card_y < 0)
                card_y = 0;
            if (card_x + 80 > ::dw)
                card_x = ::dw - 80;
            if (card_y + TITLE_H > ::dh)
                card_y = ::dh - TITLE_H;
            apply_card(1);
            return 1;
        }
        if (ev == FL_RELEASE)
            drag_on = 0;
        return Fl_Window::handle(ev);
    }
};

class TitleWin : public Fl_Window {
public:
    TitleWin(int X, int Y, int W, int H) : Fl_Window(X, Y, W, H, TITLE)
    {
        color(C_NAVY_HI);
        box(FL_FLAT_BOX);
        set_override();
        border(0);
        end();
    }
    void draw()
    {
        fl_color(C_NAVY_HI);
        fl_rectf(0, 0, w(), h());
        fl_color(C_CYAN);
        fl_rectf(0, h() - 2, w(), 2);
        fl_font(FL_HELVETICA_BOLD, 13);
        fl_color(C_TEXT);
        fl_draw(TITLE, 12, 0, w() - 130, TITLE_TOP, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        draw_children();
    }
    int handle(int ev)
    {
        if (ev == FL_PUSH || ev == FL_DRAG || ev == FL_FOCUS)
            note_mouse();
        if (ev == FL_PUSH) {
            if (Fl_Window::handle(ev))
                return 1;
            if (Fl::event_clicks()) {
                toggle_max();
                return 1;
            }
            if (win_min) {
                toggle_min();
                return 1;
            }
            drag_on = 1;
            drag_ox = Fl::event_x_root() - card_x;
            drag_oy = Fl::event_y_root() - card_y;
            return 1;
        }
        if (ev == FL_DRAG && drag_on && !win_max) {
            card_x = Fl::event_x_root() - drag_ox;
            card_y = Fl::event_y_root() - drag_oy;
            if (card_x < 0)
                card_x = 0;
            if (card_y < 0)
                card_y = 0;
            if (card_x + 80 > ::dw)
                card_x = ::dw - 80;
            if (card_y + TITLE_H > ::dh)
                card_y = ::dh - TITLE_H;
            apply_card(1);
            return 1;
        }
        if (ev == FL_RELEASE)
            drag_on = 0;
        return Fl_Window::handle(ev);
    }
};

class BarWin : public Fl_Double_Window {
public:
    BarWin(int Y, int W, int H) : Fl_Double_Window(0, Y, W, H, TITLE)
    {
        color(C_NAVY);
        box(FL_FLAT_BOX);
        set_override();
        border(0);
        /* Navy overlay. No File/Playback/View menu. No extra images. */
    }
    void draw()
    {
        int cx, tx, twide;
        Fl_Double_Window::draw();
        fl_color(C_NAVY_HI);
        fl_rectf(0, 0, w(), 2);
        cx = w() / 2;
        tx = cx + 76;
        twide = w() - 8 - tx;
        if (twide > 36)
            tt_draw_title_lower_right(tx, 28, twide, 28);
    }
    int handle(int ev)
    {
        if (ev == FL_MOVE || ev == FL_ENTER || ev == FL_PUSH || ev == FL_DRAG)
            note_mouse();
        if (ev == FL_KEYDOWN || ev == FL_SHORTCUT) {
            int ks = Fl::event_key();
            note_mouse();
            if (ks == FL_Tab)
                return Fl_Double_Window::handle(ev);
            if ((ks == FL_Enter || ks == FL_KP_Enter) &&
                Fl::focus() && Fl::focus() != this)
                return Fl_Double_Window::handle(ev);
            if (ks == ' ' && Fl::focus() && Fl::focus() != this &&
                Fl::focus() != seek_bar)
                return Fl_Double_Window::handle(ev);
            key_cmd(ks);
            return 1;
        }
        return Fl_Double_Window::handle(ev);
    }
};

static void blank_load(void)
{
    if (loadw)
        loadw->redraw();
}

static void note_mouse(void)
{
    last_mouse = now_ms();
    if (bar_hidden)
        show_bar();
}

static void show_bar(void)
{
    int bx, by, bw;

    if (!barw || win_min)
        return;
    bar_hidden = 0;
    bx = card_x + CARD_PAD;
    by = card_y + card_h - BAR_H - 4;
    bw = card_w - 2 * CARD_PAD;
    if (by < card_y + TITLE_H)
        by = card_y + TITLE_H;
    if (!barw->shown())
        barw->show();
    barw->resize(bx, by, bw, BAR_H);
    if (dpy && fl_xid(barw))
        XMoveResizeWindow(dpy, fl_xid(barw), bx, by, bw, BAR_H);
    layout_bar();
    grab_keys();
}

static void hide_bar(void)
{
    if (!barw || bar_hidden || skip_busy || !have_playback)
        return;
    if (listw && listw->shown())
        return;
    bar_hidden = 1;
    barw->hide();
    last_ptr_x = -1;
    last_ptr_y = -1;
}

static void raise_named(const char *want)
{
    Window root, parent, *kids = NULL;
    unsigned n = 0, i;
    char *name = NULL;

    if (!dpy || !want)
        return;
    if (!XQueryTree(dpy, RootWindow(dpy, DefaultScreen(dpy)), &root, &parent, &kids, &n))
        return;
    for (i = 0; i < n; i++) {
        name = NULL;
        if (XFetchName(dpy, kids[i], &name) && name) {
            if (strstr(name, want)) {
                XMapRaised(dpy, kids[i]);
                XSetInputFocus(dpy, kids[i], RevertToParent, CurrentTime);
            }
            XFree(name);
        }
    }
    if (kids)
        XFree(kids);
}

static void hide_playlist(void)
{
    if (listw && listw->shown())
        listw->hide();
    if (btn_tog)
        btn_tog->take_focus();
}

static void cb_list_pick(Fl_Widget *, void *)
{
    int n;

    if (!list_br)
        return;
    n = list_br->value();
    if (n < 1 || n > pl_count)
        return;
    hide_playlist();
    start_pl_item(n - 1);
}

static void cb_pl_save(Fl_Widget *, void *)
{
    save_local_playlist();
    refresh_ui();
}

static void cb_pl_del(Fl_Widget *, void *)
{
    int n = list_br ? list_br->value() : 0;

    if (n < 1 || n > pl_count)
        n = pl_cur + 1;
    if (n < 1 || n > pl_count)
        return;
    drop_pl_item(n - 1);
    if (pl_count > 0)
        show_playlist_panel();
    else
        hide_playlist();
    refresh_ui();
}

static void cb_pl_clear(Fl_Widget *, void *)
{
    clear_playlist();
    show_playlist_panel();
    refresh_ui();
}

static void show_playlist_panel(void)
{
    int i, px, py, ph, bh;
    char line[280];

    px = card_x + card_w - LIST_W;
    py = card_y + TITLE_H;
    ph = card_h - TITLE_H;
    if (ph < 120)
        ph = 120;
    if (px < 0)
        px = 0;
    bh = ph - LIST_HD - LIST_FT - 8;
    if (bh < 40)
        bh = 40;
    if (!listw) {
        listw = new Fl_Window(px, py, LIST_W, ph, "Playlist");
        listw->set_override();
        listw->border(0);
        listw->color(C_NAVY);
        list_hd = new Fl_Box(8, 4, LIST_W - 16, LIST_HD - 6, "Playlist");
        list_hd->box(FL_NO_BOX);
        list_hd->labelfont(FL_HELVETICA_BOLD);
        list_hd->labelsize(13);
        list_hd->labelcolor(C_CYAN);
        list_hd->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        list_br = new Fl_Hold_Browser(8, LIST_HD, LIST_W - 16, bh);
        list_br->color(C_NAVY);
        list_br->textcolor(C_TEXT);
        list_br->selection_color(C_FACE_ON);
        list_br->textfont(FL_HELVETICA);
        list_br->textsize(14);
        list_br->callback(cb_list_pick);
        list_br->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
        btn_pl_save = new TextBtn(8, ph - LIST_FT, 88, 26, "Save");
        btn_pl_save->tooltip("Save this playlist for the next Media Player open");
        btn_pl_save->callback(cb_pl_save);
        btn_pl_del = new TextBtn(104, ph - LIST_FT, 88, 26, "Delete");
        btn_pl_del->tooltip("Remove the selected row");
        btn_pl_del->callback(cb_pl_del);
        btn_pl_clear = new TextBtn(200, ph - LIST_FT, 88, 26, "Clear");
        btn_pl_clear->tooltip("Remove every row");
        btn_pl_clear->callback(cb_pl_clear);
        listw->end();
        listw->callback([](Fl_Widget *w, void *) { w->hide(); });
    } else {
        listw->resize(px, py, LIST_W, ph);
        if (list_hd)
            list_hd->resize(8, 4, LIST_W - 16, LIST_HD - 6);
        list_br->resize(8, LIST_HD, LIST_W - 16, bh);
        if (btn_pl_save)
            btn_pl_save->resize(8, ph - LIST_FT, 88, 26);
        if (btn_pl_del)
            btn_pl_del->resize(104, ph - LIST_FT, 88, 26);
        if (btn_pl_clear)
            btn_pl_clear->resize(200, ph - LIST_FT, 88, 26);
    }
    if (list_hd)
        list_hd->copy_label(local_mode ? "My playlist" : "TikTok playlist");
    list_br->clear();
    if (pl_count <= 0)
        list_br->add("Playlist is empty. Use Add.");
    for (i = 0; i < pl_count; i++) {
        const char *t = pl_titles[i][0] ? pl_titles[i] : pl_urls[i];
        char safe[240];
        size_t j = 0;
        while (*t && j + 1 < sizeof(safe)) {
            if (*t == '@') {
                if (j + 2 >= sizeof(safe))
                    break;
                safe[j++] = '@';
                safe[j++] = '@';
            } else if (*t != '\t' && *t != '\r') {
                safe[j++] = *t;
            }
            t++;
        }
        safe[j] = 0;
        snprintf(line, sizeof(line), "%d.  %s", i + 1, safe);
        list_br->add(line);
    }
    if (pl_cur >= 0 && pl_cur < pl_count)
        list_br->value(pl_cur + 1);
    listw->show();
    if (list_br)
        list_br->take_focus();
    if (dpy && fl_xid(listw))
        XRaiseWindow(dpy, fl_xid(listw));
}

static void open_playlist(void)
{
    note_mouse();
    if (listw && listw->shown()) {
        hide_playlist();
        return;
    }
    hide_add_list();
    show_playlist_panel();
}

static const char *audio_start_dir(void)
{
    FILE *df;
    char line[160], mp[64], probe[128];
    static char usb[96];

    if (file_exists("/opt/minios/music/Unforgettable.mp3"))
        return "/opt/minios/music";
    if (file_exists("/opt/minios/music"))
        return "/opt/minios/music";
    df = fopen("/proc/mounts", "r");
    if (df) {
        while (fgets(line, sizeof(line), df)) {
            if (sscanf(line, "%*s %63s", mp) != 1)
                continue;
            snprintf(probe, sizeof(probe), "%s/minios-music", mp);
            if (file_exists(probe)) {
                snprintf(usb, sizeof(usb), "%s", probe);
                fclose(df);
                return usb;
            }
        }
        fclose(df);
    }
    if (file_exists("/home/tc/music"))
        return "/home/tc/music";
    if (file_exists("/tmp"))
        return "/tmp";
    return ".";
}

static void add_scan_dir(const char *dir)
{
    DIR *d;
    struct dirent *e;
    char path[256];
    size_t n;

    if (!dir || add_count >= MAXPL)
        return;
    d = opendir(dir);
    if (!d)
        return;
    while ((e = readdir(d)) && add_count < MAXPL) {
        const char *ext;
        n = strlen(e->d_name);
        if (n < 4 || e->d_name[0] == '.')
            continue;
        ext = e->d_name + n - 4;
        if (strcasecmp(ext, ".mp3") != 0 && strcasecmp(ext, ".m4a") != 0 &&
            strcasecmp(ext, ".ogg") != 0 && strcasecmp(ext, ".wav") != 0 &&
            strcasecmp(ext, ".aac") != 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        {
            int d;
            int seen = 0;
            for (d = 0; d < add_count; d++) {
                if (strcmp(add_paths[d], path) == 0) {
                    seen = 1;
                    break;
                }
            }
            if (seen)
                continue;
        }
        snprintf(add_paths[add_count], sizeof(add_paths[0]), "%s", path);
        add_count++;
    }
    closedir(d);
}

static void hide_add_list(void)
{
    if (addw && addw->shown())
        addw->hide();
    chooser_open = 0;
}

static void add_to_playlist(const char *path)
{
    const char *base;
    int i;

    if (!path || !path[0])
        return;
    keep_only_local_items();
    for (i = 0; i < pl_count; i++) {
        if (strcmp(pl_urls[i], path) == 0) {
            pl_cur = i;
            save_pl_index();
            start_local_file(path);
            return;
        }
    }
    if (pl_count >= MAXPL)
        pl_count = MAXPL - 1;
    snprintf(pl_urls[pl_count], sizeof(pl_urls[0]), "%s", path);
    base = strrchr(path, '/');
    snprintf(pl_titles[pl_count], sizeof(pl_titles[0]), "%s",
             base ? base + 1 : path);
    pl_cur = pl_count;
    pl_count++;
    queue_on = (pl_count > 1);
    save_pl_index();
    fprintf(stderr, "player-v27 add local %s n=%d\n", path, pl_count);
    start_local_file(path);
}

static void cb_add_pick(Fl_Widget *, void *)
{
    int n;

    if (!add_br)
        return;
    n = add_br->value();
    if (n < 1 || n > add_count)
        return;
    add_to_playlist(add_paths[n - 1]);
    show_playlist_panel();
    layout_bar();
}

static void open_audio_folder(void)
{
    int i, px, py, ph;
    const char *name;

    note_mouse();
    if (addw && addw->shown()) {
        hide_add_list();
        return;
    }
    hide_playlist();
    add_count = 0;
    add_scan_dir("/opt/minios/music");
    add_scan_dir("/home/tc/music");
    add_scan_dir(audio_start_dir());
    if (add_count <= 0) {
        snprintf(dl_lab, sizeof(dl_lab), "No audio files");
        refresh_ui();
        return;
    }
    px = 40;
    py = TITLE_H + 8;
    ph = dh - TITLE_H - BAR_H - 20;
    if (ph < 120)
        ph = 160;
    if (!addw) {
        addw = new Fl_Window(px, py, LIST_W, ph, "Add audio");
        addw->set_override();
        addw->border(0);
        addw->color(C_NAVY);
        add_br = new Fl_Hold_Browser(8, 28, LIST_W - 16, ph - 36);
        add_br->color(C_NAVY);
        add_br->textcolor(C_TEXT);
        add_br->selection_color(C_FACE_ON);
        add_br->textfont(FL_HELVETICA);
        add_br->textsize(14);
        add_br->callback(cb_add_pick);
        add_br->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
        {
            Fl_Box *hd = new Fl_Box(8, 4, LIST_W - 16, 22, "Media folder — click to add");
            hd->box(FL_NO_BOX);
            hd->labelfont(FL_HELVETICA_BOLD);
            hd->labelsize(13);
            hd->labelcolor(C_CYAN);
            hd->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        }
        addw->end();
    } else {
        addw->resize(px, py, LIST_W, ph);
        add_br->resize(8, 28, LIST_W - 16, ph - 36);
    }
    add_br->clear();
    for (i = 0; i < add_count; i++) {
        name = strrchr(add_paths[i], '/');
        add_br->add(name ? name + 1 : add_paths[i]);
    }
    chooser_open = 1;
    addw->show();
    if (add_br)
        add_br->take_focus();
    if (dpy && fl_xid(addw))
        XRaiseWindow(dpy, fl_xid(addw));
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

/* The second clip can still be writing its progress after the fail flag
   was set. Drop that flag when the progress line is a live download. */
static void tt_drop_stale_load(void)
{
    if (user_quit || tt_load_gave_up)
        return;
    read_progress();
    tt_watch_progress();
    if (!tt_download_on_screen())
        return;
    if (load_failed)
        load_failed = 0;
}

static void layout_bar(void)
{
    const int bw = (barw && barw->w() > 80) ? barw->w() : (card_w > 80 ? card_w - 16 : dw);
    const int top = 6;
    const int by = 28, bh = 28;
    const int ey = 60, eh = 20;
    int bx_seek = 54, bw_seek = bw - 54 - 56;
    int cx, x;

    if (bw_seek < 80)
        bw_seek = 80;
    if (lab_t0)
        lab_t0->resize(6, top, 48, 16);
    if (lab_t1)
        lab_t1->resize(bw - 52, top, 48, 16);
    if (seek_bar)
        seek_bar->resize(bx_seek, top + 2, bw_seek, 14);
    if (lab_st)
        lab_st->resize(8, by + 4, bw / 2 - 90, 20);

    cx = bw / 2;
    if (btn_prev) {
        btn_prev->resize(cx - 68, by, 36, bh);
        btn_prev->show();
    }
    if (btn_tog)
        btn_tog->resize(cx - 24, by - 2, 48, 32);
    if (btn_next) {
        btn_next->resize(cx + 32, by, 36, bh);
        btn_next->show();
    }

    x = 8;
    if (btn_auto) {
        btn_auto->resize(x, ey, 44, eh);
        if (queue_on)
            btn_auto->show();
        else
            btn_auto->hide();
        if (queue_on)
            x += 48;
    }
    if (btn_loop) {
        btn_loop->resize(x, ey, 44, eh);
        btn_loop->show();
    }
    x += 48;
    if (btn_add) {
        btn_add->resize(x, ey, 40, eh);
        btn_add->show();
    }
    x += 44;
    if (btn_list) {
        btn_list->resize(x, ey, 64, eh);
        btn_list->show();
    }
    if (btn_quit)
        btn_quit->resize(bw - 52, ey, 46, eh);
    if (btn_cc)
        btn_cc->resize(bw - 94, ey, 36, eh);
    if (btn_vp)
        btn_vp->resize(bw - 132, ey, 32, eh);
    if (btn_vm)
        btn_vm->resize(bw - 200, ey, 32, eh);
    if (lab_vol)
        lab_vol->resize(bw - 168, ey, 34, eh);
}

static char t0buf[16], t1buf[16], stbuf[96], volbuf[16];
static const char *loop_lab(void)
{
    if (loop_mode == 1)
        return "One";
    if (loop_mode == 2)
        return "All";
    return "Off";
}

static void refresh_ui(void)
{
    int loading;

    if (!barw)
        return;
    fmt_time(t0buf, sizeof(t0buf), tpos);
    fmt_time(t1buf, sizeof(t1buf), tlen);
    if (lab_t0)
        lab_t0->copy_label(t0buf);
    if (lab_t1)
        lab_t1->copy_label(t1buf);

    loading = (!have_playback &&
               (skip_busy || download_alive() || mpid > 0 || dl_lab[0]));
    if (loading) {
        read_progress();
        if (pl_count > 0)
            snprintf(stbuf, sizeof(stbuf), "Clip %d of %d   %s %d%%",
                     pl_cur + 1, pl_count,
                     dl_lab[0] ? dl_lab : "Loading media", dl_pct);
        else
            snprintf(stbuf, sizeof(stbuf), "%s %d%%",
                     dl_lab[0] ? dl_lab : "Loading media", dl_pct);
    } else if (!have_playback && mpid <= 0) {
        snprintf(stbuf, sizeof(stbuf), " ");
    } else if (paused && have_playback) {
        if (pl_count > 0)
            snprintf(stbuf, sizeof(stbuf), "Paused clip %d of %d",
                     pl_cur + 1, pl_count);
        else
            snprintf(stbuf, sizeof(stbuf), "Paused");
    } else if (pl_count > 0) {
        snprintf(stbuf, sizeof(stbuf), "Playing clip %d of %d",
                 pl_cur + 1, pl_count);
    } else {
        snprintf(stbuf, sizeof(stbuf), "Playing");
    }
    if (lab_st)
        lab_st->copy_label(stbuf);

    if (btn_tog) {
        btn_tog->kind = paused ? IC_PLAY : IC_PAUSE;
        btn_tog->color(paused ? C_FACE : C_FACE_ON);
        btn_tog->redraw();
    }
    if (btn_loop) {
        btn_loop->copy_label(loop_lab());
        btn_loop->lit = loop_mode != 0;
        btn_loop->redraw();
    }
    if (btn_auto) {
        btn_auto->lit = autoplay;
        btn_auto->redraw();
    }
    if (btn_cc) {
        btn_cc->lit = (cc_on && have_sub) || cc_fetching;
        btn_cc->redraw();
    }
    snprintf(volbuf, sizeof(volbuf), "%d", vol);
    if (lab_vol)
        lab_vol->copy_label(volbuf);
    if (seek_bar)
        seek_bar->redraw();
    if (loading &&
        (dl_pct != last_ui_dl || strcmp(dl_lab, last_ui_lab) != 0)) {
        last_ui_dl = dl_pct;
        snprintf(last_ui_lab, sizeof(last_ui_lab), "%s", dl_lab);
        if (loadw)
            loadw->redraw();
    }
}

static void cb_tog(Fl_Widget *, void *)
{
    if (!have_playback && !mpid)
        return;
    toggle_pause("toggle");
    refresh_ui();
}

static void cb_prev(Fl_Widget *, void *)
{
    start_pl_item(pl_cur - 1);
    refresh_ui();
}

static void cb_next(Fl_Widget *, void *)
{
    start_pl_item(pl_cur + 1);
    refresh_ui();
}

static void cb_auto(Fl_Widget *, void *)
{
    autoplay = !autoplay;
    refresh_ui();
}

static void cb_loop(Fl_Widget *, void *)
{
    loop_mode = (loop_mode + 1) % 3;
    apply_loop_to_player();
    refresh_ui();
}

static void open_file_browser(void)
{
    const char *p;
    const char *start;

    note_mouse();
    hide_playlist();
    hide_add_list();
    ungrab_keys();
    chooser_open = 1;
    if (loadw)
        loadw->hide();
    if (titlew)
        titlew->hide();
    if (barw)
        barw->hide();
    start = audio_start_dir();
    p = fl_file_chooser("Add a media file",
                        "*.{mp3,m4a,ogg,wav,aac,flac,mp4,mkv}",
                        start);
    chooser_open = 0;
    if (loadw)
        loadw->show();
    if (titlew)
        titlew->show();
    show_bar();
    if (p && p[0]) {
        add_to_playlist(p);
        show_playlist_panel();
    }
    layout_bar();
}

static void cb_add(Fl_Widget *, void *)
{
    mark_seen();
    open_file_browser();
}

static void cb_list(Fl_Widget *, void *)
{
    open_playlist();
}

static void cb_min(Fl_Widget *, void *)
{
    toggle_min();
}

static void cb_max(Fl_Widget *, void *)
{
    toggle_max();
}

static void cb_close(Fl_Widget *, void *)
{
    request_quit();
}

static void cb_vm(Fl_Widget *, void *)
{
    if (!have_playback && !mpid)
        return;
    set_vol(vol - 10);
    refresh_ui();
}

static void cb_vp(Fl_Widget *, void *)
{
    if (!have_playback && !mpid)
        return;
    set_vol(vol + 10);
    refresh_ui();
}

static void cb_cc(Fl_Widget *, void *)
{
    if (!have_playback && !mpid)
        return;
    do_cc();
    refresh_ui();
}

static void cb_quit(Fl_Widget *, void *)
{
    fprintf(stderr, "player-v17 quit click\n");
    request_quit();
}

static int sys_ev(void *event, void *)
{
    XEvent *e = (XEvent *)event;
    Window w;

    if (e && e->type == KeyPress) {
        KeySym ks = XLookupKeysym(&e->xkey, 0);
        unsigned m = e->xkey.state;
        if ((m & ControlMask) && (m & Mod1Mask) &&
            (ks == XK_Delete || ks == XK_KP_Delete || ks == XK_BackSpace)) {
            if (fork() == 0) {
                execl("/opt/minios/minios-cad", "minios-cad", (char *)0);
                execl("/usr/local/bin/minios-cad", "minios-cad", (char *)0);
                _exit(127);
            }
            return 1;
        }
        if (ks == XK_9 || ks == XK_KP_9 ||
            ks == XK_minus || ks == XK_underscore || ks == XK_KP_Subtract ||
            ks == XF86XK_AudioLowerVolume) {
            note_mouse();
            set_vol(vol - 10);
            refresh_ui();
            return 1;
        }
        if (ks == XK_0 || ks == XK_KP_0 ||
            ks == XK_plus || ks == XK_equal || ks == XK_KP_Add ||
            ks == XF86XK_AudioRaiseVolume) {
            note_mouse();
            set_vol(vol + 10);
            refresh_ui();
            return 1;
        }
        if (ks == XK_space) {
            note_mouse();
            toggle_pause("space-grab");
            refresh_ui();
            return 1;
        }
        if (ks == XK_m || ks == XK_M || ks == XF86XK_AudioMute) {
            note_mouse();
            send_cmd("mute\n");
            return 1;
        }
    }
    if (!e || e->type != ButtonPress)
        return 0;
    if (chooser_open)
        return 0;
    w = e->xbutton.window;
    if (barw && fl_xid(barw) && w == fl_xid(barw))
        return 0;
    if (titlew && fl_xid(titlew) && w == fl_xid(titlew))
        return 0;
    if (addw && addw->shown() && fl_xid(addw) && w == fl_xid(addw))
        return 0;
    if (shield && w == shield) {
        note_mouse();
        if (bar_hidden)
            show_bar();
        return 1;
    }
    if (loadw && fl_xid(loadw) && w == fl_xid(loadw)) {
        note_mouse();
        if (bar_hidden)
            show_bar();
        return 1;
    }
    if (listw && listw->shown() && fl_xid(listw) && w == fl_xid(listw))
        return 0;
    if (listw && listw->shown()) {
        hide_playlist();
        return 1;
    }
    if (mpwin && w != mpwin && !is_mplayer_window(w))
        return 0;
    if (have_playback || mpid > 0) {
        note_mouse();
        if (bar_hidden)
            show_bar();
        return 1;
    }
    return 0;
}

static int mplayer_alive(void)
{
    if (mpid <= 0)
        return 1;
    return kill(mpid, 0) == 0;
}

static Fl_Box *make_lab(int X, int Y, int W, int H, const char *L, int sz)
{
    Fl_Box *b = new Fl_Box(X, Y, W, H, L);
    b->box(FL_NO_BOX);
    b->labelcolor(C_TEXT);
    b->labelfont(FL_HELVETICA_BOLD);
    b->labelsize(sz);
    b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    return b;
}

int main(int argc, char **argv)
{
    FILE *pf;
    pid_t other = 0;
    unsigned long last = 0, started;
    int vh;

    {
        int a, asked_local = 0, asked_youtube = 0;
        read_player_mode();
        for (a = 1; a < argc; a++) {
            if (strcmp(argv[a], "--local") == 0)
                asked_local = 1;
            else if (strcmp(argv[a], "--youtube") == 0)
                asked_youtube = 1;
            else if (argv[a][0] >= '1' && argv[a][0] <= '9')
                mpid = (pid_t)atoi(argv[a]);
        }
        if (asked_local)
            local_mode = 1;
        else if (asked_youtube)
            local_mode = 0;
        else if (file_exists(SHORTF) || file_exists(TIKTOKF) || file_exists(PLF))
            local_mode = 0;
        write_player_mode();
    }
    signal(SIGPIPE, SIG_IGN);

    pf = fopen(BARPID, "r");
    if (pf) {
        if (fscanf(pf, "%d", &other) != 1)
            other = 0;
        fclose(pf);
    }
    if (other > 1 && other != getpid() && kill(other, 0) == 0) {
        if (file_exists(QUITF)) {
            kill(other, SIGTERM);
            {
                int w;
                for (w = 0; w < 25 && kill(other, 0) == 0; w++)
                    usleep(40000);
            }
            if (kill(other, 0) == 0)
                kill(other, SIGKILL);
            fprintf(stderr, "player-v27 took over quitting bar %d\n", (int)other);
        } else {
            FILE *rf = fopen(RELOADF, "w");
            if (rf) {
                fputs("1\n", rf);
                fclose(rf);
            }
            fprintf(stderr, "player-v27 already running pid=%d reload\n", (int)other);
            return 0;
        }
    }
    unlink(QUITF);
    if (mpid <= 0 && !download_alive()) {
        unlink(PROGF);
        dl_lab[0] = 0;
        dl_pct = 0;
    }
    pf = fopen(BARPID, "w");
    if (pf) {
        fprintf(pf, "%d\n", (int)getpid());
        fclose(pf);
    }

    Fl::scheme(0);
    Fl::visual(FL_RGB);
    fl_open_display();
    dpy = fl_display;
    if (!dpy) {
        fprintf(stderr, "player-v7 no display\n");
        return 1;
    }

    Fl_Tooltip::color(FL_BLACK);
    Fl_Tooltip::textcolor(C_CYAN);
    Fl_Tooltip::font(FL_HELVETICA);
    Fl_Tooltip::size(12);
    Fl_Tooltip::delay(0.35f);
    Fl_Tooltip::hoverdelay(0.20f);

    dw = DisplayWidth(dpy, DefaultScreen(dpy));
    dh = DisplayHeight(dpy, DefaultScreen(dpy));
    vh = dh - BAR_H - DECO_H;
    if (vh < 80)
        vh = dh / 2;
    load_playlist();
    if (shorts_on)
        clear_subs();
    mark_seen();
    card_defaults();
    write_geom();

    loadw = new LoadWin(0, 0, dw, dh);
    titlew = new TitleWin(0, 0, dw, TITLE_H);
    titlew->begin();
    btn_min = new IconBtn(dw - 132, 3, 32, 30, IC_MIN);
    btn_min->tooltip("Hide to a small bar. Click the bar to restore.");
    btn_min->callback(cb_min);
    btn_max = new IconBtn(dw - 96, 3, 32, 30, IC_MAX);
    btn_max->tooltip("Fill the screen or return to a window. F11 also works.");
    btn_max->callback(cb_max);
    btn_close = new IconBtn(dw - 58, 3, 50, 30, IC_CLOSE);
    btn_close->tooltip("Close the player");
    btn_close->color(C_QUIT);
    btn_close->callback(cb_close);
    titlew->end();
    barw = new BarWin(card_y + card_h - BAR_H - 4, card_w - 2 * CARD_PAD, BAR_H);
    barw->callback([](Fl_Widget *, void *) { request_quit(); });
    barw->begin();
    lab_t0 = make_lab(6, 6, 48, 16, "0:00", 12);
    lab_t1 = make_lab(card_w - 60, 6, 48, 16, "0:00", 12);
    lab_t1->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
    seek_bar = new SeekBar(54, 8, card_w - 120, 14);
    lab_st = make_lab(8, 32, card_w / 2 - 90, 20, " ", 12);
    btn_tog = new IconBtn(10, MENU_H + 42, 44, 32, IC_PLAY);
    btn_tog->tooltip("Play or pause");
    btn_tog->callback(cb_tog);
    btn_prev = new IconBtn(58, MENU_H + 42, 40, 32, IC_PREV);
    btn_prev->tooltip("Previous clip");
    btn_prev->callback(cb_prev);
    btn_next = new IconBtn(102, MENU_H + 42, 40, 32, IC_NEXT);
    btn_next->tooltip("Next clip. Player stays open.");
    btn_next->callback(cb_next);
    btn_auto = new TextBtn(150, MENU_H + 42, 52, 32, "Auto");
    btn_auto->tooltip("Play the next clip when this one ends");
    btn_auto->callback(cb_auto);
    btn_loop = new TextBtn(206, MENU_H + 42, 52, 32, "Off");
    btn_loop->tooltip("Off, then One (this file), then All (playlist).");
    btn_loop->callback(cb_loop);
    btn_add = new TextBtn(262, MENU_H + 42, 48, 32, "Add");
    btn_add->tooltip("Browse for a media file to add and play");
    btn_add->callback(cb_add);
    btn_list = new TextBtn(314, MENU_H + 42, 68, 32, "Playlist");
    btn_list->tooltip("Show the playlist. Save, delete, or clear rows.");
    btn_list->callback(cb_list);
    lab_vol = make_lab(dw - 196, MENU_H + 20, 78, 18, "80", 12);
    lab_vol->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    btn_vm = new TextBtn(dw - 196, 42, 36, 32, "-");
    btn_vm->tooltip("Volume down. Keyboard - also works.");
    btn_vm->callback(cb_vm);
    btn_vp = new TextBtn(dw - 154, 42, 36, 32, "+");
    btn_vp->tooltip("Volume up. Keyboard + also works.");
    btn_vp->callback(cb_vp);
    btn_cc = new TextBtn(dw - 112, 42, 40, 32, "CC");
    btn_cc->tooltip(shorts_on ? "Click to download captions" : "Captions on or off");
    btn_cc->callback(cb_cc);
    btn_quit = new TextBtn(dw - 64, 42, 52, 32, "Quit");
    btn_quit->tooltip("Close the player");
    btn_quit->color(C_QUIT);
    btn_quit->callback(cb_quit);
    barw->end();
    apply_card(0);
    layout_bar();

    loadw->show();
    titlew->show();
    barw->show();
    apply_card(0);
    if (btn_tog)
        btn_tog->take_focus();
    Fl::add_system_handler(sys_ev, 0);
    dismiss_popup();
    grab_keys();
    refresh_ui();
    if (mpid > 0) {
        have_playback = 1;
        if (!pause_user)
            paused = 0;
        tt_begin_bar_clip();
        restack_play();
    }

    {
        FILE *gf = fopen(GENF, "w");
        if (gf) {
            fprintf(gf, "%d\n", play_gen);
            fclose(gf);
        }
        unlink(DONEF);
    }
    started = now_ms();
    last_mouse = now_ms();
    fprintf(stderr, "player-v39 loop-off-one-all screen=%dx%d shorts=%d queue=%d n=%d loop=%d local=%d\n",
            dw, dh, shorts_on, queue_on, pl_count, loop_mode, local_mode);
    fprintf(stderr, "player-v18 screen ready\n");
    if (!local_mode && pl_count > 0 && mpid <= 0 &&
        (file_exists(TIKTOKF) || file_exists(SHORTF) || file_exists(PLF)))
        start_pl_item(pl_cur);

    while (!user_quit) {
        unsigned long now;

        read_pid_files();
        tt_hide_false_load();
        tt_drop_stale_load();
        if (file_exists(QUITF))
            request_quit();
        if (skip_busy && mpid > 0 && kill(mpid, 0) == 0) {
            skip_busy = 0;
            have_playback = 1;
            if (!pause_user)
                paused = 0;
            tt_begin_bar_clip();
            if (!play_alive_since)
                play_alive_since = now_ms();
            tt_note_play_started();
            tt_commit_volume();
            blank_load();
            restack_play();
            grab_keys();
            apply_loop_to_player();
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
            tt_show_bar_time(now);
            if (shorts_on && !file_exists(TIKTOKF) && tlen > 180 &&
                have_playback && !skip_busy && pl_count > 0 &&
                !is_local_url(pl_urls[pl_cur])) {
                int next = pl_cur;
                fprintf(stderr, "player-v33 skip long short tlen=%d idx=%d\n",
                        tlen, pl_cur);
                drop_pl_item(pl_cur);
                tlen = 0;
                if (pl_count > 0)
                    start_pl_item(next < pl_count ? next : 0);
                continue;
            }
            if (file_exists(RELOADF)) {
                unlink(RELOADF);
                tt_resume_after_more();
                tt_unmap_search();
                read_player_mode();
                if (file_exists(TIKTOKF) || file_exists(SHORTF) || file_exists(PLF))
                    local_mode = 0;
                write_player_mode();
                load_playlist();
                apply_card(1);
                write_geom();
                layout_bar();
                show_bar();
                if (titlew && !titlew->shown() && !win_min)
                    titlew->show();
                grab_keys();
                restack_play();
                fprintf(stderr, "player-v37 reload mode=%s n=%d shorts=%d\n",
                        local_mode ? "local" : "youtube", pl_count, shorts_on);
                if (!local_mode && pl_count > 0 && mpid <= 0)
                    start_pl_item(pl_cur);
            }
            if (!tt_end_hold) {
                tt_hold_player_keys(now);
                tt_note_clip_title();
                tt_note_playlist_title();
                refresh_ui();
            } else {
                static int tt_hold_logged;
                if (!tt_hold_logged) {
                    tt_hold_logged = 1;
                    fprintf(stderr, "player-v235 hold\n");
                }
            }
            tt_hold_list_quiet();
            tt_keep_end_popup();
            last = now;
            if (!tt_end_hold && have_playback && !skip_busy && !bar_hidden &&
                now - last_mouse > 3500UL)
                hide_bar();
            if (!tt_end_hold && !(tt_endw && tt_endw->shown()) && bar_hidden && dpy) {
                Window rw, cw;
                int rx = 0, ry = 0, wx, wy;
                unsigned mask = 0;
                if (XQueryPointer(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                  &rw, &cw, &rx, &ry, &wx, &wy, &mask)) {
                    if (last_ptr_x < 0) {
                        last_ptr_x = rx;
                        last_ptr_y = ry;
                    } else if (rx != last_ptr_x || ry != last_ptr_y) {
                        last_ptr_x = rx;
                        last_ptr_y = ry;
                        show_bar();
                    }
                }
            }
        }

        if (user_quit)
            break;
        /* Pid vanishing is not end-of-file. Only the done file from this
         * clip's mplayer (after Starting playback) advances the list. */
        if (take_clip_done())
            finish_current_clip();
        if (mpid > 0 && !mplayer_alive()) {
            mpid = 0;
            if (!skip_busy && !download_alive() && !clip_done &&
                !file_exists(DONEF))
                fprintf(stderr, "player-v21 mplayer died without done file\n");
        }
        if (mpid <= 0 && !tt_load_gave_up &&
            (skip_busy || download_alive()) && tt_load_since &&
            now - tt_load_since > 240000UL) {
            fprintf(stderr, "player-v14 load timeout\n");
            tt_load_gave_up = 1;
            load_failed = 1;
            if (loadw)
                loadw->redraw();
        }
        Fl::wait(0.15);
    }

    send_cmd("quit\n");
    unlink(GEOMF);
    unlink(PIDF);
    unlink(BARPID);
    if (dpy)
        XUngrabKeyboard(dpy, CurrentTime);
    return 0;
}
