/*
 * Copyright (C) 2025-2026 - FLtube
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

#include "../include/gnugettext_utils.h"
#include "../include/FLTube.h"
#include "../include/FLTube_View.h"
#include "../include/fltube_utils.h"
#include "../include/ytdlp_helper.h"
#include "../include/configuration_manager.h"
#include "../include/userdata_manager.h"
#include "../include/cache.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <FL/fl_draw.H>
#include <FL/Fl_Box.H>

/** Main Fltube window. */
FLTubeMainWindow* mainWin =  (FLTubeMainWindow *)0;

/** Help window. */
HelpFLTubeWindow* helpWin = (HelpFLTubeWindow*) 0;

/** Generic message window.*/
TinyMessageWindow* message_window = (TinyMessageWindow *)0;

InitialLoadingWindow* initial_win = (InitialLoadingWindow*)0;

// Default resolution for video streaming
VCODEC_RESOLUTIONS STREAM_VIDEO_RESOLUTION = R360p;

// Array that holds the search results WIDGETS, in groups of size @PaginationManager::SEARCH_PAGE_SIZE...
std::array <VideoInfo*, PaginationManager::SEARCH_PAGE_SIZE> video_info_arr{ nullptr, nullptr, nullptr, nullptr };

// Pointer to the video currently selected for stream...
VideoInfo* video_selected_for_stream = nullptr;

// Array that holds the search results METADATA, in groups of size @PaginationManager::SEARCH_PAGE_SIZE...
std::array <YTDLP_Video_Metadata*, PaginationManager::SEARCH_PAGE_SIZE> video_metadata{ nullptr, nullptr, nullptr, nullptr };

const std::string FLTUBE_TEMPORAL_DIR(std::filesystem::temp_directory_path().generic_string() + "/fltube_tmp_files/");

//This FLAG changes over the application usage. If user wants to see all videos of a channel, is set to true (0). Defaults to false.
bool SEARCH_BY_CHANNEL_F = false;

/* MiniOS Shorts search. When on, search uses the official Shorts filter and duration. */
bool SHORTS_SEARCH_F = false;
/* MiniOS TikTok search. Results play in the MiniOS player. */
bool TIKTOK_SEARCH_F = false;

std::string USER_CONFIGFILE_PATH = std::string(getHomePathOr("")) + "/.config/fltube/fltube.conf";

std::string USERDATA_FILE_PATH = std::string(getHomePathOr("")) + "/.local/share/fltube/userdata.txt";

std::string SYSTEM_CONFIGFILE_PATH = "/usr/local/etc/fltube/fltube.conf";

std::string CONFIGFILE_PATH = "";

// Path to fltube resources, like images, sounds, etc. Can be modified at fltube.conf file.
std::string RESOURCES_PATH = "/usr/local/share/fltube/resources";

//If value is true, enable the debug mode. Defaults to false.
bool DEBUG_ENABLED = false;

//If value is 0, enable the initial verifications. Defaults to false (or 1).
bool AVOID_INITIAL_CHECKS = 1;

//If true, then video Navigation History is enabled.
bool NAVIGATION_HISTORY_ENABLED = true;

//If true, ask every time the resolution is changed.
bool KEEP_ASKING_FOR_RESOLUTION_CHANGE = true;

// This variable holds the configured video codec used when download a video.
std::string DOWNLOAD_VIDEO_CODEC;

const std::string TAB_SEARCH_NAME = "SEARCHVIDEOS_TAB";
const std::string TAB_VIDEOLIST_NAME = "VIDEOLISTS_TABS";

/* Shorts use youtube.com/shorts/ID, not youtu.be/ID. Strip every common prefix. */
static std::string youtube_id_from_url(const std::string &url)
{
    std::string id = url;
    const char *pats[] = {
        "https://www.youtube.com/shorts/",
        "http://www.youtube.com/shorts/",
        "https://youtube.com/shorts/",
        "http://youtube.com/shorts/",
        "https://www.youtube.com/watch?v=",
        "http://www.youtube.com/watch?v=",
        "https://youtube.com/watch?v=",
        "http://youtube.com/watch?v=",
        "https://youtu.be/",
        "http://youtu.be/",
        "https://www.tiktok.com/@_/video/",
        "https://www.tiktok.com/",
        nullptr
    };
    for (int i = 0; pats[i]; i++) {
        size_t n = strlen(pats[i]);
        if (id.compare(0, n, pats[i]) == 0) {
            id = id.substr(n);
            break;
        }
    }
    size_t vid = id.rfind("/video/");
    if (vid != std::string::npos)
        id = id.substr(vid + 7);
    size_t cut = id.find_first_of("?&/#");
    if (cut != std::string::npos)
        id = id.substr(0, cut);
    return id;
}

// This URL always known the last version released of this program.
const std::string FLTUBE_UPSTREAM_VERSION_URL = "https://gitlab.com/facuA/fltube/-/raw/master/VERSION";

MediaPlayerInfo* media_player;

ConfigurationManager* config = nullptr;

UserDataManager* userdata = nullptr;

std::shared_ptr<YtDlp_Helper> ytdlp = nullptr;

std::shared_ptr<PermanentDiskCache> cache = nullptr;

PaginationManager* page_manager = nullptr;

Fl_PNG_Image* live_image = nullptr;
Fl_PNG_Image* already_viewed_image = nullptr;
Fl_PNG_Image* like_icon_image = nullptr;
Fl_PNG_Image* like_red_icon_image = nullptr;
Fl_PNG_Image* playicon_image = nullptr;
Fl_PNG_Image* cached_icon_image = nullptr;
Fl_PNG_Image* remove_video_icon_image = nullptr;
Fl_PNG_Image* watchlater_icon_image = nullptr;
Fl_PNG_Image* watchlater_filled_icon_image = nullptr;
Fl_PNG_Image* arrow_up = nullptr;
Fl_PNG_Image* arrow_down = nullptr;

/* Keep the current displayed cursor. FLTK doesn't have a way to know this. */
Fl_Cursor current_displayed_cursor = FL_CURSOR_DEFAULT;

std::shared_ptr<TerminalLogger> logger;

std::atomic<bool> ytdlp_action_in_progress(false);

std::atomic<bool> SHOWING_LOADING_SCREEN_F(false);

static Fl_Box* load_shade = nullptr;

static void hide_fltube_shade() {
    if (load_shade != nullptr) {
        load_shade->hide();
    }
    if (mainWin != nullptr) {
        mainWin->activate();
        mainWin->redraw();
    }
}

static void start_minios_player() {
    /* MiniOS Media Player, not this YouTube window.
     * minios-yt-arm is YouTube-only. It starts the same download the
     * playlist button starts. Do not use pgrep -f here: that command
     * matches its own shell and skips starting the player. */
    {
        FILE *tf = fopen("/tmp/minios-mplayer.log", "a");
        if (tf) {
            fputs("no-shade-player\nsearch-row-v152\nyt-arm-v156\n", tf);
            fclose(tf);
        }
    }
    if (access("/opt/minios/minios-yt-arm", X_OK) == 0)
        system("nohup /opt/minios/minios-yt-arm >>/tmp/minios-mplayer.log 2>&1 &");
    else if (access("/usr/local/bin/minios-yt-arm", X_OK) == 0)
        system("nohup /usr/local/bin/minios-yt-arm >>/tmp/minios-mplayer.log 2>&1 &");
    else if (access("/opt/minios/minios-player-bar", X_OK) == 0)
        system("nohup /opt/minios/minios-player-bar --youtube >>/tmp/minios-mplayer.log 2>&1 &");
    else if (access("/usr/local/bin/minios-player-bar", X_OK) == 0)
        system("nohup /usr/local/bin/minios-player-bar --youtube >>/tmp/minios-mplayer.log 2>&1 &");
}

static const int DOCK_H = 56;
/* flwm draws the title, minimize, and close on the left of the frame. */
static const int FRAME_L = 40;
static const int THUMB_W = 95;
static const int THUMB_H = 80;

static void place_result_row(VideoInfo *row, int x0, int y0, int width, int height) {
    if (row == nullptr || width < 420 || height < 80)
        return;
    /* Group resize scales every child. Put the thumbnail back to a
     * fixed size, then let only the title and channel use the spare width. */
    row->resizable(NULL);
    row->resize(x0, y0, width, height);
    int thumb_w = THUMB_W;
    int thumb_h = THUMB_H;
    int tx = x0 + 8;
    int ty = y0 + (height - thumb_h) / 2;
    if (ty < y0 + 4)
        ty = y0 + 4;
    if (row->thumbnail)
        row->thumbnail->resize(tx, ty, thumb_w, thumb_h);
    if (row->thumbnail_overlay)
        row->thumbnail_overlay->resize(tx, ty, thumb_w, thumb_h);
    int text_x = tx + thumb_w + 10;
    int text_r = x0 + width - 10;
    int text_w = text_r - text_x;
    if (text_w < 80)
        text_w = 80;
    if (row->title)
        row->title->resize(text_x, y0 + 4, text_w, 34);
    if (row->remove_bttn)
        row->remove_bttn->resize(text_r - 16, y0 + 6, 16, 16);
    if (row->views_spectators)
        row->views_spectators->resize(text_x, y0 + 40, 120, 20);
    if (row->is_live_image)
        row->is_live_image->resize(text_x + 124, y0 + 40, 40, 20);
    if (row->already_viewed_icon)
        row->already_viewed_icon->resize(text_x + 168, y0 + 40, 20, 20);
    if (row->like_icon_bttn)
        row->like_icon_bttn->resize(text_x + 194, y0 + 40, 20, 20);
    if (row->watch_later_bttn)
        row->watch_later_bttn->resize(text_x + 218, y0 + 40, 20, 20);
    if (row->cache_bttn)
        row->cache_bttn->resize(text_r - 22, y0 + 40, 20, 20);
    if (row->duration)
        row->duration->resize(text_x, y0 + height - 24, 72, 20);
    if (row->uploadDate)
        row->uploadDate->resize(text_x + 78, y0 + height - 24, 110, 20);
    if (row->userUploader)
        row->userUploader->resize(text_x + 194, y0 + height - 24, text_w - 194, 20);
    row->init_sizes();
}

