#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ncurses.h>

#ifndef PATH_MAX
#define PATH_MAX        1024
#endif /* PATH_MAX */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX   _POSIX_HOST_NAME_MAX
#endif /* HOST_NAME_MAX */
#ifndef LOGIN_NAME_MAX
#define LOGIN_NAME_MAX  _POSIX_LOGIN_NAME_MAX
#endif /* LOGIN_NAME_MAX */

#define YMAX            (getmaxy(stdscr))
#define XMAX            (getmaxx(stdscr))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define ARRLEN(x)       (sizeof(x) / sizeof(x[0]))
#define ISDIGIT(x)      ((unsigned int)(x) - '0' <= 9)
#define SEL_CORRECT \
        win->sel = ((win->sel < 0) ? 0 : (win->sel > win->nfiles - 1) \
                    ? win->nfiles - 1 : win->sel);

/* type definitions */
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef long long ll;
typedef unsigned long long ull;

/* structs, unions and enums */
typedef struct {
        FTSENT  *fts;
// useless?
} Entry;

typedef struct {
        WINDOW  *w;
        Entry   *ents;
        ulong    nfiles;
        long     sel;
        int      len;
        int      id;
} Win;

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

enum {
        W_PARENT,
        W_MAIN,
        W_CHILD,
        W_FILE,
};

enum {
        NAV_LEFT,
        NAV_RIGHT,
        NAV_UP,
        NAV_DOWN,
        NAV_TOP,
        NAV_BOTTOM,
        NAV_SHOWALL,
        NAV_FPREVIEW,
};

/* globals */
static Win *win = NULL;         /* main display */
static uchar f_showall = 0;     /* show hidden files */
static uchar f_redraw = 0;      /* redraw screen */
static uchar f_fpreview = 1;    /* preview files */

/* function pointers */
static int (*sortfn)(const FTSENT *, const FTSENT *);

/* function declarations */
static void      cursesinit(void);
static int       entriescount(char *);
static Entry    *entriesget(char *, ulong);
static void      pathdraw(const char *);
static void      dirpreview(Win *);
static void      filepreview(void);
static void      statsprint(void);
static void      promptget(const Arg *);
static void      nav(const Arg *);
static void      cd(const Arg *);
static void      spawn(const Arg *);
static void      quit(const Arg *);
static void     *emalloc(size_t);
static void      die(const char *, ...);

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
        /*scrollok(stdscr, 1);*/

        win = emalloc(sizeof(Win));
        win->ents = NULL;
        win->sel = 0;
        win->len = XMAX / 2;
        win->w = newwin(YMAX - 2, win->len, 2, 0);
        box(win->w, winborder, winborder);
}

int
entriescount(char *path)
{
        FTS *ftsp;
        FTSENT *p, *chp, *cur;
        char *args[] = {path, NULL};
        int n = 0;

        if ((ftsp = fts_open(args, FTS_NOCHDIR, NULL)) == NULL)
                return 0;
        while ((p = fts_read(ftsp)) != NULL) {
                fts_set(ftsp, p, FTS_SKIP);
                chp = fts_children(ftsp, 0);
                for (cur = chp; cur; cur = cur->fts_link) {
                        if (!f_showall && cur->fts_name[0] == '.')
                                continue;
                        n++;
                }
        }
        fts_close(ftsp);
        return n;
}

Entry *
entriesget(char *path, ulong n)
{
        Entry *ents;
        FTS *ftsp;
        FTSENT *p, *chp, *cur;
        char *args[] = {path, NULL};
        int i = 0;
        
        ents = emalloc(n * sizeof(Entry));
        if ((ftsp = fts_open(args, FTS_NOCHDIR, NULL)) == NULL)
                return NULL;
        while ((p = fts_read(ftsp)) != NULL) {
                fts_set(ftsp, p, FTS_SKIP);
                chp = fts_children(ftsp, 0);
                for (cur = chp; cur; cur = cur->fts_link) {
                        if (!f_showall && cur->fts_name[0] == '.')
                                continue;
                        ents[i].fts = cur;
                        i++;
                }
        }
        fts_close(ftsp);
        return ents;
}

void
pathdraw(const char *path)
{
        char host[HOST_NAME_MAX];
        char user[LOGIN_NAME_MAX];

        gethostname(host, HOST_NAME_MAX);
        getlogin_r(user, LOGIN_NAME_MAX);
        attron(A_BOLD);
        mvprintw(0, 0, "%s@%s:%s\n", user, host, path);
        attroff(A_BOLD);
        mvhline(1, 0, ACS_HLINE, XMAX);
}

void
dirpreview(Win *win)
{
        char buf[FILENAME_MAX];
        int i = 0;

        for (; i < win->nfiles && i < YMAX; i++) {
                if (i == win->sel)
                        wattron(win->w, A_REVERSE);
                if (win->ents[i].fts->fts_info == FTS_D)
                        wattron(win->w, A_BOLD);
                sprintf(buf, "%-*s", win->len, win->ents[i].fts->fts_name);
                mvwprintw(win->w, i, 0, " %s ", buf);
                switch (win->ents[i].fts->fts_info) {
                case FTS_D:
                        /*mvwprintw(win->w, i, win->len, "%12d ",*/
                                  /*entriescount(win->ents[i].fts->fts_name));*/
                        break;
                case FTS_F:
                        // no hardcoding please.
                        mvwprintw(win->w, i, win->len - 13, "%10d B ",
                                 win->ents[i].fts->fts_statp->st_size);
                        break;
                }
                wattroff(win->w, A_BOLD | A_REVERSE);
                refresh();
                wrefresh(win->w);
        }
}

