#include <sys/types.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>

/* globals */
static const char *envs[] = {
        "HOME",
        "USER",
        "EDITOR",
        "BROWSER",
        "PAGER"
};

atatic unsigned int maxcols = 3;
static int nfiles = 0;

/* function declarations */
static void curses_init(void);
static void files_list(const char *);

/* function implementations */
void
curses_init(void)
{
        initscr();
        noecho();
        cbreak();
        curs_set(0);
        keypad(stdscr, 1);
}

void
files_list(const char *dirname)
{
        DIR *dir;
        struct dirent *dp;
        char *filename;

        dir = opendir(dirname);
        // get hidden files flag
        while ((dp = readdir(dir)) != NULL) {
                if (strcmp(dp->d_name, dirname) &&  strcmp(dp->d_name, ".")
                &&  strcmp(dp->d_name, "..")) {
                        filename = dp->d_name;
                        printw("%s\n", filename);
                        nfiles++;
                }
        }
        refresh();
        closedir(dir);
}

int
main(int argc, char *argv[])
{
        char cwd[256], *curdir;
        int c, i;

        if ((curdir = getcwd(cwd, sizeof(cwd))) == NULL) {
                return -1;
        }
        curses_init();

        while ((c = getch()) != 'q') {
                erase();

                attron(A_BOLD);
                mvprintw(0, 0, "%s\n", curdir);
                attroff(A_BOLD);

                files_list(curdir);

                switch (c) {
                case 'h':        /* FALLTHROUGH */ 
                case KEY_LEFT:
                        chdir("..");
                        break;
                case 'l':       /* FALLTHROUGH */
                case KEY_RIGHT:
                        break;
                }
                curdir = getcwd(cwd, sizeof(cwd));

                refresh();
        }

        endwin();

        return 0;
}