static void layout_fltube_chrome(FLTubeMainWindow *w) {
    if (w == nullptr)
        return;
    int sw = w->w();
    int sh = w->h();
    if (sw < 640 || sh < 400)
        return;
    const int margin = 8;
    int inner = sw - margin * 2;
    if (w->about_bttn)
        w->about_bttn->resize(sw - margin - 64, 6, 64, 22);
    if (w->options_menu)
        w->options_menu->resize(sw - margin - 64 - 8 - 80, 6, 80, 22);
    if (w->central_tabs)
        w->central_tabs->resize(margin, 28, inner, 92);
    if (w->searchbox_tab)
        w->searchbox_tab->resize(margin, 56, inner, 64);
    int input_x = 110;
    int arrows = 18;
    int input_w = sw - margin - arrows - 8 - input_x;
    if (input_w < 220)
        input_w = 220;
    if (w->search_term_or_url)
        w->search_term_or_url->resize(input_x, 63, input_w, 24);
    if (w->next_search_term_bttn)
        w->next_search_term_bttn->resize(input_x + input_w + 4, 60, arrows, 14);
    if (w->prev_search_term_bttn)
        w->prev_search_term_bttn->resize(input_x + input_w + 4, 76, arrows, 14);
    int go_x = (sw - 230) / 2;
    if (w->do_search_bttn)
        w->do_search_bttn->resize(go_x, 90, 130, 22);
    if (w->shorts_bttn)
        w->shorts_bttn->resize(go_x + 140, 90, 90, 22);
    if (w->videolists_tab)
        w->videolists_tab->resize(margin, 56, inner, 64);
    if (w->videolist_selector)
        w->videolist_selector->resize(89, 63, inner - 110, 24);
    int pag_h = 30;
    int pag_y = sh - margin - pag_h;
    int res_y = 126;
    int res_h = pag_y - res_y - 6;
    if (res_h < 160)
        res_h = 160;
    if (w->search_result_selectors)
        w->search_result_selectors->resize(margin, res_y, inner, res_h);
    if (w->pagination_controls)
        w->pagination_controls->resize(margin, pag_y, inner, pag_h);
    if (w->first_page_bttn)
        w->first_page_bttn->resize(margin + 2, pag_y + 2, 18, 23);
    if (w->previous_results_bttn)
        w->previous_results_bttn->resize(margin + 24, pag_y + 2, 115, 23);
    if (w->last_page_bttn)
        w->last_page_bttn->resize(sw - margin - 18, pag_y + 2, 18, 23);
    if (w->next_results_bttn)
        w->next_results_bttn->resize(sw - margin - 18 - 6 - 115, pag_y + 2, 115, 23);
    if (w->pagination_status_info)
        w->pagination_status_info->resize((sw - 115) / 2, pag_y + 4, 115, 23);
    int row_w = inner - 16;
    int row_x = margin + 8;
    int row_y = res_y + 8;
    int slots = (int)video_info_arr.size();
    int row_h = 90;
    if (slots > 0 && res_h > 90)
        row_h = (res_h - 12) / slots;
    if (row_h < 90)
        row_h = 90;
    if (row_h > 120)
        row_h = 120;
    for (int i = 0; i < slots; i++)
        place_result_row(video_info_arr[i], row_x, row_y + i * row_h, row_w, row_h);
    if (w->no_videos_list_warn)
        w->no_videos_list_warn->resize(margin + 20, res_y + res_h / 2 - 36, inner - 40, 72);
}

void FLTubeMainWindow::resize(int X, int Y, int W, int H) {
    int limit_w = Fl::w();
    int limit_h = Fl::h();
    /* flwm draws minimize and close on the left edge of the frame.
     * x=0 puts those buttons off the screen. */
    if (limit_w > 200 && X < FRAME_L) {
        W -= (FRAME_L - X);
        X = FRAME_L;
    }
    if (limit_w > 200 && X + W > limit_w - 4)
        W = limit_w - 4 - X;
    if (limit_h > 320 && Y + H > limit_h - DOCK_H)
        H = limit_h - DOCK_H - (Y > 0 ? Y : 0);
    if (W < 640)
        W = 640;
    if (H < 400)
        H = 400;
    Fl_Double_Window::resize(X, Y, W, H);
    layout_fltube_chrome(this);
}

static void fit_search_to_screen(FLTubeMainWindow *w) {
    if (w == nullptr)
        return;
    int sw = Fl::w();
    int sh = Fl::h();
    if (sw < 640)
        sw = 1024;
    if (sh < 480)
        sh = 768;
    int ww = sw - FRAME_L - 4;
    int wh = sh - DOCK_H - 4;
    w->resize(FRAME_L, 4, ww, wh);
}

static void show_fltube_shade() {
    /* Do not paint a loading layer over MiniOS YouTube. The MiniOS
     * Media Player shows download status. no-shade-player */
    start_minios_player();
    if (mainWin != nullptr)
        mainWin->redraw();
}

static void import_minios_seen(void)
{
    FILE *f;
    char line[320], id[80], title[200];
    char *bar;

    f = fopen("/tmp/minios-yt-seen", "r");
    if (!f || !userdata)
        return;
    while (fgets(line, sizeof(line), f)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r'))
            line[--L] = 0;
        if (!L)
            continue;
        bar = strchr(line, '|');
        if (bar) {
            *bar = 0;
            snprintf(id, sizeof(id), "%s", line);
            snprintf(title, sizeof(title), "%s", bar + 1);
        } else {
            snprintf(id, sizeof(id), "%s", line);
            snprintf(title, sizeof(title), "%s", line);
        }
        if (strstr(id, "tiktok") || strstr(title, "tiktok.com"))
            continue;
        if (userdata->getHistoryList() &&
            userdata->getHistoryList()->findVideoById(id) != nullptr)
            continue;
        Video *v = new Video(id, title, "YouTube", "", "", "", "");
        userdata->addVideo(v, UserDataManager::HISTORY_LIST_NAME);
    }
    fclose(f);
}

static void poll_stream_done(void*) {
    import_minios_seen();
    if (!ytdlp_action_in_progress) {
        hide_fltube_shade();
        if (access("/tmp/minios-yt-shorts", F_OK) == 0)
            Fl::repeat_timeout(1.0, poll_stream_done);
        return;
    }
    Fl::repeat_timeout(0.25, poll_stream_done);
}

/**
 * Callback for main window close action. By default, exit app with success status code (0).
 */
void exitApp(unsigned short int exitStatusCode = FLT_OK) {
    /* Start the same RAM, zram, and swap release TikTok uses, before
     * cleanup. setsid keeps it alive after this window exits. */
    system("if [ -x /opt/minios/minios-gui-close ]; then G=/opt/minios/minios-gui-close; "
           "elif [ -x /usr/local/bin/minios-gui-close ]; then G=/usr/local/bin/minios-gui-close; "
           "else G=; fi; "
           "if [ -n \"$G\" ]; then "
           "if command -v setsid >/dev/null 2>&1; then "
           "setsid \"$G\" youtube </dev/null >/dev/null 2>&1 & "
           "else \"$G\" youtube </dev/null >/dev/null 2>&1 & fi; fi");
    if (ytdlp_action_in_progress) {
        logger->info(_("The main window was closed by user demand when a video was in streaming."));
    }
    //Do some cleaning...
    char message[1024];
    snprintf(message, sizeof(message), _("Cleaning temporal files at %s."), FLTUBE_TEMPORAL_DIR.c_str());
    logger->info(std::string(message));
    std::filesystem::remove_all(FLTUBE_TEMPORAL_DIR);
    if (helpWin != nullptr) delete helpWin;
    if (message_window != nullptr) delete message_window;
    delete userdata;
    cache->finish();
    delete page_manager;
    delete mainWin;
    //Exiting the app...
    logger->info(_("Closing FLtube... Bye!\n"));
    exit(exitStatusCode);
}

/**
 * Hide the target window.
 */
void closeWindow_cb(Fl_Widget*, Fl_Window *targetWindow) {
    targetWindow->hide();
}

/**
 * Show a tiny window with a message.
 */
void showMessageWindow(const char* message, const char *title){
    if (message_window == nullptr) {
        message_window = new TinyMessageWindow();
        message_window->close_bttn->callback((Fl_Callback*)closeWindow_cb, (void*)(message_window));
        message_window->set_modal();
    }
    center_window(message_window);
    if (title != nullptr)  message_window->label(title);
    message_window->show();
    message_window->error_label->label(message);

    // Loop until the message window is closed...
    while (message_window->shown()) {
        Fl::wait();
    }
}

/** Show a choice window with a message. The user can accept or cancel to continue an action.
 *  @param acceptAlwaysFlag: a memory address to a boolean that tells if the user wants to continue see this window in the future.
 *
 * Returns @true only if the user accept the choice window.
 */
bool showChoiceWindow(const char* message, bool& keepShowingFlag) {
    bool choice_result = false;
    TinyChoiceWindow* choiceWindow = new TinyChoiceWindow();
    center_window(choiceWindow);
    choiceWindow->cancel_bttn->callback( [] (Fl_Widget* widget, void* data) {
        bool* accept_ch = static_cast<bool*>(data);
        *accept_ch = false;
        widget->parent()->hide();
    }, &choice_result);
    choiceWindow->accept_bttn->callback( [] (Fl_Widget* widget, void* data) {
        bool* accept_ch = static_cast<bool*>(data);
        *accept_ch = true;
        widget->parent()->hide();
    }, &choice_result);
    choiceWindow->warnme_again_check->callback( [] (Fl_Widget* widget, void* data) {
        bool* keep_showing_message_flag = static_cast<bool*>(data);
        Fl_Check_Button* ch_bttn = static_cast<Fl_Check_Button*>(widget);
        *keep_showing_message_flag = ! (ch_bttn->value());
    }, &keepShowingFlag);
    choiceWindow->set_modal();
    choiceWindow->show();
    choiceWindow->choice_label->label(message);

    // Loop until the message window is closed...
    while (choiceWindow->shown()) {
        Fl::wait();
    }
    delete choiceWindow;
    return choice_result;
}

