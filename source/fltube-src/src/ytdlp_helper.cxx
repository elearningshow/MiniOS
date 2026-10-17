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

#include "../include/ytdlp_helper.h"
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

/* Clock "0:27:50" / "27:50" / raw seconds. -1 if unknown. */
static int duration_seconds(const std::string &sec_field, const std::string &clock)
{
    const char *s = sec_field.c_str();
    if (s[0] && isdigit((unsigned char)s[0])) {
        int all_digit = 1;
        for (const char *p = s; *p; p++) {
            if (!isdigit((unsigned char)*p)) {
                all_digit = 0;
                break;
            }
        }
        if (all_digit)
            return atoi(s);
    }
    s = clock.c_str();
    if (!s[0] || s[0] == 'N' || s[0] == 'n')
        return -1;
    int h = 0, m = 0, sec = 0;
    if (sscanf(s, "%d:%d:%d", &h, &m, &sec) == 3)
        return h * 3600 + m * 60 + sec;
    if (sscanf(s, "%d:%d", &m, &sec) == 2)
        return m * 60 + sec;
    if (sscanf(s, "%d", &sec) == 1)
        return sec;
    return -1;
}

const std::string YtDlp_Helper::PRINT_METADATA_TEMPLATE = "title=\\\"%(title)s\\\">>thumbnail=\\\"%(thumbnails.0.url)s" \
"\\\">>creators=\\\"%(uploader,playlist_channel)s\\\">>video_id=\\\"%(id)s\\\">>upload_date=\\\"%(upload_date>%Y-%m-%d)s" \
"\\\">>duration=\\\"%(duration>%H:%M:%S)s\\\">>duration_sec=\\\"%(duration)s\\\">>webpage_url=\\\"%(webpage_url,url)s" \
"\\\">>channel_id=\\\"%(playlist_channel_id,channel_id)s\\\">>live_status=\\\"%(live_status)s\\\">>viewers_count=\\\"%(view_count,concurrent_view_count)s\\\">>";


/** Parse the metadata as define in @PRINT_METADATA_TEMPLATE. Returns a YTDLP_Video_Metadata struct. */
YTDLP_Video_Metadata* YtDlp_Helper::parse_metadata(const char ytdlp_video_metadata[512]){
    std::stringstream lineStream(ytdlp_video_metadata);
    YTDLP_Video_Metadata* metadata = new YTDLP_Video_Metadata();
    std::string keyvalue;

    // Find metadata key=value delimited by ">>" symbols...
    while(std::getline (lineStream,keyvalue,'>'))
    {
        if (lineStream.peek() == '>'){
            lineStream.ignore();
            size_t pos = keyvalue.find('=');
            if (pos != std::string::npos) {
                std::string key = keyvalue.substr(0, pos);
                std::string value = keyvalue.substr(pos + 2, keyvalue.size() - pos - 3); // Remove quotes
                //printf("key: %s, value: %s\n", key.c_str(), value.c_str());
                if (key == "title") {
                    metadata->title = value;
                } else if (key == "video_id"){
                    metadata->id = value;
                } else if (key == "creators"){
                    metadata->creators = value;
                } else if (key == "upload_date"){
                    metadata->upload_date = value;
                } else if (key == "duration"){
                    metadata->duration = value;
                } else if (key == "duration_sec"){
                    metadata->duration_sec = value;
                } else if (key == "webpage_url"){
                    metadata->webpage_url = value;
                } else if (key == "thumbnail"){
                    metadata->thumbnail_url = value;
                } else if (key == "channel_id"){
                    metadata->channel_id = value;
                } else if (key == "live_status"){
                    metadata->live_status = value;
                } else if (key == "viewers_count"){
                    metadata->viewers_count = value;
                }
            }
        }
    }
    metadata->url =  YOUTUBE_URL_PREFIX + metadata->id;
    return metadata;
}

std::string YtDlp_Helper::getIdFor(std::string video_url) {
    return video_url + ":" + std::to_string(this->video_resolution);
}

