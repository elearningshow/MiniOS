/* Always start Xvesa with -nozap. Setuid; Linux ignores setuid on scripts. */
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char *nargv[96];
    int i, j = 0;

    nargv[j++] = "/opt/minios/Xvesa.real";
    nargv[j++] = "-nozap";
    for (i = 1; i < argc && j < 94; i++) {
        if (strcmp(argv[i], "-nozap") == 0)
            continue;
        nargv[j++] = argv[i];
    }
    nargv[j] = 0;
    execv(nargv[0], nargv);
    return 127;
}