Fl_PNG_Image* load_image(std::string path) {
    if (std::filesystem::exists(path)) {
        return new Fl_PNG_Image(path.c_str());
    } else {
        char message[1024];
        snprintf(message, sizeof(message), _("Resource image at '%s' cannot be loaded..."), path.c_str());
        logger->warn(std::string(message));
    }
    return nullptr;
}

/**
 * Load a PNG image from the resource directory, and return as a Fl_PNG_Image object.
 * If image not exists, a nullptr is returned.
 */
Fl_PNG_Image* load_resource_image(std::string image_filename) {
    std::string resource_image_path = RESOURCES_PATH + "/img/" + image_filename;
    return load_image(resource_image_path);
}

void openURI(const char* uri) {
    char errmsg[512];
    if ( !fl_open_uri(uri, errmsg, sizeof(errmsg)) ) {
        char warnmsg[768];
        sprintf(warnmsg, "Error: %s", errmsg);
        fl_alert("%s", warnmsg);
    }
}

void showInitialWindow(){
    initial_win = new InitialLoadingWindow();
    initial_win->show();
    SHOWING_LOADING_SCREEN_F = true;
    initial_win->fltube_logo->image(load_resource_image("loading.png"));
    center_window(initial_win);
}

/* Change the current window cursor to any valid Fl_Cursor. If no parameter passed, change to FL_CURSOR_DEFAULT. */
void change_cursor(Fl_Cursor new_cursor = FL_CURSOR_DEFAULT) {
    if (current_displayed_cursor != new_cursor) {
        mainWin->cursor(new_cursor);
        current_displayed_cursor = new_cursor;
        Fl::check();
    }
}

void showFLTubeHelpWindow(Fl_Widget* w) {
    if (helpWin == nullptr) {
        helpWin = new HelpFLTubeWindow(480, 352, _("MiniOS YouTube"));
        helpWin->callback([](Fl_Widget* w) { w->hide(); });
        helpWin->about_version->copy_label(_("search based on FLTube"));
        helpWin->hlp_fltube_img->image(load_image("/usr/local/share/icons/fltube/128x128.png"));
        helpWin->hlp_source_code->callback([](Fl_Widget*) {openURI("https://gitlab.com/facuA/fltube");});
        helpWin->hlp_fltk_link->callback([](Fl_Widget*) {openURI("https://www.fltk.org/");});
        helpWin->hlp_ytdlp_link->callback([](Fl_Widget*) {openURI("https://github.com/yt-dlp/yt-dlp");});
        helpWin->hlp_cpp_link->callback([](Fl_Widget*) {openURI("https://isocpp.org/");});
        helpWin->howtouse_txt->buffer(new Fl_Text_Buffer());
        helpWin->howtouse_txt->wrap_mode(Fl_Text_Display::WRAP_AT_COLUMN, 50);
        helpWin->howtouse_txt->buffer()->text(_("MiniOS YouTube searches and plays YouTube videos. Search based on FLTube. You can search by a word or paste a YouTube address.\n\nOnce the results are returned, you can watch any of these videos by clicking on their thumbnail. The stream resolution is 360p by default, but can be changed at fltube.conf configuration file.\n\n"
        "You can also view the latest videos by the creator of a specific video by clicking on their name (a button at the bottom right of each result).\n\nIf a search returns multiple results, they will be paginated, and you can navigate through them using the \"<Previous\" and \"Next>\" buttons at the bottom of the window. You can jump yo the first page or the last loaded page using the \"<l\" and \"l>\" buttons.\n\n"
        "Every time you watch a video, it is added to an existing list called \"Navigation History\". This list can be consulted using the user interface at the \"My Lists\" tab.\n\nBesides, you can mark a video as \"Liked\" pressing a button with heart shape. A video list called \"Liked\" will be generated to in order to consult it in the future.\n\n"
        "The application keeps a cache of the final URL of the latest viewed videos, in order to speed-up the video loading time. The duration of this cache is of 6 hours by default; passed that time, the cache entry will be invalidated and the video must be loaded again from Youtube servers.\n"));
        helpWin->shortcuts_txt->buffer(new Fl_Text_Buffer());
        helpWin->shortcuts_txt->wrap_mode(Fl_Text_Display::WRAP_AT_COLUMN, 50);
        char help_text_bffr[1024];
        snprintf(help_text_bffr, sizeof(help_text_bffr),
                 _("- Focus at search input: %s\n- Search videos: %s\n- Search term history: %s\n"
            "- Get previous results: %s\n- Get next results: %s\n- Open video 1: %s\n- Open video 2: %s\n"
            "- Open video 3: %s\n- Open video 4: %s\n- Search videos of Channel 1: %s\n- Search videos of Channel 2: %s\n"
            "- Search videos of Channel 3: %s\n- Search videos of Channel 4: %s\n- Show this Help Window: %s\n\n"
            "[NOTE] Default shortcuts can be modified at %s.\n"),
            config->getShortcutTextFor(SHORTCUTS::FOCUS_SEARCH).c_str(), "Alt + s | Intro", "Up and Down arrow keys", "Alt + p", "Alt + n",
            config->getShortcutTextFor(SHORTCUTS::FOCUS_VIDEO_1).c_str(), config->getShortcutTextFor(SHORTCUTS::FOCUS_VIDEO_2).c_str(),
            config->getShortcutTextFor(SHORTCUTS::FOCUS_VIDEO_3).c_str(), config->getShortcutTextFor(SHORTCUTS::FOCUS_VIDEO_4).c_str(),
            config->getShortcutTextFor(SHORTCUTS::FOCUS_CHANNEL_1).c_str(), config->getShortcutTextFor(SHORTCUTS::FOCUS_CHANNEL_2).c_str(),
            config->getShortcutTextFor(SHORTCUTS::FOCUS_CHANNEL_3).c_str(), config->getShortcutTextFor(SHORTCUTS::FOCUS_CHANNEL_4).c_str(),
            config->getShortcutTextFor(SHORTCUTS::SHOW_HELP).c_str(), CONFIGFILE_PATH.c_str());
        helpWin->shortcuts_txt->buffer()->text(help_text_bffr);
        helpWin->config_txt->buffer(new Fl_Text_Buffer());
        helpWin->config_txt->wrap_mode(Fl_Text_Display::WRAP_AT_COLUMN, 50);
        snprintf(help_text_bffr, sizeof(help_text_bffr),
                _("The current instance of FLTube is loading the configurations from %s file.\n\n*** EXAMPLES ***\n[*] CHANGE THE DEFAULT STREAM RESOLUTION TO 720p:\n - Uncomment or add this property, and set the desire resolution (240, 360, 480, 720 or 1080).\n      %s = 720\n\n[*] CHANGE THE VIDEO PLAYER TO mpv INSTEAD OF mplayer:\n - Add or uncomment the following properties:\n      %s = mpv\n      %s = --ao=pulse\n      %s = \n The last two params can be empty, but MUST be set uncomment if you change the video player.\n"),
                CONFIGFILE_PATH.c_str(), "STREAM_VIDEO_RESOLUTION", "STREAM_PLAYER_PATH", "STREAM_PLAYER_PARAMS", "STREAM_PLAYER_EXTRA_PARAMS_FOR_LIVE");
        helpWin->config_txt->buffer()->text(help_text_bffr);
        helpWin->authors_txt->buffer(new Fl_Text_Buffer());
        helpWin->authors_txt->wrap_mode(Fl_Text_Display::WRAP_AT_COLUMN, 50);
        snprintf(help_text_bffr, sizeof(help_text_bffr),
                 _("[*_ DEVELOPERS _*]\n  Facundo Gabriel Adorno (Argentina)\n    -Contact: adorno.facundo@gmail.com\n\n"
                     "[*_ CONTRIBUTORS _*]\n  Nicolas Longardi (Uruguay)\n   -Contact: nico@locosporlinux.com\n"));
        helpWin->authors_txt->buffer()->text(help_text_bffr);
    }
    center_window(helpWin);
    helpWin->show();
    // Loop until the message window is closed...
    while (helpWin->shown()) {
        Fl::wait();
    }
}


/**
 * Lock/unlock the following buttons: main search, pagination, preview and search channel's video.
 */
void lock_buttons(bool lock){
    if (lock) {
        mainWin->do_search_bttn->deactivate();
        if (mainWin->shorts_bttn)
            mainWin->shorts_bttn->deactivate();
        if (mainWin->tiktok_bttn)
            mainWin->tiktok_bttn->deactivate();
        mainWin->next_results_bttn->deactivate();
        mainWin->last_page_bttn->deactivate();
        mainWin->previous_results_bttn->deactivate();
        mainWin->first_page_bttn->deactivate();
        mainWin->next_search_term_bttn->deactivate();
        mainWin->prev_search_term_bttn->deactivate();
        for (int j=0; j < video_info_arr.size(); j++) {
            video_info_arr[j]->thumbnail->deactivate();
            video_info_arr[j]->userUploader->deactivate();
            video_info_arr[j]->like_icon_bttn->deactivate();
            video_info_arr[j]->cache_bttn->deactivate();
            if (video_info_arr[j]->remove_bttn->visible()) {
                video_info_arr[j]->remove_bttn->deactivate();
            }
        }
    } else {
        //unlock
        mainWin->do_search_bttn->activate();
        if (mainWin->shorts_bttn)
            mainWin->shorts_bttn->activate();
        if (mainWin->tiktok_bttn)
            mainWin->tiktok_bttn->activate();
        mainWin->next_results_bttn->activate();
        mainWin->last_page_bttn->activate();
        mainWin->previous_results_bttn->activate();
        mainWin->first_page_bttn->activate();
        mainWin->next_search_term_bttn->activate();
        mainWin->prev_search_term_bttn->activate();
        for (int j=0; j < video_info_arr.size(); j++) {
            video_info_arr[j]->thumbnail->activate();
            video_info_arr[j]->userUploader->activate();
            video_info_arr[j]->like_icon_bttn->activate();
            video_info_arr[j]->cache_bttn->activate();
            if (video_info_arr[j]->remove_bttn->visible()) {
                video_info_arr[j]->remove_bttn->activate();
            }
        }
    }
    Fl::check();
}