std::vector<YTDLP_Video_Metadata*> YtDlp_Helper::retrieve_metadata(const char* ytdlp_cmd) {
    logger->debug(ytdlp_cmd);
    std::string result = exec(ytdlp_cmd);
    std::vector<YTDLP_Video_Metadata*> metadata;
    logger->debug(result);
    // Read input lines until an empty line is encountered
    std::istringstream result_sstream(result);
    std::string line;

    getline(result_sstream, line);
    while (!line.empty()) {
        //Set the video metadata at the array...
        metadata.push_back(YtDlp_Helper::parse_metadata(line.c_str()));
        getline(result_sstream, line);
    }
    return metadata;
}

/**
 * Make a search by term in the specified extractor (i.e. "youtube", etc.). See a complete list of extractors at yt-dlp docs.
 * For now, only do searchs at Youtube.
 */
yt_metadata_arr YtDlp_Helper::search(const char* search_term, Pagination_Info page_info){
    std::string clean_text = std::string(search_term);
    trim_and_clean(clean_text);
    if (this->extractor == YTDLP_EXTRACTOR::YOUTUBE) {
        return this->do_youtube_search(clean_text.c_str(), page_info);
    } else {
        return {};
    }
}

yt_metadata_arr YtDlp_Helper::do_youtube_search(const char* search_text ,Pagination_Info page_info){
    char search_component[400];
    yt_metadata_arr result_yt_metadata;
    result_yt_metadata.fill(nullptr);
    std::vector<YTDLP_Video_Metadata*> mtd;
    Pagination_Info page_info_ = page_info;
    std::string cache_key = search_text;
    if (tiktok_mode && search_type == SEARCH_BY_TYPE::TERM)
        cache_key = std::string("tiktok:") + search_text;
    else if (shorts_mode && search_type == SEARCH_BY_TYPE::TERM)
        cache_key = std::string("shorts:") + search_text;
    // Define search yt-dlp command, that will be used if necessary...
    switch (this->search_type) {
        case SEARCH_BY_TYPE::CHANNEL_URL:
            snprintf(search_component, sizeof(search_component), "%s", search_text);  //The term is a Channel URL...
            break;
        case SEARCH_BY_TYPE::VIDEO_URL:
            strcpy(search_component, search_text);
            page_info_ = Pagination_Info(1,0);
            break;
        case SEARCH_BY_TYPE::TERM:
            // Else, do a normal search by term.
            break;
    }
    char ytdlp_cmd[2048];
    char cmd_format[768] = "yt-dlp \"%s\" -I %d-%d --flat-playlist --print \"%s\" --extractor-args youtubetab:approximate_date";

    // Check if exists cached results for this type of search...
    if (search_type != SEARCH_BY_TYPE::VIDEO_URL) {
        auto search_data = search_cache.find(cache_key);
        // If not cached, OR have to get more results, then retrieve and cache new results...
        if (search_data == search_cache.end()
                || (search_data != search_cache.end() && search_data->second.size() < page_info_.upper_end())) {

            int start, end;
            if (search_data == search_cache.end()) {
                search_cache[cache_key] = {};
                search_data = search_cache.find(cache_key);
                start = 1; end = batch_search_size;
                search_history.push_back(search_text);
            } else {
                start = search_data->second.size();
                end = search_data->second.size() + batch_search_size;
            }

            if ( search_type == SEARCH_BY_TYPE::TERM) {
                if (tiktok_mode) {
                    snprintf(search_component, sizeof(search_component),
                             "tiktoksearch%d:%s", end, search_text);
                } else if (shorts_mode) {
                    char qenc[180];
                    size_t j = 0;
                    for (size_t i = 0; search_text[i] && j + 4 < sizeof(qenc); i++) {
                        unsigned char c = (unsigned char)search_text[i];
                        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
                            qenc[j++] = (char)c;
                        else if (c == ' ')
                            qenc[j++] = '+';
                        else {
                            snprintf(qenc + j, sizeof(qenc) - j, "%%%02X", c);
                            j += 3;
                        }
                    }
                    qenc[j] = 0;
                    /* Official Shorts filter. Drop anything longer than 3 minutes. */
                    snprintf(search_component, sizeof(search_component),
                             "https://www.youtube.com/results?search_query=%s&sp=EgIYAQ%%3D%%3D",
                             qenc);
                } else {
                    snprintf(search_component, sizeof(search_component), "ytsearch%d:%s", end, search_text);
                }
            }
            if (shorts_mode && search_type == SEARCH_BY_TYPE::TERM) {
                /* Pull extra raw hits; most long clips are dropped below. */
                int raw_end = end + batch_search_size;
                snprintf(ytdlp_cmd, sizeof(ytdlp_cmd),
                         "yt-dlp \"%s\" -I %d-%d --flat-playlist --match-filter \"duration<=180\" --print \"%s\" --extractor-args youtubetab:approximate_date",
                         search_component, start, raw_end, YtDlp_Helper::PRINT_METADATA_TEMPLATE.c_str());
            }
            else
                snprintf(ytdlp_cmd, sizeof(ytdlp_cmd), cmd_format, search_component, start, end, YtDlp_Helper::PRINT_METADATA_TEMPLATE.c_str());
            mtd = retrieve_metadata(ytdlp_cmd);

            for ( YTDLP_Video_Metadata* video_m: mtd) {
                int secs = duration_seconds(video_m->duration_sec, video_m->duration);
                const std::string &src = video_m->webpage_url.empty() ? video_m->url : video_m->webpage_url;
                if (tiktok_mode) {
                    if (!video_m->id.empty())
                        video_m->url = std::string("https://www.tiktok.com/@_/video/") + video_m->id;
                    if (src.find("tiktok.com") != std::string::npos)
                        video_m->url = src;
                } else if (shorts_mode) {
                    /* Keep only clips we know are 3 minutes or less.
                     * A /shorts/ URL is not enough: YouTube still lists
                     * 27-minute videos there. Unknown duration is dropped. */
                    if (secs < 1 || secs > 180)
                        continue;
                    if (!video_m->id.empty())
                        video_m->url = std::string("https://www.youtube.com/shorts/") + video_m->id;
                    if (src.find("/shorts/") != std::string::npos)
                        video_m->url = src;
                }
                search_data->second.push_back(std::make_pair(video_m->id, video_m));
            }
        }
        auto search_cached_results = search_data->second;
        int retrieve_position;
        for (int i=0; i < PaginationManager::SEARCH_PAGE_SIZE; i++) {
            retrieve_position = (page_info_.lower_end() - 1) + i;
            if (retrieve_position < search_cached_results.size()) {
                result_yt_metadata[i] = search_cached_results[retrieve_position].second;
            }
        }
    } else {
        // If search only one video (SEARCH_BY_TYPE::VIDEO_URL), then get its metadata...
        snprintf(ytdlp_cmd, sizeof(ytdlp_cmd), cmd_format, search_component,
                 page_info_.lower_end(), page_info_.upper_end(), YtDlp_Helper::PRINT_METADATA_TEMPLATE.c_str());
        mtd = retrieve_metadata(ytdlp_cmd);
        if (!mtd.empty()) result_yt_metadata[0] = mtd[0];
    }
    return result_yt_metadata;
}

