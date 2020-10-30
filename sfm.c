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

#define PARENT_COL      0
#define FOCUS_COL       40
#define FOCUS_NCHLD_COL 80
#define CHILD_COL       95

#define YMAX            (getmaxy(stdscr))
#define XMAX            (getmaxx(stdscr))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define ARRLEN(x)       (sizeof(x) / sizeof(x[0]))
#define ISDIGIT(x)      ((unsigned int)(x) - '0' <= 9)
#define SEL_CORRECT     (fsel = ((fsel < 0) ? 0 : (fsel > nfiles - 1) ? nfiles - 1 : fsel))

/* structs, unions and enums */
typedef struct {
        char            name[NAME_MAX];
        char            abspath[PATH_MAX];
        off_t           size;
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
static int fsel = 0; /* focused sel */
static int showall = 0;

/* function declarations */
static void      cursesinit(void);
static int       entriescount(const char *);
static Entry    *entriesget(const char *);
static void      pathdraw(const char *);
static void      focusdraw(const Entry *);
static void      dirpreview(const Entry *, int, int, int);
static void      filepreview(const Entry *);
static void      promptget(const Arg *);
static void      nav(const Arg *);
static void      cd(const Arg *);
static void      entsort(const Arg *);
static void      spawn(const Arg *);
static void      quit(const Arg *);
static void      run(void);
static void      die(const char *, ...);

#include "config.h"

/* function implementations */
static inline int
entcmpnameasc(const void *lhs, const void *rhs)
{
        return (strcmp(((Entry *)lhs)->name, ((Entry *)rhs)->name) > 0);
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
        
        if ((dents = malloc(entriescount(path) * sizeof(Entry))) == NULL)
                return NULL;
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
                        i++;
                }
        }
        closedir(dir);
        return dents;
}

void
pathdraw(const char *path)
{
        mvprintw(YMAX - 1, XMAX - 10, "%ld/%ld\n", fsel + 1, nfiles); // shouldn't be here
        attron(A_BOLD);
        mvprintw(0, 0, "%s\n", path);
        attroff(A_BOLD);
        mvhline(1, 0, ACS_HLINE, XMAX);
}

void
focusdraw(const Entry *dents)
{
        int i = 0;

        for (; i < nfiles && i < YMAX; i++) {
                if (i == fsel)
                        attron(A_REVERSE);
                mvprintw(i + 2, FOCUS_COL, "%s\n", dents[i].name);
                // align numbers properly
                if (dents[i].type == DT_DIR)
                        mvprintw(i + 2, FOCUS_NCHLD_COL, "%12.ld\n", dents[i].nchld);
                attroff(A_REVERSE);
        }
}

// might merge with focusdraw
void
dirpreview(const Entry *dents, int n, int sel, int col)
{
        int i = 0;

        for (; i < n && i < YMAX; i++) {
                if (i == sel)
                        attron(A_REVERSE);
                mvprintw(i + 2, col, "%s\n", dents[i].name);
                attroff(A_REVERSE);
        }
}

void
filepreview(const Entry *entry)
{
        FILE *fp;
        char buf[BUFSIZ];
        int ln = 0;

        if ((fp = fopen(entry->name, "r")) == NULL)
                return;
        while (fgets(buf, BUFSIZ, fp) && ln < YMAX)
                mvprintw(ln++ + 2, 120, "%s\n", buf);
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
                if (entrs[fsel].name != NULL) {
                        // handle symlinks
                        if (entrs[fsel].type == DT_DIR)
                                chdir(entrs[fsel].name);
                        /*else if (entrs[fsel].type == DT_REG)*/
                                /*spawn();*/
                }
                break;
        case NAV_UP:
                fsel--;
                break;
        case NAV_DOWN:
                fsel++;
                break;
        case NAV_TOP:
                fsel = 0;
                break;
        case NAV_BOTTOM:
                fsel = nfiles - 1;
                break;
        case NAV_SHOWALL:
                showall = !showall;
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
                if (strcmp(curdir, prevdir) != 0) {
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
                        /*if (entrs[fsel].type == DT_DIR)*/
                                /*if ((child = entriesget(entrs[fsel].name)) == NULL)*/
                                        /*continue;*/
                        // is this a memleak?
                        strcpy(prevdir, curdir);
                }

                SEL_CORRECT;
                pathdraw(curdir);
                if (strcmp(curdir, "/") != 0)
                        dirpreview(parent, entrs[fsel].nparents, fsel, PARENT_COL);
                focusdraw(entrs);
                /*if (entrs[fsel].type == DT_REG)*/
                        /*filepreview(&entrs[fsel]);*/
                /*dirpreview(child, entrs[fsel].nchld, fsel, CHILD_COL);*/

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
        }

        return 0;
}