/** Callback to preview a video... */
void preview_video_cb(Fl_Button* widget, void* video_url){
    if (ytdlp_action_in_progress)
        return;
    if (! verify_network_connection()) {
        logger->warn(_("Your device is offline. Check your internet connection."));
        showMessageWindow( _("There seems that you don't have access to the Internet. "
        "Please, verify you network connection before proceed..."));
        return;
    }
    std::string* url = static_cast<std::string*>(video_url);
    if (url){
        VideoInfo *vi = static_cast<VideoInfo *>(widget->parent());
        video_selected_for_stream = vi;
        video_selected_for_stream->thumbnail_overlay->image(playicon_image);
        video_selected_for_stream->thumbnail_overlay->show();
        if (NAVIGATION_HISTORY_ENABLED) {
            //Registering view of current video at History List...
            std::string id = youtube_id_from_url(*url);
            bool added = false;
            for (YTDLP_Video_Metadata* ytv : video_metadata) {
                if (ytv->id == id) {
                    Video* v = new Video(id, ytv->title, ytv->creators, ytv->channel_id, ytv->viewers_count, ytv->duration, ytv->thumbnail_url);
                    userdata->addVideo(v, UserDataManager::HISTORY_LIST_NAME);
                    added = true;
                    break;
                }
            }
            if (!added && !id.empty()) {
                Video* v = new Video(id, id, "Shorts", "", "", "", "");
                userdata->addVideo(v, UserDataManager::HISTORY_LIST_NAME);
            }
        }
        //Stream video section...
        char message[256];
        snprintf(message, sizeof(message), _("Starting streaming preview of video '%s' - (%s)..."), vi->title->label(), url->c_str());
        logger->info(message);
        if (YtDlp_Helper::isTikTokURL(url->c_str())) {
            showMessageWindow(_("TikTok is not part of YouTube. Use the TikTok icon."));
            return;
        }
        if (ytdlp)
            ytdlp->write_playlist("/tmp/minios-yt-playlist", url->c_str());
        if (SHORTS_SEARCH_F) {
            FILE *flag = fopen("/tmp/minios-yt-shorts", "w");
            if (flag) {
                fputs("1\n", flag);
                fclose(flag);
            }
            unlink("/tmp/minios-yt-tiktok");
        } else {
            unlink("/tmp/minios-yt-shorts");
            unlink("/tmp/minios-yt-tiktok");
        }
        /* The player bar reads the playlist and runs minios-shorts-play.
         * That calls minios-yt-resolve, which saves video, audio, and
         * captions, then starts mplayer. Do not start a second download. */
        show_fltube_shade();
        ytdlp_action_in_progress = false;
        lock_buttons(false);
        change_cursor();
    } else {
        logger->error(_("Cannot get video URL. Review the video metadata enabling app debugging..."));
    }
}

/**
 * Hide VideoInfo widgets.
 */
void clear_video_info() {
    for (int j=0; j < video_info_arr.size(); j++) {
        video_info_arr[j]->hide();
    }
}

/**
 * Update the VideoInfo widgets data, based on the data on the video_metadata array.
 */
void update_video_info() {
    YTDLP_Video_Metadata* ytvm = nullptr;
    char text_buffer[128];
    bool is_livestream;
    /* Place rows before measuring the thumbnail. A scaled thumbnail
     * covers the start of the title. */
    layout_fltube_chrome(mainWin);
    char* tabname = static_cast<char*>(mainWin->central_tabs->value()->user_data());
    bool displaying_video_list_tab = (tabname == TAB_VIDEOLIST_NAME);
    for (int j=0; j < video_metadata.size(); j++) {
      if (video_metadata[j] != nullptr) {
            is_livestream = (video_metadata[j]->live_status == "is_live");
            video_info_arr[j]->show();
            video_info_arr[j]->title->label(video_metadata[j]->title.c_str());
            video_info_arr[j]->title->tooltip(video_metadata[j]->title.c_str());
            video_info_arr[j]->duration->label(video_metadata[j]->duration.c_str());
            video_info_arr[j]->uploadDate->label(video_metadata[j]->upload_date.c_str());
            video_info_arr[j]->userUploader->label(video_metadata[j]->creators.c_str());
            video_info_arr[j]->userUploader->user_data(static_cast<void*>(&video_metadata[j]->channel_id));
            if (is_livestream) {
                video_info_arr[j]->is_live_image->show();
            } else {
                video_info_arr[j]->is_live_image->hide();
            }
            // Update History icon...
            if (userdata->getHistoryList()->findVideoById(video_metadata[j]->id) != nullptr) {
                video_info_arr[j]->already_viewed_icon->show();
            } else {
                video_info_arr[j]->already_viewed_icon->hide();
            }
            // Update Liked icon...
            if (userdata->getLikedVideosList()->existAtList(video_metadata[j]->id)) {
                video_info_arr[j]->like_icon_bttn->image(like_red_icon_image);
            } else {
                video_info_arr[j]->like_icon_bttn->image(like_icon_image);
            }
            video_info_arr[j]->like_icon_bttn->redraw();
            // Update Watch Later icon...
            if (userdata->getWatchLaterVideoList()->existAtList(video_metadata[j]->id)) {
                video_info_arr[j]->watch_later_bttn->image(watchlater_filled_icon_image);
            } else {
                video_info_arr[j]->watch_later_bttn->image(watchlater_icon_image);
            }
            video_info_arr[j]->watch_later_bttn->redraw();
            // Update Cache icon
            if (cache->is_cached(ytdlp->getIdFor(video_metadata[j]->url))) {
                char cache_tooltip[128];
                std::snprintf(cache_tooltip, sizeof(cache_tooltip), _("Video URL Cached (valid until %s). Click to remove from cache."),
                              cache->get_cache_expiration_date(ytdlp->getIdFor(video_metadata[j]->url)).c_str());
                video_info_arr[j]->cache_bttn->copy_tooltip(cache_tooltip);
                video_info_arr[j]->cache_bttn->show();
            } else {
                video_info_arr[j]->cache_bttn->hide();
            }
            // If displaying the videos when the @TAB_VIDEOLIST_NAME is selected, then shows the "remove from list" button...
            if (displaying_video_list_tab) {
                video_info_arr[j]->remove_bttn->show();
                video_info_arr[j]->remove_bttn->activate();
            } else {
                video_info_arr[j]->remove_bttn->hide();
                video_info_arr[j]->remove_bttn->deactivate();
            }
            try {
                snprintf(text_buffer, sizeof(text_buffer), "%s %s", YtDlp_Helper::get_metric_abbreviation(std::stoi(video_metadata[j]->viewers_count))->c_str(), (is_livestream) ? _("viewers") : _("views"));
            } catch (const std::invalid_argument& ex) {
                char err_msg[256];
                snprintf(err_msg, sizeof(err_msg), _("Error when getting count of viewers of video: (title: \"%s\", ID: \"%s\")"), video_metadata[j]->title.c_str(), video_metadata[j]->id.c_str());
                logger->warn(err_msg);
                logger->debug(ex.what());
                strcpy(text_buffer, _("Unknown"));
            }
            video_info_arr[j]->views_spectators->copy_label(text_buffer);
            //Setting URL for streaming a video preview...
            video_info_arr[j]->thumbnail->user_data(static_cast<void*>(&video_metadata[j]->url));

            //Download, resize and update the video thumbnail image...
            int cut_pos = video_metadata[j]->thumbnail_url.find("hq720.jpg?");
            if (cut_pos == std::string::npos)   cut_pos = video_metadata[j]->thumbnail_url.find("hqdefault.jpg?");
            std::string thumbn_url = video_metadata[j]->thumbnail_url.substr(0, cut_pos)
                + ((cut_pos != std::string::npos) ? "mqdefault.jpg" : "");
            std::string thumbn_name = "th_" + video_metadata[j]->id + ".jpg";
            if (download_file(thumbn_url, FLTUBE_TEMPORAL_DIR, thumbn_name) != FLT_DOWNLOAD_FL_FAILED) {
                video_info_arr[j]->thumbnail->resize(
                    video_info_arr[j]->thumbnail->x(),
                    video_info_arr[j]->thumbnail->y(),
                    THUMB_W, THUMB_H);
                if (video_info_arr[j]->thumbnail_overlay) {
                    video_info_arr[j]->thumbnail_overlay->resize(
                        video_info_arr[j]->thumbnail->x(),
                        video_info_arr[j]->thumbnail->y(),
                        THUMB_W, THUMB_H);
                }
                Fl_Image* resized_thumbnail = create_resized_image_from_jpg(FLTUBE_TEMPORAL_DIR + thumbn_name, THUMB_W);
                if (resized_thumbnail && resized_thumbnail->h() > THUMB_H) {
                    int nw = resized_thumbnail->w() * THUMB_H / resized_thumbnail->h();
                    if (nw < 1)
                        nw = 1;
                    if (nw > THUMB_W)
                        nw = THUMB_W;
                    Fl_Image *fitted = resized_thumbnail->copy(nw, THUMB_H);
                    delete resized_thumbnail;
                    resized_thumbnail = fitted;
                }
                if (resized_thumbnail == nullptr) {
                    logger->error(_("Something went wrong when generating a resize thumbnail for video with ID=") + video_metadata[j]->id);
                } else {
                    delete video_info_arr[j]->thumbnail->image();
                    video_info_arr[j]->thumbnail->image(resized_thumbnail);
                    video_info_arr[j]->thumbnail->redraw();
                }
            } else {
                //If thumbnail cannot be downloaded, then load a generic one...
                Fl_PNG_Image* generic_thumbnail = load_resource_image("no_thumbnail_available.png");
                video_info_arr[j]->thumbnail->image(generic_thumbnail);
                video_info_arr[j]->thumbnail->redraw();
            }

        } else {
            // Hide the VideoInfo widget if there are no more results to show...
            video_info_arr[j]->hide();
        }
    }
}