void
statsprint(void)
{
        Entry *ent = &win->ents[win->sel];
        struct tm *tm;

        tm = localtime(&ent->fts->fts_statp->st_ctime);
        mvprintw(YMAX - 1, 0, "%ld/%ld %c%c%c%c%c%c%c%c%c%c %dB %s",
                 win->sel + 1, win->nfiles,
                 (S_ISDIR(ent->fts->fts_statp->st_mode)) ? 'd' : '-',
                 ent->fts->fts_statp->st_mode & S_IRUSR ? 'r' : '-',
                 ent->fts->fts_statp->st_mode & S_IWUSR ? 'w' : '-',
                 ent->fts->fts_statp->st_mode & S_IXUSR ? 'x' : '-',
                 ent->fts->fts_statp->st_mode & S_IRGRP ? 'r' : '-',
                 ent->fts->fts_statp->st_mode & S_IWGRP ? 'w' : '-',
                 ent->fts->fts_statp->st_mode & S_IXGRP ? 'x' : '-',
                 ent->fts->fts_statp->st_mode & S_IROTH ? 'r' : '-',
                 ent->fts->fts_statp->st_mode & S_IWOTH ? 'w' : '-',
                 ent->fts->fts_statp->st_mode & S_IXOTH ? 'x' : '-',
                 ent->fts->fts_statp->st_size,
                 asctime(tm));
}

void
filepreview(void)
{
        WINDOW *fw;
        FILE *fp;
        Entry *ent;
        char buf[BUFSIZ];
        size_t maxlen = XMAX / 2;
        int ln = 0;

        if (f_fpreview) {
                ent = &win->ents[win->sel];
                if ((fp = fopen(ent->fts->fts_name, "r")) == NULL)
                        return;
                fw = newwin(YMAX - 2, maxlen, 2, maxlen);
                while (fgets(buf, BUFSIZ, fp) && ln < YMAX) {
                        if (strlen(buf) > maxlen)
                                buf[maxlen] = '\0';
                        mvwprintw(fw, ln++, 0, "%s\n", buf);
                }
                fclose(fp);
                refresh();
                wrefresh(fw);
        }
}

// handle user given shell cmds
void
spawn(const Arg *arg)
{
        /*execvp(*((char **)arg->f), (char **)arg->f);*/
}                                                
                                                 
void                        
promptget(const Arg *arg)
{  
}

void
nav(const Arg *arg)
{
        FTSENT *ftsp = win->ents[win->sel].fts;

        switch (arg->n) {
        case NAV_LEFT:
                chdir("..");
                break;
        case NAV_RIGHT:
                if (ftsp->fts_info == FTS_D)
                        chdir(ftsp->fts_name);
                break;
        case NAV_UP:
                win->sel--;
                break;
        case NAV_DOWN:
                win->sel++;
                break;
        case NAV_TOP:
                win->sel = 0;
                break;
        case NAV_BOTTOM:
                win->sel = win->nfiles - 1;
                break;
        case NAV_SHOWALL:
                f_showall = !f_showall;
                f_redraw = 1;
                break;
        case NAV_FPREVIEW:
                f_fpreview = !f_fpreview;
                f_redraw = 1;
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
        if (win->w != NULL)
                delwin(win->w);
        if (win->ents != NULL)
                free(win->ents);
        free(win);
        endwin();
        exit(0);
}

void *
emalloc(size_t nb)
{
        void *p;

        if ((p = malloc(nb)) == NULL)
                die("emalloc:");
        return p;
}

void
die(const char *fmt, ...)
{
        va_list args;
        
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);

        if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
                fputc(' ', stderr);
                perror(NULL);
        } else
                fputc('\n', stderr);
        exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
        char cwd[PATH_MAX], *curdir, prevdir[PATH_MAX] = {0};
        int c, i;

        cursesinit();
        if ((curdir = getcwd(cwd, sizeof(cwd))) == NULL)
                die("sfm: can't get cwd");

        for (;;) {
                erase();

                curdir = getcwd(cwd, sizeof(cwd));
                if (strcmp(curdir, prevdir) != 0 || f_redraw) {
                        if (win->ents != NULL)
                                free(win->ents);
                        win->nfiles = entriescount(curdir);
                        if ((win->ents = entriesget(curdir, win->nfiles)) == NULL)
                                continue;
                        strcpy(prevdir, curdir);
                        f_redraw = 0;
                }

                SEL_CORRECT;
                pathdraw(curdir);
                dirpreview(win);
                statsprint();
                if (win->ents[win->sel].fts->fts_info == FTS_F)
                        filepreview();

                c = getch();
                for (i = 0; i < ARRLEN(keys); i++) {
                        if (c == keys[i].mod) {
                                mvaddch(YMAX - 1, 0, c);
                                c = getch();
                        }
                        if (c == keys[i].key)
                                keys[i].func(&(keys[i].arg));
                }

                refresh();
        }

        return 0;
}
