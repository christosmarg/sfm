#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ncurses.h>

#define PATH_MAX        1024 // syslimits' PATH_MAX?
#define HOST_NAME_MAX   _POSIX_HOST_NAME_MAX
#define LOGIN_NAME_MAX  _POSIX_LOGIN_NAME_MAX

#define PARENT_COL      0
#define MAIN_COL        ((XMAX / 2) / 3)
#define CHILD_COL       (XMAX / 2)

#define YMAX            (getmaxy(stdscr))
#define XMAX            (getmaxx(stdscr))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define ARRLEN(x)       (sizeof(x) / sizeof(x[0]))
#define ISDIGIT(x)      ((unsigned int)(x) - '0' <= 9)
#define SEL_CORRECT \
        (msel = ((msel < 0) ? 0 : (msel > nfiles - 1) ? nfiles - 1 : msel))

/* typedefs */
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef long long ll;
typedef unsigned long long ull;

/* structs, unions and enums */
typedef struct {
        FTSENT  *fts;
        ulong    nchld;
        ulong    nparents;
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

enum {
        NAV_LEFT,
        NAV_RIGHT,
        NAV_UP,
        NAV_DOWN,
        NAV_TOP,
        NAV_BOTTOM,
        NAV_SHOWALL,
};

/* globals */
static Entry *entrs = NULL;     /* main directory's entries */
static ulong nfiles = 0;        /* number of files in directory */
static long msel = 0;           /* main sel */
static long psel = 0;           /* parent sel */
static long csel = 0;           /* child sel */
static uchar f_showall = 0;     /* show hidden files */
static uchar f_redraw = 0;      /* redraw screen */

/* function pointers */
static int (*sortfn)(const FTSENT *, const FTSENT *);

/* function declarations */
static void      cursesinit(void);
static int       entriescount(char *);
static Entry    *entriesget(char *);
static void      pathdraw(const char *);
static void      dirpreview(const Entry *, int, int, int, ulong);
static void      filepreview(const Entry *);
static void      statsprint(const Entry *);
static void      promptget(const Arg *);
static void      nav(const Arg *);
static void      cd(const Arg *);
static void      spawn(const Arg *);
static void      quit(const Arg *);
static void     *emalloc(size_t);
static void      die(const char *, ...);

#include "config.h"

/* function implementations */
static inline int
namecmp(const FTSENT *a, const FTSENT *b)
{
        return (strcoll(a->fts_name, b->fts_name));
}

void
cursesinit(void)
{
        initscr();
        noecho();
        cbreak();
        curs_set(0);
        keypad(stdscr, 1);
        /*scrollok(stdscr, 1);*/
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
                        /*if (!f_showall)*/
                        if (cur->fts_name[0] == '.')
                                continue;
                        n++;
                }
        }
        fts_close(ftsp);
        return n;
}