/**
 * Update the elements in the video_metadata array using as input the videos in the selected list at @TAB_VIDEOLIST_NAME section.
 * This must be called for show the videos in a saved list (History, Liked, etc...).
 * Returns @true if there is more results to process at video list. Returns @false in otherwise.
 */
bool updateVideoMetadataFromVideoList() {
    Fl::check();
    std::string selected_list = mainWin->videolist_selector->mvalue()->label();
    VideoList* vlist = userdata->getVideoList(selected_list);
    Pagination_Info current_page = page_manager->current();
    int next_video_position = (current_page.index * current_page.size);
    // If list is empty...
    if (vlist->getLength() == 0)    mainWin->no_videos_list_warn->show();
    else    mainWin->no_videos_list_warn->hide();


    for (int i=0; i < PaginationManager::SEARCH_PAGE_SIZE; i++) {
        if (next_video_position < vlist->getLength()) {
            video_metadata[i] = vlist->toYTDLPVideo(vlist->getVideoAt(next_video_position));
        } else {
            video_metadata[i] = nullptr;
        }
        next_video_position++;
    }

    update_video_info();

    // Update the displaying pagination info...
    std::string page_status = page_manager->print_pagination_info();
    mainWin->pagination_status_info->copy_label(page_status.c_str());

    if (next_video_position < vlist->getLength()) {
        mainWin->next_results_bttn->activate();
        mainWin->last_page_bttn->activate();
    }
    if (vlist->getLength() < next_video_position + 1){
        mainWin->next_results_bttn->deactivate();     // If the list doesn't have more elements to show, deactivate next button...'
        mainWin->last_page_bttn->deactivate();
        return false;
    }

    return true;
}

void getVideosAtList_cb(Fl_Choice* w, void* a){
    std::string selected_list = mainWin->videolist_selector->mvalue()->label();
    VideoList* vlist = userdata->getVideoList(selected_list);
    page_manager->reset();
    page_manager->limit(true);
    page_manager->set_max_results(vlist->getLength());
    mainWin->previous_results_bttn->deactivate();
    mainWin->first_page_bttn->deactivate();
    // Check if there is Internet connectivity before do a search...
    if (! verify_network_connection()) {
        logger->warn(_("Your device is offline. Check your internet connection."));
        showMessageWindow( _("There seems that you don't have access to the Internet. "
        "Please, verify you network connection before proceed..."));
        return;
    }
    updateVideoMetadataFromVideoList();
};

void selectCentralTab_cb(Fl_Choice* w, void* a){
    char* tabname = static_cast<char*>(mainWin->central_tabs->value()->user_data());
    if (tabname == TAB_VIDEOLIST_NAME) {
        mainWin->videolist_selector->value(mainWin->videolist_selector->find_index(UserDataManager::HISTORY_LIST_NAME.c_str()));
        mainWin->pagination_status_info->show();
        mainWin->videolist_selector->set_changed();
        mainWin->videolist_selector->do_callback();
    }
    if (tabname == TAB_SEARCH_NAME) {
        mainWin->search_term_or_url->value("");
        mainWin->previous_results_bttn->deactivate();
        mainWin->first_page_bttn->deactivate();
        mainWin->next_results_bttn->deactivate();
        mainWin->last_page_bttn->deactivate();
        mainWin->pagination_status_info->label("");
        mainWin->pagination_status_info->hide();
        mainWin->no_videos_list_warn->hide();
        ytdlp->resetSearchHistoryPos();
        page_manager->reset();
        clear_video_info();
    }
}

/*
 * Return the name of the current active tab at @central_tabs widget.
 */
std::string getActiveTabName() {
    char* tabname = static_cast<char*>(mainWin->central_tabs->value()->user_data());
    return std::string(tabname);
}

void markLikedVideo_cb(Fl_Widget *wdg) {
    VideoInfo* vi = static_cast<VideoInfo*>(wdg->parent());
    //Add current video to Liked List...
    std::string id = youtube_id_from_url(*static_cast<std::string*>(vi->thumbnail->user_data()));
    for (YTDLP_Video_Metadata* ytv : video_metadata) {
        if (ytv->id == id) {
            if ( userdata->getLikedVideosList()->existAtList(ytv->id)) {
                userdata->removeVideoFromList(ytv->id, UserDataManager::LIKED_LIST_NAME);
                vi->like_icon_bttn->image(like_icon_image);
            } else {
                Video* v = new Video(id, ytv->title, ytv->creators, ytv->channel_id, ytv->viewers_count, ytv->duration, ytv->thumbnail_url);
                userdata->addVideo(v, UserDataManager::LIKED_LIST_NAME);
                vi->like_icon_bttn->image(like_red_icon_image);
            }
            break;
        }
    }
}

void add_to_watch_later(Fl_Widget *wdg) {
    VideoInfo* vi = static_cast<VideoInfo*>(wdg->parent());
    std::string id = youtube_id_from_url(*static_cast<std::string*>(vi->thumbnail->user_data()));
    for (YTDLP_Video_Metadata* ytv : video_metadata) {
        if (ytv->id == id) {
            if ( userdata->getWatchLaterVideoList()->existAtList(ytv->id)) {
                userdata->removeVideoFromList(ytv->id, UserDataManager::WATCHLATER_LIST_NAME);
                vi->watch_later_bttn->image(watchlater_icon_image);
            } else {
                Video* v = new Video(id, ytv->title, ytv->creators, ytv->channel_id, ytv->viewers_count, ytv->duration, ytv->thumbnail_url);
                userdata->addVideo(v, UserDataManager::WATCHLATER_LIST_NAME);
                vi->watch_later_bttn->image(watchlater_filled_icon_image);
            }
            break;
        }
    }
}

void removeFromCache_cb(Fl_Widget *wdg) {
    VideoInfo* vi = static_cast<VideoInfo*>(wdg->parent());
    //Registering view of current video at History List...
    std::string video_url = *static_cast<std::string*>(vi->thumbnail->user_data());
    if (cache->is_cached(ytdlp->getIdFor(video_url))) {
        char mssg[256];
        snprintf(mssg, sizeof(mssg), _("The following cache was invalidated by user request: id=%s; expiration_date=%s."), ytdlp->getIdFor(video_url).c_str(), cache->get_cache_expiration_date(ytdlp->getIdFor(video_url)).c_str());
        if (cache->remove_entry(ytdlp->getIdFor(video_url))) logger->debug(mssg);
    }
    wdg->hide();
}

/* Callback to remove a video from a video list: (the History Navigation list, the Liked list, or any other custom list). */
void removeFromVideoList_cb(Fl_Widget *wdg) {
    // Avoid proceed the callback action if there is no video list displaying currently...
    if (getActiveTabName() != TAB_VIDEOLIST_NAME) return;

    VideoInfo* vi = static_cast<VideoInfo*>(wdg->parent());
    std::string video_url = *static_cast<std::string*>(vi->thumbnail->user_data());
    std::string video_id = youtube_id_from_url(video_url);
    std::string selected_list = mainWin->videolist_selector->mvalue()->label();
    if (userdata->removeVideoFromList(video_id, selected_list)) {
        char mssg[256];
        snprintf(mssg, sizeof(mssg), _("Remove video '%s' from list '%s'."), video_url.c_str(), selected_list.c_str());
        logger->debug(mssg);
        // Decrement the count of current list's videos at the pagination manager.
        page_manager->set_max_results(page_manager->get_count_results() - 1);
    }
    updateVideoMetadataFromVideoList();
}

/* Notify if a new version of FLTube is available for download... */
void check_fltube_update_cb (Fl_Widget* w, void* data) {
    change_cursor(FL_CURSOR_WAIT);
    std::string version_filename = "VERSION";
    if (!std::filesystem::exists(FLTUBE_TEMPORAL_DIR + version_filename)) {
        int result = download_file(FLTUBE_UPSTREAM_VERSION_URL, FLTUBE_TEMPORAL_DIR, version_filename);
        if (result != 0) {
            logger->error(_("For some reason, FLTUBE VERSION file for update verification cannot be downloaded."));
            return;
        }
    }
    std::string upstream_version;
    std::ifstream infile(FLTUBE_TEMPORAL_DIR + version_filename);
    std::getline(infile, upstream_version);
    trim(upstream_version);
    change_cursor();
    if (getIntVersion(upstream_version) > getIntVersion(VERSION)) {
        char message[256];
        snprintf(message, sizeof(message), _("FLTube has available a new version to download.\nNew version available: %s."), upstream_version.c_str());
        showMessageWindow(message, _("New version available!"));
    } else {
        showMessageWindow(_("You have the last version available."), _("You're up to date."));
    }
}

void change_stream_resolution_cb (Fl_Widget* w, void* data) {
    intptr_t new_resolution = reinterpret_cast<intptr_t>(data);
    int old_resolution = STREAM_VIDEO_RESOLUTION;
    bool has_changed = false;
    if (new_resolution == old_resolution) return;
    if (new_resolution > 360 && KEEP_ASKING_FOR_RESOLUTION_CHANGE) {
        bool proceed = showChoiceWindow(_("Changing the video stream resolution to higher than 360p may cause older laptops to freeze. Do you want to proceed?"), KEEP_ASKING_FOR_RESOLUTION_CHANGE);
        if (!proceed) {
            // Select again the previous resolution radio button...
            switch (old_resolution) {
                case R240p:   mainWin->quality_240_bttn->setonly(); break;
                case R360p:   mainWin->quality_360_bttn->setonly(); break;
                case R480p:   mainWin->quality_480_bttn->setonly(); break;
                case R720p:   mainWin->quality_720_bttn->setonly(); break;
                case R1080p:   mainWin->quality_1080_bttn->setonly(); break;
            }
            return;
        }
    }
    switch (new_resolution) {
        case R240p:   STREAM_VIDEO_RESOLUTION=R240p; has_changed=true; break;
        case R360p:   STREAM_VIDEO_RESOLUTION=R360p; has_changed=true; break;
        case R480p:   STREAM_VIDEO_RESOLUTION=R480p; has_changed=true; break;
        case R720p:   STREAM_VIDEO_RESOLUTION=R720p; has_changed=true; break;
        case R1080p:  STREAM_VIDEO_RESOLUTION=R1080p; has_changed=true; break;
    }
    if (has_changed) {
        ytdlp->set_resolution(STREAM_VIDEO_RESOLUTION);
        update_video_info();
        logger->debug(_("Stream video resolution updated by user to ") + std::to_string(new_resolution));
    }
}