void YtDlp_Helper::stream(const char* video_url) {
    char get_final_url_cmd[2048];
    char stream_videoplayer_cmd[2048];
    char stream_format[100];
    std::string final_url_result;
    stream_videoplayer_cmd[0] = 0;
    snprintf(stream_format, sizeof(stream_format), "res:%d,+codec:avc1:m4a", this->video_resolution);
    if (this->is_live_flag) {
        snprintf(stream_videoplayer_cmd, sizeof(stream_videoplayer_cmd),
                 "yt-dlp -S \"%s\" -o - \"%s\" | %s %s %s -", stream_format, video_url, this->media_player->getBinaryPath().c_str(), this->media_player->getParams().c_str(), this->media_player->getExtraParams().c_str());
    } else {
        // If video is not live, 1rst try to obtain final video URL using default method...
        // 1rst: lookup final video URL if exists at cache...
        final_url_result = cache->get_entry_value(getIdFor(video_url));
        if (final_url_result == CacheEntry::EMPTY_VALUE) {
            // 2nd: if final video url is not cached, then obtain it using yt-dlp.
            snprintf(get_final_url_cmd, sizeof(get_final_url_cmd), "yt-dlp -S \"%s\" -g \"%s\" 2> %s/ytdlp_errors.log", stream_format, video_url, this->TEMP_WORKING_DIR.c_str());
            this->logger->debug("EXEC COMMAND = " + std::string(get_final_url_cmd) + "\n");
            final_url_result = exec(get_final_url_cmd);
            replace_all(final_url_result, "\n", "");
        }
        if (final_url_result != "") {
            // Once final URL is obtained, then open at configured Media Player...
            snprintf(stream_videoplayer_cmd, sizeof(stream_videoplayer_cmd),
                    "%s %s \"%s\"", this->media_player->getBinaryPath().c_str(), this->media_player->getParams().c_str(), final_url_result.c_str());
            cache->add_entry(getIdFor(video_url), final_url_result);
        } else {
            // If default method doesn't works, then try the alternative method (if configured this way)...
            if (this->enable_alternative_stream_method) {
                logger->warn(_("The default stream command doesn't work. Fallback to the alternative method to get final video URL."));
                snprintf(stream_format, sizeof(stream_format), "bv*[height<=%d][vcodec^=avc]+ba[acodec^=mp4a]", this->video_resolution);
                snprintf(stream_videoplayer_cmd, sizeof(stream_videoplayer_cmd),
                    "yt-dlp -f \"%s\" -o - --merge-output-format mkv \"%s\" | %s %s -", stream_format, video_url, this->media_player->getBinaryPath().c_str(), this->media_player->getParams().c_str());
            } else {
                logger->error(_("Cannot obtain URL for specified video, and alternative stream method is disabled."));
            }
        }
    }
    if (stream_videoplayer_cmd[0] == 0) {
        this->logger->error(_("Cannot obtain URL for specified video, and alternative stream method is disabled."));
        FILE *ff = fopen("/tmp/minios-mp.fail", "w");
        if (ff) {
            fputs("fail\n", ff);
            fclose(ff);
        }
        return;
    }
    this->logger->debug("EXEC COMMAND = " + std::string(stream_videoplayer_cmd) + "\n");
    system(stream_videoplayer_cmd);
}