Entry *
entriesget(char *path)
{
        FTS *ftsp;
        FTSENT *p, *chp, *cur;
        Entry *dents;
        char *args[] = {path, NULL};
        int i = 0;
        
        dents = emalloc(entriescount(path) * sizeof(Entry));
        if ((ftsp = fts_open(args, FTS_NOCHDIR, NULL)) == NULL)
                return NULL;
        while ((p = fts_read(ftsp)) != NULL) {
                fts_set(ftsp, p, FTS_SKIP);
                chp = fts_children(ftsp, 0);
                for (cur = chp; cur; cur = cur->fts_link) {
                        /*if (!f_showall)*/
                        if (cur->fts_name[0] == '.')
                                continue;
                        dents[i].fts = cur;
                        /*dents[i].nchld = entriescount(dents[i].fts->fts_name);*/
                        i++;
                }
                /* TODO: count parents only one time, as it's the same for everyone */
                /*dents[i].nparents = entriescount("..");*/
        }
        fts_close(ftsp);
        return dents;
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
dirpreview(const Entry *dents, int n, int col, int len, ulong sel)
{
        char buf[FILENAME_MAX];
        int i = 0;

        for (; i < n && i < YMAX; i++) {
                if (i == sel)
                        attron(A_REVERSE);
                if (dents[i].fts->fts_info == FTS_D)
                        attron(A_BOLD);
                sprintf(buf, "%-*s", len, dents[i].fts->fts_name);
                // handle empty dir
                mvprintw(i + 2, col, " %s \n", buf);
                if (col == MAIN_COL) {
                        // align numbers properly
                        switch (dents[i].fts->fts_info) {
                        case FTS_D:
                                /*mvprintw(i + 2, CHILD_COL, "%12d \n",*/
                                         /*dents[i].nchld);*/
                                break;
                        case FTS_F:
                                mvprintw(i + 2, CHILD_COL, "%10d B \n",
                                         dents[i].fts->fts_statp->st_size);
                                break;
                        }
                }
                attroff(A_BOLD);
                attroff(A_REVERSE);
        }
}

void
statsprint(const Entry *entry)
{
        struct tm *tm;

        tm = localtime(&entry->fts->fts_statp->st_ctime);
        mvprintw(YMAX - 1, 0, "%c%c%c%c%c%c%c%c%c%c %dB %s",
                 (S_ISDIR(entry->fts->fts_statp->st_mode)) ? 'd' : '-',
                 entry->fts->fts_statp->st_mode & S_IRUSR ? 'r' : '-',
                 entry->fts->fts_statp->st_mode & S_IWUSR ? 'w' : '-',
                 entry->fts->fts_statp->st_mode & S_IXUSR ? 'x' : '-',
                 entry->fts->fts_statp->st_mode & S_IRGRP ? 'r' : '-',
                 entry->fts->fts_statp->st_mode & S_IWGRP ? 'w' : '-',
                 entry->fts->fts_statp->st_mode & S_IXGRP ? 'x' : '-',
                 entry->fts->fts_statp->st_mode & S_IROTH ? 'r' : '-',
                 entry->fts->fts_statp->st_mode & S_IWOTH ? 'w' : '-',
                 entry->fts->fts_statp->st_mode & S_IXOTH ? 'x' : '-',
                 entry->fts->fts_statp->st_size, asctime(tm));
        // align
        mvprintw(YMAX - 1, XMAX - 10, "%ld/%ld\n", msel + 1, nfiles);
}

void
filepreview(const Entry *entry)
{
        FILE *fp;
        char buf[BUFSIZ];
        int ln = 0;

        /*if ((fp = fopen(entry->name, "r")) == NULL)*/
                /*return;*/
        // replace XMAX / 2
        while (fgets(buf, BUFSIZ, fp) && ln < YMAX) {
                if (strlen(buf) > XMAX / 2)
                        buf[XMAX / 2 - 2] = '\0';
                mvprintw(ln++ + 2, CHILD_COL, "%s\n", buf);
        }
        fclose(fp);
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
                /*if (entrs[msel].name != NULL) {*/
                        // handle symlinks
                        /*if (entrs[msel].type == FTS_D)*/
                                /*chdir(entrs[msel].name);*/
                        /*else if (entrs[msel].type == DT_REG)*/
                                /*spawn();*/
                /*}*/
                break;
        case NAV_UP:
                msel--;
                break;
        case NAV_DOWN:
                msel++;
                break;
        case NAV_TOP:
                msel = 0;
                break;
        case NAV_BOTTOM:
                msel = nfiles - 1;
                break;
        case NAV_SHOWALL:
                f_showall = !f_showall;
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
        Entry *parent = NULL, *child = NULL;
        char cwd[PATH_MAX], *curdir, prevdir[PATH_MAX] = {0};
        int c, i;

        // check for NCURSES_VERSION
        cursesinit();
        if ((curdir = getcwd(cwd, sizeof(cwd))) == NULL)
                die("can't get cwd");

        for (;;) {
                erase();

                curdir = getcwd(cwd, sizeof(cwd));
                if (strcmp(curdir, prevdir) != 0 || f_redraw) {
                        if (entrs != NULL)
                                free(entrs);
                        if (parent != NULL)
                                free(parent);
                        /*if (child != NULL)*/
                                /*free(child);*/
                        nfiles = entriescount(curdir);
                        if ((entrs = entriesget(curdir)) == NULL)
                                continue;
                        /*if ((parent = entriesget("..")) == NULL)*/
                                /*continue;*/
                        /*if (entrs[msel].type == DT_DIR)*/
                                /*if ((child = entriesget(entrs[msel].name)) == NULL)*/
                                        /*continue;*/
                        strcpy(prevdir, curdir);
                        f_redraw = 0;
                }

                SEL_CORRECT;
                pathdraw(curdir);
                /*if (strcmp(curdir, "/") != 0)*/
                        /*dirpreview(parent, entrs[msel].nparents, PARENT_COL,*/
                                   /*MAIN_COL - 3, psel);*/
                dirpreview(entrs, nfiles, MAIN_COL, CHILD_COL, msel);
                statsprint(&entrs[msel]);
                /*dirpreview(child, entrs[msel].nchld, CHILD_COL + 2 + 12,*/
                           /*CHILD_COL - MAIN_COL, csel);*/
                /*if (entrs[msel].type == DT_REG)*/
                        /*filepreview(&entrs[msel]);*/

                c = getch();
                for (i = 0; i < ARRLEN(keys); i++) {
                        // handle same key combinations
                        // and same key but without mod
                        if (c == keys[i].mod) {
                                // may add lf and ranger style cmd printing
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