/**
 * Hook: Actions to execute before main window is drawn...
 */
void pre_init() {
    ///// LOAD CONFIGURATIONS  //////
    // If not specified custom configuration file through parameter "--config"...
    if (CONFIGFILE_PATH.empty())
        CONFIGFILE_PATH = (std::filesystem::exists(USER_CONFIGFILE_PATH)) ? USER_CONFIGFILE_PATH : SYSTEM_CONFIGFILE_PATH;
    logger->debug(_("Loading configurations from ") + CONFIGFILE_PATH);
    initial_win->loading_about_data->label(_("Processing configuration file..."));
    config = new ConfigurationManager(CONFIGFILE_PATH.c_str(), logger);
    initial_win->loading_about_data->label(_("Processing user configuration file..."));
    userdata = new UserDataManager(USERDATA_FILE_PATH, getIntVersion(std::string(VERSION)), logger);
    page_manager = new PaginationManager();
    //TODO Create properties defining if enable the use of cache and the TTL for every cache entry...
    initial_win->loading_about_data->label(_("Populating cache..."));
    cache = std::make_shared<PermanentDiskCache>(logger);
    std::string default_cache_path = std::string(getHomePathOr("")) + "/.cache/fltube";
    cache->set_save_directory_path(
        config->getProperty("CACHE_PATH", default_cache_path.c_str()), "fltube_url_cache.txt");
    cache->init();
    AVOID_INITIAL_CHECKS = config->getBoolProperty("AVOID_INITIAL_VERIFICATIONS", false);

    initial_win->loading_about_data->label(_("Checking 'yt-dlp' installation..."));
    logger->info(_("Cheking if yt-dlp is at your system PATH..."));
    if ( !AVOID_INITIAL_CHECKS && !isInstalledYTDLP() ) {
        showMessageWindow(_("yt-dlp is not installed on your system or its binary is not at $PATH system variable. See how to install it at https://github.com/yt-dlp/yt-dlp. Or run in a terminal the 'install_yt-dlp' script."));
        logger->error(_("yt-dlp is not installed. Closing app...\n"));
        exitApp(FLT_GENERAL_FAILED);
    }
    //Init Localization. Use locale path specified at config, or custom config default_locale_path().
    setup_gettext("", config->getProperty("LOCALE_PATH", default_locale_path().c_str()));

    initial_win->loading_about_data->label(_("Configuring the application..."));
    if(config->existProperty("STREAM_PLAYER_PATH")) {
        media_player = new MediaPlayerInfo(
            config->getProperty("STREAM_PLAYER_PATH", ""),
                                           config->getProperty("STREAM_PLAYER_PARAMS", ""),
                                           config->getProperty("STREAM_PLAYER_EXTRA_PARAMS_FOR_LIVE", ""));
    } else {
        media_player = new MediaPlayerInfo(DEFAULT_STREAM_PLAYER, DEFAULT_PLAYER_PARAMS, DEFAULT_PLAYER_EXTRAPARAMS_LIVE);
    }
    if(config->existProperty("RESOURCES_PATH")) {
        RESOURCES_PATH = config->getProperty("RESOURCES_PATH", RESOURCES_PATH.c_str());
    }
    if (config->existProperty("STREAM_VIDEO_RESOLUTION")) {
        int pretended_resolution = config->getIntProperty("STREAM_VIDEO_RESOLUTION", DEFAULT_STREAM_VIDEO_RESOLUTION);
        std::vector<VCODEC_RESOLUTIONS> resolutions = {R240p, R360p, R480p, R720p, R1080p};
        for (auto res: resolutions){
            if (res == pretended_resolution) {
                STREAM_VIDEO_RESOLUTION = res;
                break;
            }
        }
        if (STREAM_VIDEO_RESOLUTION != DEFAULT_STREAM_VIDEO_RESOLUTION)
            logger->debug(_("Default streaming resolution (360p) changed at configuration to this new resolution: ") + std::to_string(STREAM_VIDEO_RESOLUTION));
    }

    initial_win->loading_about_data->label(_("Loading resources files..."));
    live_image = load_resource_image("livebutton_18p.png");
    already_viewed_image = load_resource_image("clock_18p.png");
    like_icon_image = load_resource_image("heart_18p.png");
    like_red_icon_image = load_resource_image("heart_18p_red.png");
    playicon_image = load_resource_image("playicon.png");
    cached_icon_image = load_resource_image("cache_icon_18p.png");
    remove_video_icon_image = load_resource_image("remove_video_icon_14p.png");
    watchlater_icon_image = load_resource_image("watch_later.png");
    watchlater_filled_icon_image = load_resource_image("watch_later_filled.png");
    arrow_up = load_resource_image("arrow_up.png");
    arrow_down = load_resource_image("arrow_down.png");

    //Create temporal directory and change current working directory to that dir.
    std::filesystem::create_directory(FLTUBE_TEMPORAL_DIR);
    std::filesystem::current_path(FLTUBE_TEMPORAL_DIR);

    bool enable_alt_stream = config->getBoolProperty("ENABLE_ALTERNATIVE_STREAM_METHOD", true);
    int batch_size = config->getIntProperty("PREFETCH_BATCH_RESULTS_SIZE", YtDlp_Helper::DEFAULT_MIN_BATCH_SIZE);
    ytdlp = std::make_shared<YtDlp_Helper>(STREAM_VIDEO_RESOLUTION, media_player, enable_alt_stream, logger, cache, FLTUBE_TEMPORAL_DIR, batch_size);

    if (initial_win) {
        SHOWING_LOADING_SCREEN_F = false;
    }

    // Add a custom FLTK event dispatcher
    Fl::event_dispatch([](int event, Fl_Window* w) -> int{
        if (ytdlp_action_in_progress) {
            // If streaming is in progress, then turn cursor to default if changed...
            if (current_displayed_cursor != FL_CURSOR_WAIT) lock_buttons(true);
            change_cursor(FL_CURSOR_WAIT);
            switch (event) {
                case FL_PUSH:
                case FL_RELEASE:
                case FL_DRAG:
                case FL_KEYDOWN:
                case FL_KEYUP:
                case FL_MOUSEWHEEL:
                    return 1;       // Ignore this events when streaming in course...
            }
        } else {
            if (current_displayed_cursor != FL_CURSOR_DEFAULT) {
                hide_fltube_shade();
                lock_buttons(false);
                if (video_selected_for_stream != nullptr && video_selected_for_stream->thumbnail_overlay->visible()) {
                    video_selected_for_stream->thumbnail_overlay->image(nullptr);
                    video_selected_for_stream->thumbnail_overlay->hide();
                    std::string* video_url = static_cast<std::string*>(video_selected_for_stream->thumbnail->user_data());
                    std::string id = youtube_id_from_url(*video_url);
                    if (video_url != nullptr && userdata->getHistoryList()->findVideoById(id) != nullptr) {
                        video_selected_for_stream->already_viewed_icon->show();
                    }
                    if (video_url != nullptr && cache->is_cached(ytdlp->getIdFor(video_url->c_str()))) {
                        char cache_tooltip[128];
                        std::snprintf(cache_tooltip, sizeof(cache_tooltip), _("Video URL Cached (valid until %s). Click to remove from cache."),
                                      cache->get_cache_expiration_date(ytdlp->getIdFor(video_url->c_str())).c_str());
                        video_selected_for_stream->cache_bttn->copy_tooltip(cache_tooltip);
                        video_selected_for_stream->cache_bttn->show();
                    }
                    video_selected_for_stream = nullptr;
                }
            }
            change_cursor();
        }
        return Fl::handle_(event, w);      // Otherwise, let default FLTK Handler handle the event...
    });
}

/**
 * Hook: Actions to execute after main window is drawed...
 */
