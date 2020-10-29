#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>

#ifndef NAME_MAX
#define NAME_MAX 256
#endif /* NAME_MAX */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif /* PATH_MAX */

#define ISDIGIT(x) ((unsigned int)(x) - '0' <= 9)
#define SEL_CORRECT() (sel = ((sel < 0) ? 0 : (sel > nfiles - 1) ? nfiles - 1 : sel))

/* structs and enums */
typedef struct {
        char            *name;
        off_t            size;
        unsigned short   nmlen;
        unsigned char    type;
} Entry;

/* globals */
static unsigned int maxcols = 3;
static unsigned long nfiles = 0;
static int sel = 0;

/* function declarations */
static void      cursesinit(void);
static Entry    *entriesget(const char *);
static void      pathdraw(const char *);
static void      dirdraw(const Entry *);
static void      spawn();

/* function implementations */
void
cursesinit(void)
{
        initscr();
        noecho();
        cbreak();
        curs_set(0);
        keypad(stdscr, 1);
        scrollok(stdscr, 1);
}

Entry *
entriesget(const char *dirname)
{
        DIR *dir;
        struct dirent *dp;
        Entry *entrs;
        
        if ((entrs = malloc(50 * sizeof(Entry))) == NULL) // fix this
                return NULL;
        if ((dir = opendir(dirname)) == NULL)
                return NULL;
        nfiles = 0;
        // get hidden files flag
        while ((dp = readdir(dir)) != NULL) {
                if (strcmp(dp->d_name, dirname) &&  strcmp(dp->d_name, ".")
                &&  strcmp(dp->d_name, "..")) {
                        entrs[nfiles].name = dp->d_name;
                        entrs[nfiles].type = dp->d_type;
                        nfiles++;
                }
        }
        closedir(dir);
        return entrs;
}

void
pathdraw(const char *path)
{
        attron(A_BOLD);
        mvprintw(0, 0, "%s\n", path);
        attroff(A_BOLD);
}

void
dirdraw(const Entry *entrs)
{
        // crashes on include, bin and some other dirs
        int i = 0;

        SEL_CORRECT();
        for (; i < nfiles; i++) {
                if (i == sel)
                        attron(A_REVERSE);
                mvprintw(i + 1, 0, "%s\n", entrs[i].name);
                attroff(A_REVERSE);
        }
}

void
spawn(const char *s)
{
        /*pid_t pid;*/
        /*char *args[] = {"xdg-open", s, NULL};*/
}

int
main(int argc, char *argv[])
{
        Entry *entrs;
        char cwd[PATH_MAX], *curdir;
        int c;

        if ((curdir = getcwd(cwd, sizeof(cwd))) == NULL) {
                return -1;
        }
        cursesinit();

        for (;;) {
                erase();

                curdir = getcwd(cwd, sizeof(cwd));
                entrs = entriesget(curdir);

                pathdraw(curdir);
                dirdraw(entrs);

                switch (c = getch()) {
                case 'h':       /* FALLTHROUGH */
                case KEY_LEFT:
                        chdir("..");
                        break;
                case 'l':       /* FALLTHROUGH */
                case '\n':
                case KEY_RIGHT:
                        if (entrs[sel].name != NULL)
                                // handle symlinks
                                if (entrs[sel].type == DT_DIR)
                                        chdir(entrs[sel].name);
                                else if (entrs[sel].type == DT_REG)
                                        spawn(entrs[sel].name);
                        break;
                case 'k':       /* FALLTHROUGH */
                case KEY_UP:
                        sel--;
                        break;
                case 'j':       /* FALLTHROUGH */
                case KEY_DOWN:
                        sel++;
                        break;
                case 'g':
                        c = getch();
                        if (c == 'g')
                                sel = 0;
                        break;
                case 'G':
                        sel = nfiles - 1;
                        break;
                case 'q':
                        // a goto!
                        goto exit;
                }

                refresh();
                free(entrs);
        }

exit:
        endwin();

        return 0;
}