/**
 * Download a video from a its URL using the configured multimedia player.
 */
void YtDlp_Helper::download_video(const char* video_url, const char* download_path, VCODEC_RESOLUTIONS v_resolution,
                    const char* vcodec = VIDEOCODEC_PREFERRED.c_str()){
    char download_cmd[2048], s_dwl_data[200], s_dwl_dir[200];
    const char* download_data_format= "bestvideo[height<=%d][vcodec^=%s]+bestaudio/best";
    const char* download_dir_format = "%s/%(id)s.%s";
    snprintf(s_dwl_data, sizeof(s_dwl_data), download_data_format, v_resolution, vcodec);
    snprintf(s_dwl_dir, sizeof(s_dwl_dir), download_dir_format, download_path, DOWNLOAD_VIDEO_PREFERRED_EXT.c_str());
    snprintf(download_cmd, sizeof(download_cmd),
             "yt-dlp -f \"%s\" \"%s\" -o \"%s\"", s_dwl_data, video_url, s_dwl_dir);
    printf("%s\n", download_cmd);
    system(download_cmd);
}

void YtDlp_Helper::write_playlist(const char *path, const char *current_url)
{
    FILE *f = fopen(path, "w");
    int idx = 0;
    int matched = 0;
    std::string key;
    std::vector<std::pair<std::string, std::string> > rows;

    if (!f)
        return;
    if (!search_history.empty())
        key = search_history.back();
    auto it = search_cache.end();
    if (shorts_mode)
        it = search_cache.find(std::string("shorts:") + key);
    else if (tiktok_mode)
        it = search_cache.find(std::string("tiktok:") + key);
    if (it == search_cache.end())
        it = search_cache.find(key);
    if (it != search_cache.end()) {
        for (auto &p : it->second) {
            int secs;
            if (!p.second)
                continue;
            secs = duration_seconds(p.second->duration_sec, p.second->duration);
            /* Check every row. A leftover regular search cache must not
             * put a 30-minute clip on a Shorts playlist. */
            /* Rows already passed the search filter. Only drop a long clip.
             * Unknown duration used to empty the whole Shorts playlist. */
            if (shorts_mode && secs > 180)
                continue;
            if (current_url && p.second->url == current_url) {
                idx = (int)rows.size();
                matched = 1;
            } else if (current_url) {
                /* The button URL and the cached URL can differ by extra
                 * parameters. Search rows are stored as youtu.be/ID.
                 * Match that id, watch?v=, or /shorts/. */
                auto id_of = [](const std::string &u) -> std::string {
                    auto v = u.find("v=");
                    if (v != std::string::npos && u.size() >= v + 13)
                        return u.substr(v + 2, 11);
                    auto s = u.find("/shorts/");
                    if (s != std::string::npos && u.size() >= s + 19)
                        return u.substr(s + 8, 11);
                    auto y = u.find("youtu.be/");
                    if (y != std::string::npos && u.size() >= y + 20)
                        return u.substr(y + 9, 11);
                    return std::string();
                };
                std::string want = id_of(current_url);
                std::string got = id_of(p.second->url);
                if (!want.empty() && want == got) {
                    idx = (int)rows.size();
                    matched = 1;
                }
            }
            std::string t = p.second->title;
            for (char &c : t) {
                if (c == '\t' || c == '\n' || c == '\r')
                    c = ' ';
            }
            if (t.empty())
                t = p.second->id;
            char line[400];
            snprintf(line, sizeof(line), "%s\t%s\t%d",
                     p.second->url.c_str(), t.c_str(), secs > 0 ? secs : 0);
            rows.push_back(std::make_pair(p.second->url, std::string(line)));
        }
    }
    /* The running player starts playlist row 0 on open. Put the clicked
     * video on that row so the first open plays the selection. */
    if (matched && idx > 0 && idx < (int)rows.size()) {
        auto clicked = rows[(size_t)idx];
        rows.erase(rows.begin() + idx);
        rows.insert(rows.begin(), clicked);
    }
    if (!matched && current_url && current_url[0]) {
        char extra[400];
        snprintf(extra, sizeof(extra), "%s\tSelected video\t0", current_url);
        rows.insert(rows.begin(), std::make_pair(std::string(current_url), std::string(extra)));
    }
    idx = 0;
    fprintf(f, "%d\n", idx);
    for (size_t i = 0; i < rows.size(); i++)
        fprintf(f, "%s\n", rows[i].second.c_str());
    fclose(f);
    fprintf(stderr, "playlist-v37 shorts-checked %zu\n", rows.size());
}