void post_init() {
    int y_refernce_pos = mainWin->search_result_selectors->y() + 10;

    for (int i = 0; i < video_info_arr.size(); i++) {
        video_info_arr[i] = create_video_group(15, y_refernce_pos + (90 * i));
        video_info_arr[i]->thumbnail->shortcut(config->getShortcutFor("FOCUS_VIDEO_" + std::to_string(i + 1)));
        video_info_arr[i]->userUploader->shortcut(config->getShortcutFor("FOCUS_CHANNEL_" + std::to_string(i + 1)));
        mainWin->add(video_info_arr[i]);
    }

    for (VideoInfo* videoInfo: video_info_arr) {
        videoInfo->hide();
    }

    mainWin->about_bttn->callback((Fl_Callback*)showFLTubeHelpWindow);
    mainWin->about_bttn->shortcut(config->getShortcutFor(SHORTCUTS::SHOW_HELP));

    std::vector<std::string> videoLists = userdata->getVideoListNames();
    std::sort(videoLists.begin(),videoLists.end()); //Sort lists alphabetically
    for (int i=0; i < videoLists.size() ; i++) {
        mainWin->videolist_selector->add(videoLists.at(i).c_str());
    }

    mainWin->videolist_selector->value(mainWin->videolist_selector->find_index(UserDataManager::HISTORY_LIST_NAME.c_str()));
    mainWin->videolist_selector->when(FL_WHEN_CHANGED);
    mainWin->videolist_selector->callback((Fl_Callback*)getVideosAtList_cb);

    mainWin->central_tabs->when(FL_WHEN_RELEASE_ALWAYS);
    mainWin->central_tabs->callback((Fl_Callback*)selectCentralTab_cb);

    if (arrow_up != nullptr) mainWin->next_search_term_bttn->image(arrow_up);
    if (arrow_down != nullptr) mainWin->prev_search_term_bttn->image(arrow_down);


    mainWin->cache_unpause_bttn->hide();
    mainWin->cache_clearall_bttn->callback([](Fl_Widget* w, void* data) {
        cache->remove_all_entries();
        logger->debug(_("All cache entries were deleted by user demand..."));
    });
    mainWin->cache_pause_bttn->callback([](Fl_Widget* w, void* data) {
        if (cache->change_status(CACHE_RECORD_STATUS::STOPPED)) {
            mainWin->cache_pause_bttn->hide();
            mainWin->cache_unpause_bttn->show();
            logger->debug(_("Cache recording was stopped by user demand..."));
        }
    });
    mainWin->cache_unpause_bttn->callback([](Fl_Widget* w, void* data) {
        if (cache->change_status(CACHE_RECORD_STATUS::STARTED)) {
            mainWin->cache_unpause_bttn->hide();
            mainWin->cache_pause_bttn->show();
            logger->debug(_("Cache recording was started by user demand..."));
        }
    });
    mainWin->history_unpause_bttn->hide();
    mainWin->history_pause_bttn->callback([](Fl_Widget* w, void* data) {
        if (NAVIGATION_HISTORY_ENABLED) {
            NAVIGATION_HISTORY_ENABLED = false;
            mainWin->history_unpause_bttn->show();
            mainWin->history_pause_bttn->hide();
            logger->debug(_("Navigation History recording was stopped by user demand..."));
        }
    });
    mainWin->history_unpause_bttn->callback([](Fl_Widget* w, void* data) {
        if (!NAVIGATION_HISTORY_ENABLED) {
            NAVIGATION_HISTORY_ENABLED = true;
            mainWin->history_pause_bttn->show();
            mainWin->history_unpause_bttn->hide();
            logger->debug(_("Navigation History recording was started by user demand..."));
        }
    });
    mainWin->history_clearall_bttn->callback([](Fl_Widget* w, void* data) {
        bool dummyFlag;
        bool continue_clearall = showChoiceWindow(_("This action cannot be undone. Do you want to clean all Navigation History?"), dummyFlag);
        if (continue_clearall) {
            if (userdata->cleanVideoList(UserDataManager::HISTORY_LIST_NAME)){
                logger->debug(_("Navigation History content was cleaned by user demand..."));
            }
        }
    });

    mainWin->check_update_bttn->callback((Fl_Callback*) check_fltube_update_cb);

    mainWin->quality_240_bttn->callback((Fl_Callback*) change_stream_resolution_cb);
    mainWin->quality_360_bttn->callback((Fl_Callback*) change_stream_resolution_cb);
    mainWin->quality_480_bttn->callback((Fl_Callback*) change_stream_resolution_cb);
    mainWin->quality_720_bttn->callback((Fl_Callback*) change_stream_resolution_cb);
    mainWin->quality_1080_bttn->callback((Fl_Callback*) change_stream_resolution_cb);

    switch (STREAM_VIDEO_RESOLUTION) {
        case R240p:   mainWin->quality_240_bttn->setonly(); break;
        case R360p:   mainWin->quality_360_bttn->setonly(); break;
        case R480p:   mainWin->quality_480_bttn->setonly(); break;
        case R720p:   mainWin->quality_720_bttn->setonly(); break;
        case R1080p:   mainWin->quality_1080_bttn->setonly(); break;
    }
    // Redraw the window to show the new button
    mainWin->redraw();
}

/**
 * Create a new group to display video info.
 */
VideoInfo* create_video_group(int posx, int posy) {
    VideoInfo *video_info = new VideoInfo (posx, posy, 600, 90, "");
    video_info->thumbnail->callback((Fl_Callback*)preview_video_cb);
    video_info->userUploader->callback((Fl_Callback*)getYTChannelVideo_cb);
    if (live_image != nullptr) video_info->is_live_image->image(live_image);
    if (already_viewed_image != nullptr) video_info->already_viewed_icon->image(already_viewed_image);
    if (like_icon_image != nullptr) video_info->like_icon_bttn->image(like_icon_image);
    video_info->like_icon_bttn->callback((Fl_Callback*)markLikedVideo_cb);
    if (watchlater_icon_image != nullptr) video_info->watch_later_bttn->image(watchlater_icon_image);
    video_info->watch_later_bttn->callback((Fl_Callback*)add_to_watch_later);
    if (cached_icon_image != nullptr) video_info->cache_bttn->image(cached_icon_image);
    video_info->cache_bttn->callback((Fl_Callback*)removeFromCache_cb);
    if (remove_video_icon_image != nullptr) video_info->remove_bttn->image(remove_video_icon_image);
    video_info->remove_bttn->callback((Fl_Callback*)removeFromVideoList_cb);
    return video_info;
}

/**
 * Search by YT URL or search term. Or if "is_a_channel" is set, then return videos from channel URL specified at "input_text".
 */
void doSearch(const char* input_text) {
    //Change cursor to wait symbol, to indicate that the search is in process...
    ytdlp_action_in_progress = true;
    change_cursor(FL_CURSOR_WAIT);
    // Check if there is Internet connectivity before do a search...
    if (! verify_network_connection()) {
        //Restore cursor after search failed because no Internet is available...
        ytdlp_action_in_progress = false;
        change_cursor();
        logger->warn(_("Your device is offline. Check your internet connection."));
        showMessageWindow( _("There seems that you don't have access to the Internet. "
                            "Please, verify you network connection before proceed..."));
        return;
    }

    char message[1024];
    snprintf(message, sizeof(message), _("Searching for results for '%s' user input..."), input_text);
    logger->debug(std::string(message));
    if (isUrl(input_text) && !SEARCH_BY_CHANNEL_F) {
        if (YtDlp_Helper::isTikTokURL(input_text)) {
            ytdlp_action_in_progress = false;
            change_cursor();
            showMessageWindow(_("TikTok is not part of YouTube. Use the TikTok icon."));
            return;
        }
        if(!YtDlp_Helper::isYoutubeURL(input_text)){
            ytdlp_action_in_progress = false;
            change_cursor();
            std::string warn_message = _("Use a YouTube address, or search with a word.");
            showMessageWindow(warn_message.c_str());
            logger->warn(warn_message);
            return;
        }
        ytdlp->set_search_type(SEARCH_BY_TYPE::VIDEO_URL);
        page_manager->reset();
        page_manager->limit(true);
        page_manager->set_max_results(1);

    } else {
        if (page_manager->current().index == 0) {
            mainWin->next_results_bttn->activate();
        }
        ytdlp->set_search_type( (SEARCH_BY_CHANNEL_F) ? SEARCH_BY_TYPE::CHANNEL_URL : SEARCH_BY_TYPE::TERM);
    }
    if (ytdlp) {
        ytdlp->set_shorts(SHORTS_SEARCH_F && !SEARCH_BY_CHANNEL_F);
        ytdlp->set_tiktok(false);
    }
    video_metadata = ytdlp->search(input_text, page_manager->current());
    bool is_empty_metadata = std::all_of(video_metadata.begin(), video_metadata.end(),
                                         [](YTDLP_Video_Metadata* ptr) { return ptr == nullptr; });
    bool are_more_results = std::none_of(video_metadata.begin(), video_metadata.end(),
                                         [](YTDLP_Video_Metadata* ptr) { return ptr == nullptr; });
    if (!is_empty_metadata)  {
        update_video_info();
    }
    if (!are_more_results) {
        page_manager->limit(true);
        page_manager->set_max_results(page_manager->current().upper_end());
    }
    //Restore cursor to default when search is done.
    ytdlp_action_in_progress = false;
    change_cursor();
}

// Callback for moving backward or forward the search value in history.
void moveSearchValue(Fl_Widget* w, void* data) {
    bool next = (data != nullptr);
    std::string new_value;
    if (next) {
        new_value = ytdlp->getNextInSearchHistory();
    } else {
        new_value = ytdlp->getPreviousInSearchHistory();
    }
    if (!new_value.empty()) mainWin->search_term_or_url->value(new_value.c_str());
}

/*
 *  Callback to get videos from an specific Youtube channel.
 */
void getYTChannelVideo_cb(Fl_Button* bttn, void* channel_id_str){
    if (bttn == nullptr) {
        logger->error(_("The Fl_Button is inexistent when search videos for a channel!!!\n"));
        return;
    }
    std::string* channel_id =  static_cast<std::string*>(channel_id_str);
    logger->debug(_("Looking up videos of CHANNEL with ID: ") + *channel_id);
    if (channel_id == nullptr || channel_id->empty()) {
        logger->error(_("Channel ID is empty. Please verify the video metadata initilization!!!\n"));
        return;
    }
    if (getActiveTabName() == TAB_VIDEOLIST_NAME) {
        // If "My Lists" tab is open, then change to the main searchbox tab...
        mainWin->central_tabs->value(mainWin->searchbox_tab);
    }
    page_manager->reset();
    SEARCH_BY_CHANNEL_F = true;
    char channel_videos_URL[256];
    snprintf(channel_videos_URL, sizeof(channel_videos_URL), "https://www.youtube.com/channel/%s/videos", channel_id->c_str());
    mainWin->search_term_or_url->value(channel_videos_URL);
    lock_buttons(true);
    doSearch(channel_videos_URL);
    lock_buttons(false);
    mainWin->previous_results_bttn->deactivate();
    mainWin->first_page_bttn->deactivate();
    mainWin->last_page_bttn->deactivate();
}


/*
 * Extract and return the search term value from the search input. Returns nullptr only if text if empty or some error happens...
 */
