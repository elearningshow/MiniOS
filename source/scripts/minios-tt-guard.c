/* Ends the TikTok player as soon as Close writes the quit file.
   Close looks for the play script by name. This program uses its own name. */
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

static int read_pid(const char *path)
{
    FILE *f;
    int pid = 0;

    f = fopen(path, "r");
    if (!f)
        return 0;
    if (fscanf(f, "%d", &pid) != 1)
        pid = 0;
    fclose(f);
    if (pid <= 1)
        return 0;
    return pid;
}

static void kill_player(const char *path)
{
    int i, pid;

    for (i = 0; i < 25; i++) {
        pid = read_pid(path);
        if (pid > 1) {
            if (kill(pid, SIGKILL) == 0)
                fprintf(stderr, "tt-guard kill %d\n", pid);
            else
                fprintf(stderr, "tt-guard kill %d errno %d\n", pid, errno);
            return;
        }
        usleep(10000);
    }
    fprintf(stderr, "tt-guard no pid\n");
}

int main(int argc, char **argv)
{
    int fd, wd;
    char buf[4096];
    const char *pidpath;
    struct pollfd pfd;

    if (argc < 2)
        return 1;
    pidpath = argv[1];
    setsid();
    signal(SIGPIPE, SIG_IGN);
    if (access("/tmp/minios-tt-mp.quit", F_OK) == 0) {
        kill_player(pidpath);
        return 0;
    }
    fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
        return 1;
    wd = inotify_add_watch(fd, "/tmp", IN_CREATE | IN_CLOSE_WRITE | IN_MOVED_TO);
    if (wd < 0)
        return 1;
    pfd.fd = fd;
    pfd.events = POLLIN;
    for (;;) {
        int n = poll(&pfd, 1, -1);
        int off = 0;
        int saw = 0;
        ssize_t len;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (access("/tmp/minios-tt-mp.quit", F_OK) == 0)
            saw = 1;
        len = read(fd, buf, sizeof buf);
        while (len > 0 && off < (int)len) {
            struct inotify_event *ev = (struct inotify_event *)(buf + off);
            if (off + (int)sizeof(*ev) > (int)len)
                break;
            if (ev->len > 0 && strcmp(ev->name, "minios-tt-mp.quit") == 0)
                saw = 1;
            off += (int)sizeof(*ev) + (int)ev->len;
        }
        if (saw) {
            kill_player(pidpath);
            break;
        }
    }
    close(fd);
    return 0;
}
