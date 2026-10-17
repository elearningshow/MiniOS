/* Turn JavaScript on, then reopen Nightly so the new setting is used. */
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int safe_url(const char *s)
{
    if (strncmp(s, "http://", 7) != 0 && strncmp(s, "https://", 8) != 0)
        return 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 33 || c > 126)
            return 0;
        if (strchr("'\"`$;&|\\<>", c))
            return 0;
    }
    return 1;
}

static void enable_js(const char *url)
{
    char cmd[2400];

    if (!safe_url(url))
        url = "https://lite.duckduckgo.com/lite/";
    system("sed -i 's/javascript.enabled\", false/javascript.enabled\", true/' "
           "/opt/minios/firefox-user.js "
           "/home/tc/.mozilla/firefox/minios.default/user.js "
           ">/dev/null 2>&1");
    system("touch /tmp/minios-js-on");
    snprintf(cmd, sizeof cmd,
             "pgrep -x firefox > /tmp/minios-ff-old 2>/dev/null; "
             "/usr/local/bin/minios-firefox '%s' >/tmp/minios-js-relaunch.log 2>&1 & "
             "sleep 2; "
             "while read p; do kill \"$p\" 2>/dev/null; done < /tmp/minios-ff-old",
             url);
    system(cmd);
}

static void reply(int fd)
{
    const char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                      "Content-Length: 2\r\nConnection: close\r\n\r\nok";
    send(fd, msg, strlen(msg), 0);
}

int main(void)
{
    int s, fd, one = 1;
    struct sockaddr_in addr;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8791);
    addr.sin_addr.s_addr = htonl(0x7f000001);
    if (bind(s, (struct sockaddr *)&addr, sizeof addr) != 0)
        return 1;
    listen(s, 4);
    for (;;) {
        char buf[2048];
        char url[1600];
        int n;
        char *q;

        fd = accept(s, NULL, NULL);
        if (fd < 0)
            continue;
        n = recv(fd, buf, sizeof buf - 1, 0);
        if (n < 0)
            n = 0;
        buf[n] = 0;
        url[0] = 0;
        q = strstr(buf, "u=");
        if (q) {
            int i = 0;
            q += 2;
            while (*q && *q != ' ' && *q != '&' && *q != '\r' && i < (int)sizeof url - 1) {
                if (*q == '%' && isxdigit((unsigned char)q[1]) && isxdigit((unsigned char)q[2])) {
                    char hex[3] = { q[1], q[2], 0 };
                    url[i++] = (char)strtol(hex, NULL, 16);
                    q += 3;
                } else if (*q == '+') {
                    url[i++] = ' ';
                    q++;
                } else {
                    url[i++] = *q++;
                }
            }
            url[i] = 0;
        }
        reply(fd);
        close(fd);
        enable_js(url);
    }
}