const char* getSearchValue(Fl_Input *input){
    const char* input_text = input->value();
    if (input_text == nullptr || input_text[0] == '\0') {
        showMessageWindow(_("Search input is empty. Please, retry and enter a valid text."));
        return nullptr;
    }
    return input_text;
}

/*
 * Callback for search button.
 */
void searchButtonAction_cb(Fl_Widget *wdgt, Fl_Input *input){
    //Reset global pagination index and "deactivate" previos button...
    page_manager->reset();
    SEARCH_BY_CHANNEL_F = false;
    const char* input_text = getSearchValue(input);
    if (input_text != nullptr)  {
        lock_buttons(true);
        doSearch(input_text);
        lock_buttons(false);
    }
    mainWin->previous_results_bttn->deactivate();
    mainWin->first_page_bttn->deactivate();
    if (!page_manager->exists_next()) mainWin->next_results_bttn->deactivate();
    if (page_manager->is_last_page_known()) mainWin->last_page_bttn->deactivate();
}

void change_results_page(Fl_Input *input) {
    if ( getActiveTabName() == TAB_SEARCH_NAME ) {
        const char* input_text = getSearchValue(input);
        if (input_text != nullptr)  {
            lock_buttons(true);
            doSearch(input_text);
            lock_buttons(false);
        }
    }
    if (getActiveTabName() == TAB_VIDEOLIST_NAME) {
        lock_buttons(true);
        updateVideoMetadataFromVideoList();
        lock_buttons(false);
    }
}

/** Callback for Previous results button... */
void getPreviousSearchResults_cb(Fl_Widget* widget, Fl_Input *input){
    page_manager->previous();
    change_results_page(input);
    if (!page_manager->exists_previous()) {
        mainWin->previous_results_bttn->deactivate();
        mainWin->first_page_bttn->deactivate();
    }
}

void getFirstResultsPage_cb(Fl_Widget* widget, Fl_Input *input) {
    page_manager->first();
    change_results_page(input);
    if (page_manager->exists_next()) {
        mainWin->next_results_bttn->activate();
        mainWin->last_page_bttn->activate();
    } else {
        mainWin->next_results_bttn->deactivate();
        mainWin->last_page_bttn->deactivate();
    }
    mainWin->previous_results_bttn->deactivate();
    mainWin->first_page_bttn->deactivate();
}

void getLastKnownResultsPage_cb (Fl_Widget* widget, Fl_Input *input) {
    page_manager->last();
    change_results_page(input);
    mainWin->last_page_bttn->deactivate();
    if (page_manager->is_limited()) mainWin->next_results_bttn->deactivate();
    if (page_manager->exists_previous()) {
        mainWin->previous_results_bttn->activate();
        mainWin->first_page_bttn->activate();
    } else {
        mainWin->previous_results_bttn->deactivate();
    }
}

/** Callback for Next results button.. */
void getNextSearchResults_cb(Fl_Widget* widget, Fl_Input *input){
    //TODO: what to do if there is no more results? It must be controlled in some way...
    page_manager->next();
    change_results_page(input);
    if (page_manager->is_limited()) {
        if (!page_manager->exists_next()){
            mainWin->next_results_bttn->deactivate();
            mainWin->last_page_bttn->deactivate();
        } else {
            mainWin->next_results_bttn->activate();
            mainWin->last_page_bttn->activate();
        }
    } else {
        //If pagination is not limited...
        if (page_manager->is_last_page_known()) mainWin->last_page_bttn->deactivate();
        else mainWin->last_page_bttn->activate();
    }
    mainWin->previous_results_bttn->activate();
    mainWin->first_page_bttn->activate();
}

/*
 * Print program usage and exit if desire (exitApp = true).
 */
void showUsage(bool exitApp){
    const char* usage = R"([FLTube HELP]:
FLTube is a program developed in FLTK for search and download Youtube videos, as a front-end for the subjacent "yt-dlp" tool.

USAGE:
    fltube [options]

OPTIONS:
  --config=[PATH_TO_FILE]   Please specify the ABSOLUTE path to the custom configuration file, which should be
                              a copy of the original fltube.conf. Use this option when you want to apply a
                              development configuration file for development purposes.
                              By default, the configuration file is searched for in the following locations,
                              in this order: '~/.config/fltube/' and '/usr/local/etc/fltube'.
  -d, --debug               Enable the debug mode through more verbose log messages.
  -h, --help                Show this text help.
  -v, --version             Show the program version.

)";
    printf("%s", usage);

    if (exitApp) { exit(FLT_OK); }
}

/*
 *  Parse commandline parameters.
 */
void parseOptions(int argc, char **argv){
    if (existsCmdOption(argc, argv, "-h") || existsCmdOption(argc, argv, "--help")) {
        showUsage(true);
    }
    if (existsCmdOption(argc, argv, "-v") || existsCmdOption(argc, argv, "--version")) {
        printf("FLtube v.%s\n", VERSION);
        exit(FLT_OK);
    }
    if (existsCmdOption(argc, argv, "--config")){
        const std::string path = getOptionValue(argc, argv, "--config");
        if (path != "" && std::filesystem::exists(path)){
            CONFIGFILE_PATH = path;
            printf(_("Loading custom config file %s.\n"), CONFIGFILE_PATH.c_str());
        } else {
            if (!std::filesystem::exists(path)) {
                printf(_("Specified path at --config parameter does not exist.\n"));
            } else {
                printf(_("--config parameter with no path specified.\n"));
            }
            exit(FLT_INVALID_CMD_PARAM);
        }
    }
    if (existsCmdOption(argc, argv, "-d") || existsCmdOption(argc, argv, "--debug")) {
        printf(_("DEBUG MODE enabled.\n"));
        DEBUG_ENABLED = true;
    }
}

//////////////////////////////////////////////////////////////////////////////////
///////////////// MAIN     MAIN     MAIN        MAIN        MAIN   //////////////
//////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
    parseOptions(argc, argv);
    logger = std::make_shared<TerminalLogger>(DEBUG_ENABLED);
    showInitialWindow();
    std::thread worker(pre_init);
    worker.detach();
    while (initial_win->shown()) {
        Fl::check();
        if (!SHOWING_LOADING_SCREEN_F) {
            break;
        }
    }
    initial_win->hide();
    delete initial_win;

    char message[64];
    snprintf(message, sizeof(message), _("Starting MiniOS YouTube\n"));
    logger->info(message);

    snprintf(message, sizeof(message), "MiniOS YouTube");
    mainWin = new FLTubeMainWindow(593, 540, message);
    center_window(mainWin);
    mainWin->callback((Fl_Callback*)exitApp);
    mainWin->search_term_or_url->when(FL_WHEN_ENTER_KEY);
    mainWin->search_term_or_url->callback((Fl_Callback*)searchButtonAction_cb, (void*)(mainWin->search_term_or_url));
    mainWin->search_term_or_url->shortcut(config->getShortcutFor(SHORTCUTS::FOCUS_SEARCH));
    mainWin->search_term_or_url->set_search_source(ytdlp);
    mainWin->prev_search_term_bttn->callback((Fl_Callback*)moveSearchValue,(void*)0);
    mainWin->next_search_term_bttn->callback((Fl_Callback*)moveSearchValue,(void*)1);
    mainWin->do_search_bttn->callback((Fl_Callback*)searchButtonAction_cb, (void*)(mainWin->search_term_or_url));
    if (mainWin->shorts_bttn) {
        mainWin->shorts_bttn->callback([](Fl_Widget* w, void*) {
            SHORTS_SEARCH_F = !SHORTS_SEARCH_F;
            if (SHORTS_SEARCH_F)
                TIKTOK_SEARCH_F = false;
            Fl_Button *b = (Fl_Button *)w;
            if (SHORTS_SEARCH_F) {
                b->copy_label("Shorts ON");
                b->color(fl_rgb_color(200, 160, 0));
                b->labelcolor(FL_BLACK);
                FILE *flag = fopen("/tmp/minios-yt-shorts", "w");
                if (flag) {
                    fputs("1\n", flag);
                    fclose(flag);
                }
                unlink("/tmp/minios-yt-tiktok");
            } else {
                b->copy_label("Shorts");
                b->color(FL_BACKGROUND_COLOR);
                b->labelcolor(FL_FOREGROUND_COLOR);
                unlink("/tmp/minios-yt-shorts");
            }
            b->redraw();
            if (ytdlp) {
                ytdlp->set_shorts(SHORTS_SEARCH_F);
                ytdlp->set_tiktok(false);
            }
            const char *term = mainWin->search_term_or_url->value();
            if (term && term[0])
                doSearch(term);
        });
    }
    mainWin->previous_results_bttn->callback((Fl_Callback*)getPreviousSearchResults_cb, (void*)(mainWin->search_term_or_url));
    mainWin->previous_results_bttn->deactivate();
    mainWin->next_results_bttn->callback((Fl_Callback*)getNextSearchResults_cb, (void*)(mainWin->search_term_or_url));
    mainWin->next_results_bttn->deactivate();
    mainWin->first_page_bttn->callback((Fl_Callback*)getFirstResultsPage_cb, (void*)(mainWin->search_term_or_url));
    mainWin->first_page_bttn->deactivate();
    mainWin->last_page_bttn->callback((Fl_Callback*)getLastKnownResultsPage_cb, (void*)(mainWin->search_term_or_url));
    mainWin->last_page_bttn->deactivate();

    post_init();
    //mainWin->show(argc, argv);    //TODO Avoid passing parameters to the mainWin function to prevent undesirable logs.
    mainWin->icon(load_image("/usr/local/share/icons/fltube/64x64.png"));
    mainWin->show();
    Fl::check();
    fit_search_to_screen(mainWin);
    mainWin->redraw();
    // GUI is on screen. Close the dock "YouTube is loading" box now
    // (do not wait for its timeout).
    if (system("/opt/minios/minios-loading stop >/dev/null 2>&1") != 0) {
        system("pkill -x xmessage >/dev/null 2>&1");
    }
    logger->debug(_("The FLTube main window has loaded."));
    return Fl::run();

}