/**
* Return a metric abbreviation from a number, for example: 1520
* = 1.5K, 1.450.000 = 1.4M
*/
std::string* YtDlp_Helper::get_metric_abbreviation(int number) {
    char metric_abbr[32];
    if (number < 1000) {
        snprintf(metric_abbr, sizeof(metric_abbr), "%d", number );
    } else if (number < 1000000) {
        snprintf(metric_abbr, sizeof(metric_abbr), "%.1fK", (number/1000.0));
    } else {
        snprintf(metric_abbr, sizeof(metric_abbr), "%.1fM", (number/1000000.0));
    }
    return new std::string(metric_abbr);
}

/** Check if an URL starts with any valid Youtube URL format.**/
bool YtDlp_Helper::isYoutubeURL(const char* url){
    std::string url_s(url);
    return ((url_s.rfind("https://www.youtube.com/", 0) == 0) || (url_s.rfind("https://youtube.com/", 0) == 0) || (url_s.rfind("https://youtu.be/", 0) == 0));
}

bool YtDlp_Helper::isTikTokURL(const char* url){
    std::string url_s(url);
    return (url_s.find("tiktok.com/") != std::string::npos);
}

bool YtDlp_Helper::isSupportedURL(const char* url){
    return isYoutubeURL(url) || isTikTokURL(url);
}

std::string YtDlp_Helper::getNextInSearchHistory() {
    if (search_history.empty()) return "";
    if (current_search_history_index + 1 <= search_history.size() - 1) {
        current_search_history_index++;
    }
    return search_history.at(current_search_history_index);
}

std::string YtDlp_Helper::getPreviousInSearchHistory() {
    if (search_history.empty()) return "";
    if (current_search_history_index - 1 >= 0) {
        current_search_history_index--;
    }
    return search_history.at(current_search_history_index);
}

void YtDlp_Helper::resetSearchHistoryPos() {
    current_search_history_index = 0;
}
