#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ncurses.h>

// why tho
#ifndef DT_DIR
#define DT_DIR 4
#endif /* DT_DIR */

#ifndef DT_REG
#define DT_REG 8
#endif /* DT_REG */

#define PATH_MAX        1024 // syslimits' PATH_MAX?
#define HOST_NAME_MAX   _POSIX_HOST_NAME_MAX
#define LOGIN_NAME_MAX  _POSIX_LOGIN_NAME_MAX

#define PARENT_COL      0
#define FOCUS_COL       ((XMAX / 2) / 3)
#define CHILD_COL       (XMAX / 2)

#define YMAX            (getmaxy(stdscr))
#define XMAX            (getmaxx(stdscr))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define ARRLEN(x)       (sizeof(x) / sizeof(x[0]))
#define ISDIGIT(x)      ((unsigned int)(x) - '0' <= 9)
#define SEL_CORRECT     (msel = ((msel < 0) ? 0 : (msel > nfiles - 1) ? nfiles - 1 : msel))

/* structs, unions and enums */
typedef struct {
        struct stat     stat;
        char            name[FILENAME_MAX];
        char            abspath[PATH_MAX];
        /*char            parentpath[PATH_MAX];*/
        unsigned int    nchld;
        unsigned int    nparents;
        unsigned short  nmlen;
        unsigned char   type;
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
        LEVEL_PARENT = -1,
        LEVEL_MAIN,
        LEVEL_CHILD
};

enum {
        NAV_LEFT,
        NAV_RIGHT,
        NAV_UP,
        NAV_DOWN,
        NAV_TOP,
        NAV_BOTTOM,
        NAV_SHOWALL,
};

enum {
        ENTSORT_NAME_ASC,
        ENTSORT_NAME_DESC,
};

/* globals */
static Entry *entrs = NULL;
static unsigned long nfiles = 0;
// change to ul
static int msel = 0; /* main sel */
static int psel = 0; /* parent sel */
static int csel = 0; /* child sel */
static int showall = 0;
static int needredraw = 0;

/* function declarations */
static void      cursesinit(void);
static int       entriescount(const char *);
static Entry    *entriesget(const char *);
static void      pathdraw(const char *);
static void      dirpreview(const Entry *, int, int, int, int, int);
static void      filepreview(const Entry *);
static void      statsprint(const Entry *);
static void      promptget(const Arg *);
static void      nav(const Arg *);
static void      cd(const Arg *);
static void      entsort(const Arg *);
static void      spawn(const Arg *);
static void      quit(const Arg *);
static void      run(void);
static void     *emalloc(size_t);
static void      die(const char *, ...);

#include "config.h"

/* function implementations */
static inline int
entcmpnameasc(const void *lhs, const void *rhs)
{
        return (strcmp(((Entry *)lhs)->name, ((Entry *)rhs)->name) < 0);
}

