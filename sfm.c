#include <sys/types.h>
#include <sys/wait.h>

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

#define YMAX            (getmaxy(stdscr))
#define XMAX            (getmaxx(stdscr))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define ARRLEN(x)       (sizeof(x) / sizeof(x[0]))
#define ISDIGIT(x)      ((unsigned int)(x) - '0' <= 9)
#define SEL_CORRECT     (sel = ((sel < 0) ? 0 : (sel > nfiles - 1) ? nfiles - 1 : sel))

/* structs, unions and enums */
typedef struct {
        // may use just dirent
        char            *name;
        char             abspath[PATH_MAX];
        off_t            size;
        int              nchld;
        unsigned short   nmlen;
        unsigned char    type;
} Entry;

typedef union {
        int n;
        const char *d;
        const void *f;
} Arg;

typedef struct {
        int mod;
        int key;
        void (*func)(const Arg *arg);
        const Arg arg;
} Key;

enum NavFlags {
        NAV_LEFT,
        NAV_RIGHT,
        NAV_UP,
        NAV_DOWN,
        NAV_TOP,
        NAV_BOTTOM
};

// add sort flags (by size, name etc)

/* globals */
static unsigned long nfiles = 0;
static int sel = 0;
static Entry *entrs = NULL;

/* function declarations */
static void      cursesinit(void);
static int       entriescount(const char *);
static Entry    *entriesget(const char *);
static void      pathdraw(const char *);
static void      dirdraw(const Entry *);
static void      promptget(const Arg *);
static void      nav(const Arg *);
static void      cd(const Arg *);
static void      spawn(const Arg *);
static void      quit(const Arg *);

#include "config.h"

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

int
entriescount(const char *path)
{
        DIR *dir;
        struct dirent *dp;
        int nentrs = 0;
        
        if ((dir = opendir(path)) == NULL)
                return -1; // this might cause bugs
        // get hidden files flag
        while ((dp = readdir(dir)) != NULL) {
                if (strcmp(dp->d_name, path) &&  strcmp(dp->d_name, ".")
                &&  strcmp(dp->d_name, ".."))
                        nentrs++;
        }
        closedir(dir);
        return nentrs;
}

Entry *
entriesget(const char *path)
{
        DIR *dir;
        struct dirent *dp;
        Entry *entrs;
        
        if ((entrs = malloc(entriescount(path) * sizeof(Entry))) == NULL)
                return NULL;
        if ((dir = opendir(path)) == NULL)
                return NULL;
        nfiles = 0;
        // get hidden files flag
        while ((dp = readdir(dir)) != NULL) {
                if (strcmp(dp->d_name, path) &&  strcmp(dp->d_name, ".")
                &&  strcmp(dp->d_name, "..")) {
                        entrs[nfiles].name = dp->d_name;
                        entrs[nfiles].nmlen = strlen(dp->d_name);
                        entrs[nfiles].type = dp->d_type;
                        sprintf(entrs[nfiles].abspath, "%s/%s", path, dp->d_name);
                        entrs[nfiles].nchld = entriescount(entrs[nfiles].abspath);
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
        /*mvhline(1, 0, ACS_HLINE, XMAX);*/
}

void
dirdraw(const Entry *entrs)
{
        int i = 0;

        SEL_CORRECT;
        for (; i < nfiles; i++) {
                if (i == sel)
                        attron(A_REVERSE);
                mvprintw(i + 1, 0, "%s\n", entrs[i].name);
                // align numbers
                if (entrs[i].type == DT_DIR)
                        mvprintw(i + 1, 50, "%12.ld\n", entrs[i].nchld);
                attroff(A_REVERSE);
        }
}

// handle user given shell cmds
void
spawn(const Arg *arg)
{
        // huh?
        char buf[BUFSIZ];

        snprintf(buf, BUFSIZ, ((char **)arg->f)[2], entrs[sel].name);
        printw("Confirm action %s (y/n): ", ((char **)arg->f)[0]);
        if (getch() == 'y') {
                /*execvp(*((char **)arg->f), (char **)arg->f);*/
                printw(" done");
        }
        getch();
}                                                
                                                 
void                        
promptget(const Arg *arg)
{                                                
        char buf[BUFSIZ];                        
                                                 
        move(getmaxy(stdscr) - 1, 0);            
        echo();                                  
        curs_set(1);                             
        printw(":");                             
        getnstr(buf, BUFSIZ);                      
        noecho();                                
        curs_set(0);                             
}

void
nav(const Arg *arg)
{
        switch (arg->n) {
        case NAV_LEFT:
                chdir("..");
                break;
        case NAV_RIGHT:
                if (entrs[sel].name != NULL) {
                        // handle symlinks
                        if (entrs[sel].type == DT_DIR)
                                chdir(entrs[sel].name);
                        /*else if (entrs[sel].type == DT_REG)*/
                                /*spawn();*/
                }
                break;
        case NAV_UP:
                sel--;
                break;
        case NAV_DOWN:
                sel++;
                break;
        case NAV_TOP:
                sel = 0;
                break;
        case NAV_BOTTOM:
                sel = nfiles - 1;
                break;
        }
}

void
cd(const Arg *arg)
{
        chdir(arg->d);
}

void
quit(const Arg *arg)
{
        endwin();
        exit(0);
}

int
main(int argc, char *argv[])
{
        /*Entry *entrs;*/
        char cwd[PATH_MAX], *curdir;
        int c, i;

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

                c = getch();
                for (i = 0; i < ARRLEN(keys); i++) {
                        // handle same key combinations
                        // and same key but without mod
                        if (c == keys[i].mod) {
                                mvaddch(YMAX - 1, 0, c);
                                c = getch();
                        }
                        if (c == keys[i].key)
                                keys[i].func(&(keys[i].arg));
                }

                refresh();
                free(entrs);
        }

        return 0;
}