static inline int
entcmpnamedesc(const void *lhs, const void *rhs)
{
        return (strcmp(((Entry *)lhs)->name, ((Entry *)rhs)->name) < 0);
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
entriescount(const char *path)
{
        DIR *dir;
        struct dirent *dp;
        int n = 0;
        
        if ((dir = opendir(path)) == NULL)
                return 0;
        while ((dp = readdir(dir)) != NULL) {
                if (!showall && dp->d_name[0] == '.')
                        continue;
                if (strcmp(dp->d_name, path) && strcmp(dp->d_name, ".")
                &&  strcmp(dp->d_name, ".."))
                        n++;
        }
        closedir(dir);
        return n;
}

Entry *
entriesget(const char *path)
{
        DIR *dir;
        struct dirent *dp;
        Entry *dents;
        int i = 0;
        
        dents = emalloc(entriescount(path) * sizeof(Entry));
        if ((dir = opendir(path)) == NULL)
                return NULL;
        while ((dp = readdir(dir)) != NULL) {
                if (!showall && dp->d_name[0] == '.')
                        continue;
                if (strcmp(dp->d_name, path) && strcmp(dp->d_name, ".")
                &&  strcmp(dp->d_name, "..")) {
                        strcpy(dents[i].name, dp->d_name);
                        dents[i].nmlen = strlen(dp->d_name);
                        dents[i].type = dp->d_type;
                        sprintf(dents[i].abspath, "%s/%s", path, dp->d_name);
                        dents[i].nchld = entriescount(dents[i].name);
                        dents[i].nparents = entriescount("..");
                        stat(dents[i].abspath, &dents[i].stat);
                        i++;
                }
        }
        closedir(dir);
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
dirpreview(const Entry *dents, int n, int col, int len, int sel, int level)
{
        char buf[FILENAME_MAX];
        int i = 0;

        for (; i < n && i < YMAX; i++) {
                if (i == sel)
                        attron(A_REVERSE);
                if (dents[i].type == DT_DIR)
                        attron(A_BOLD);
                sprintf(buf, "%-*s", len, dents[i].name);
                // handle empty dir
                mvprintw(i + 2, col, " %s \n", buf);
                if (level == 0) {
                        // align numbers properly
                        if (dents[i].type == DT_DIR)
                                mvprintw(i + 2, CHILD_COL, "%12d \n",
                                         dents[i].nchld);
                        else if (dents[i].type == DT_REG)
                                mvprintw(i + 2, CHILD_COL, "%10d B \n",
                                         dents[i].stat.st_size);
                }
                attroff(A_BOLD);
                attroff(A_REVERSE);
        }
}

void
statsprint(const Entry *entry)
{
        struct tm *tm;

        tm = localtime(&entry->stat.st_ctime);
        mvprintw(YMAX - 1, 0, "%c%c%c%c%c%c%c%c%c%c %dB %s",
                 (S_ISDIR(entry->stat.st_mode)) ? 'd' : '-',
                 entry->stat.st_mode & S_IRUSR ? 'r' : '-',
                 entry->stat.st_mode & S_IWUSR ? 'w' : '-',
                 entry->stat.st_mode & S_IXUSR ? 'x' : '-',
                 entry->stat.st_mode & S_IRGRP ? 'r' : '-',
                 entry->stat.st_mode & S_IWGRP ? 'w' : '-',
                 entry->stat.st_mode & S_IXGRP ? 'x' : '-',
                 entry->stat.st_mode & S_IROTH ? 'r' : '-',
                 entry->stat.st_mode & S_IWOTH ? 'w' : '-',
                 entry->stat.st_mode & S_IXOTH ? 'x' : '-',
                 entry->stat.st_size, asctime(tm));
        // align
        mvprintw(YMAX - 1, XMAX - 10, "%ld/%ld\n", msel + 1, nfiles);
}

void
filepreview(const Entry *entry)
{
        FILE *fp;
        char buf[BUFSIZ];
        int ln = 0;

        if ((fp = fopen(entry->name, "r")) == NULL)
                return;
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
                if (entrs[msel].name != NULL) {
                        // handle symlinks
                        if (entrs[msel].type == DT_DIR)
                                chdir(entrs[msel].name);
                        /*else if (entrs[msel].type == DT_REG)*/
                                /*spawn();*/
                }
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
                showall = !showall;
                needredraw = 1;
                break;
        }
}

void
entsort(const Arg *arg)
{
        switch (arg->n) {
        case ENTSORT_NAME_ASC:
                qsort(entrs, nfiles, sizeof(Entry), entcmpnameasc);
                break;
        case ENTSORT_NAME_DESC:
                qsort(entrs, nfiles, sizeof(Entry), entcmpnamedesc);
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

void
run(void)
{
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
        run();

        for (;;) {
                erase();

                curdir = getcwd(cwd, sizeof(cwd));
                if (strcmp(curdir, prevdir) != 0 || needredraw) {
                        if (entrs != NULL)
                                free(entrs);
                        if (parent != NULL)
                                free(parent);
                        /*if (child != NULL)*/
                                /*free(child);*/
                        nfiles = entriescount(curdir);
                        if ((entrs = entriesget(curdir)) == NULL)
                                continue;
                        if ((parent = entriesget("..")) == NULL)
                                continue;
                        /*if (entrs[msel].type == DT_DIR)*/
                                /*if ((child = entriesget(entrs[msel].name)) == NULL)*/
                                        /*continue;*/
                        // is this a memleak?
                        strcpy(prevdir, curdir);
                        needredraw = 0;
                }

                SEL_CORRECT;
                pathdraw(curdir);
                /*if (strcmp(curdir, "/") != 0)*/
                        /*dirpreview(parent, entrs[msel].nparents, PARENT_COL,*/
                                   /*FOCUS_COL - 3, psel, LEVEL_PARENT);*/
                dirpreview(entrs, nfiles, FOCUS_COL, CHILD_COL, msel, LEVEL_MAIN);
                statsprint(&entrs[msel]);
                /*dirpreview(child, entrs[msel].nchld, CHILD_COL + 2, CHILD_COL - FOCUS_COL, csel, LEVEL_CHILD);*/
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
