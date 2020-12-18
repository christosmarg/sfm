/* See LICENSE file for copyright and license details. */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ncurses.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif /* PATH_MAX */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX  _POSIX_HOST_NAME_MAX
#endif /* HOST_NAME_MAX */
#ifndef LOGIN_NAME_MAX
#define LOGIN_NAME_MAX _POSIX_LOGIN_NAME_MAX
#endif /* LOGIN_NAME_MAX */

#ifndef DT_DIR
#define DT_DIR 4
#endif /* DT_DIR */
#ifndef DT_REG
#define DT_REG 8
#endif /* DT_REG */
#ifndef DT_LNK
#define DT_LNK 10
#endif /* DT_LNK */

#ifndef ESC
#define ESC 27
#endif /* ESC */
#ifndef DEL
#define DEL 127
#endif /* DEL */

#define DELAY_MS 350000
#define SCROLLOFF 4

#define CTRL(x)         ((x) & 0x1f)
#define YMAX            (getmaxy(stdscr))
#define XMAX            (getmaxx(stdscr))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define ARRLEN(x)       (sizeof(x) / sizeof(x[0]))
#define ISDIGIT(x)      ((unsigned int)(x) - '0' <= 9)
#define ENTSORT(e, n)   (qsort((e), (n), sizeof(*(e)), sortfn))

/* type definitions */
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef long long ll;
typedef unsigned long long ull;

/* structs, unions and enums */
typedef struct {
        struct stat      stat;
        struct tm       *tm;
        char             statstr[BUFSIZ];
        char            *atime;
        char            *name;
        uint             attrs;
        ushort           nlen;
        uchar            flags;
        uchar            selected;
} Entry;

typedef struct {
        Entry           *ents;
        ulong            nents;
        long             sel;
        long             nsel;
} Win;

typedef union {
        int n;
        const char *s;
        const void *v;
} Arg;

typedef struct {
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
        NAV_SELECT,
        NAV_SHOWALL,
        NAV_INFO,
        NAV_REDRAW,
        NAV_EXIT,
};

enum {
        RUN_EDITOR,
        RUN_PAGER,
        RUN_OPENWITH,
        RUN_RENAME,
};

enum {
        CMD_OPEN,
        CMD_MV,
};

enum {
        ENV_SHELL,
        ENV_EDITOR,
        ENV_PAGER,
};

enum {
        MSG_OPENWITH,
        MSG_RENAME,
        MSG_EXEC,
        MSG_SORT,
        MSG_PROMPT,
        MSG_FAIL,
};

/* function declarations */
static void      cursesinit(void);
static ulong     entcount(char *);
static Entry    *entget(char *, ulong);
static void      entprint(int);
static void      notify(int, const char *);
static char     *promptstr(const char *);
static int       confirmact(const char *);
static int       spawn(char *);
static void      nav(const Arg *);
static void      cd(const Arg *);
static void      run(const Arg *);
static void      builtinrun(const Arg *);
static void      sort(const Arg *);
static void      prompt(const Arg *);
static void      selcorrect(void);
static void      entcleanup(Entry *);
static void      escape(char *, const char *);
static void      xdelay(useconds_t);
static void      echdir(const char *);
static void     *emalloc(size_t);
static void      die(const char *, ...);
static void      sfmrun(void);
static void      cleanup(void);

/* useful strings */
static const char *cmds[] = {
        [CMD_OPEN] = "xdg-open",
        [CMD_MV] = "mv",
};

static const char *envs[] = {
        [ENV_SHELL] = "SHELL",
        [ENV_EDITOR] = "EDITOR",
        [ENV_PAGER] = "PAGER",
};

static const char *msgs[] = {
        [MSG_OPENWITH] = "open with: ",
        [MSG_RENAME] = "rename: ",
        [MSG_EXEC] = "execute '%s' (y/N)?",
        [MSG_SORT] = "'n'ame 's'ize 'r'everse",
        [MSG_PROMPT] = ":",
        [MSG_FAIL] = "action failed"
};

/* globals variables */
static Win *win = NULL;         /* main display */
static char *curdir = NULL;     /* current directory */
static int curscroll = 0;       /* cursor scroll */

/* flags */
static uchar f_showall = 0;     /* show hidden files */
static uchar f_redraw = 0;      /* redraw screen */
static uchar f_info = 0;        /* show info about entries */
static uchar f_noconfirm = 0;   /* exec without confirmation */
static uchar f_namesort = 0;    /* sort entries by name */
static uchar f_sizesort = 0;    /* sort entries by size */
static uchar f_revsort = 0;     /* reverse current sort function */
static uchar f_running = 1;     /* 0 when sfm should exit */

static int (*sortfn)(const void *x, const void *y);

#include "config.h"

/* function implementations */
static inline int
namecmp(const void *x, const void *y)
{
        return (strcmp(((Entry *)x)->name, ((Entry *)y)->name));
}

static inline int
revnamecmp(const void *x, const void *y)
{
        return -namecmp(x, y);
}

static inline int
sizecmp(const void *x, const void *y)
{
        return -(((Entry *)x)->stat.st_size - ((Entry *)y)->stat.st_size);
}

static inline int
revsizecmp(const void *x, const void *y)
{
        return -sizecmp(x, y);
}

static void
cursesinit(void)
{
        int i = 0;

        if (!initscr())
                die("sfm: initscr failed");

        noecho();
        cbreak();
        curs_set(0);
        keypad(stdscr, 1);
        /*timeout(1000);*/
        /*set_escdelay(25);*/

        start_color();

        for (; i < ARRLEN(colors); i++)
                init_pair(i + 1, colors[i], COLOR_BLACK);
}

static ulong
entcount(char *path)
{
        DIR *dir;
        struct dirent *dent;
        int n = 0;

        /* FIXME: repating code, do something! */
        if ((dir = opendir(path)) == NULL)
                die("sfm: opendir failed");

        while ((dent = readdir(dir)) != NULL) {
                if (!strcmp(dent->d_name, "..") || !strcmp(dent->d_name, "."))
                        continue;
                if (!f_showall && dent->d_name[0] == '.')
                        continue;
                n++;

        }
        (void)closedir(dir);

        return n;
}

static Entry *
entget(char *path, ulong n)
{
        DIR *dir;
        struct dirent *dent;
        Entry *ents;
        int i = 0;
        
        ents = emalloc(n * sizeof(Entry));
        if ((dir = opendir(path)) == NULL)
                die("sfm: opendir failed");

        while ((dent = readdir(dir)) != NULL) {
                if (!strcmp(dent->d_name, "..") || !strcmp(dent->d_name, "."))
                        continue;
                if (!f_showall && dent->d_name[0] == '.')
                        continue;

                ents[i].nlen = strlen(dent->d_name);
                ents[i].name = emalloc(ents[i].nlen + 1);
                strcpy(ents[i].name, dent->d_name);

                stat(ents[i].name, &ents[i].stat);
                ents[i].tm = localtime(&ents[i].stat.st_ctime);
                ents[i].atime = asctime(ents[i].tm);

                ents[i].attrs = 0;
                ents[i].flags = 0;
                ents[i].selected = 0;
                ents[i].flags |= dent->d_type;

                switch (ents[i].flags) {
                case DT_DIR:
                        ents[i].attrs |= COLOR_PAIR(1) | A_BOLD;
                        break;
                case DT_REG:
                        ents[i].attrs |= COLOR_PAIR(2);
                        break;
                case DT_LNK:
                        ents[i].attrs |= COLOR_PAIR(3);
                        if (S_ISDIR(ents[i].stat.st_mode)) {
                                ents[i].flags |= DT_DIR;
                                ents[i].attrs |= A_BOLD;
                        }
                        break;
                /* TODO: handle more modes from stat(3) */
                }

                sprintf(ents[i].statstr, "%c%c%c%c%c%c%c%c%c%c %ldB %s",
                        (S_ISDIR(ents[i].stat.st_mode)) ? 'd' : '-',
                        ents[i].stat.st_mode & S_IRUSR ? 'r' : '-',
                        ents[i].stat.st_mode & S_IWUSR ? 'w' : '-',
                        ents[i].stat.st_mode & S_IXUSR ? 'x' : '-',
                        ents[i].stat.st_mode & S_IRGRP ? 'r' : '-',
                        ents[i].stat.st_mode & S_IWGRP ? 'w' : '-',
                        ents[i].stat.st_mode & S_IXGRP ? 'x' : '-',
                        ents[i].stat.st_mode & S_IROTH ? 'r' : '-',
                        ents[i].stat.st_mode & S_IWOTH ? 'w' : '-',
                        ents[i].stat.st_mode & S_IXOTH ? 'x' : '-',
                        ents[i].stat.st_size, ents[i].atime);

                /* remove newlines */
                ents[i].statstr[strlen(ents[i].statstr) - 1] = '\0';
                ents[i].atime[strlen(ents[i].atime) - 1] = '\0';

                i++;

        }
        (void)closedir(dir);
        ENTSORT(ents, n);

        return ents;
}

/*TODO: add offset for scrolling */
static void
entprint(int off)
{
        Entry *ent;
        int i = 0;
        char ind;

        attron(A_BOLD);
        addstr(curdir);
        attroff(A_BOLD);
        mvhline(1, 0, ACS_HLINE, XMAX);

        for (; i < win->nents && i < YMAX; i++) {
                ent = &win->ents[i + off];

                move(i + 2, 0);
                addch(ent->selected ? '+' : ' ');

                if (i == win->sel)
                        attron(A_REVERSE);

                if (f_info)
                        printw("%s  %c%c%c %10ldB  ",
                                ent->atime,
                                '0' + ((ent->stat.st_mode >> 6) & 7),
                                '0' + ((ent->stat.st_mode >> 3) & 7),
                                '0' + (ent->stat.st_mode & 7),
                                ent->stat.st_size);

                attron(ent->attrs);
                addstr(ent->name);
                attroff(A_REVERSE | ent->attrs);

                switch (ent->stat.st_mode & S_IFMT) {
                /* FIXME: for some reason regular files fall here too */
                case S_IFDIR:
                        ind = '/';
                        break;
                case S_IFREG:
                        if (ent->stat.st_mode & 0100)
                                ind = '*';
                        break;
                case S_IFLNK:
                        ind = (ent->flags & DT_DIR) ? '/' : '@';
                        break;
                case S_IFSOCK:
                        ind = '=';
                        break;
                case S_IFIFO:
                        ind = '|';
                        break;
                case S_IFBLK:
                        ind = '%';
                        break;
                case S_IFCHR:
                        ind = '#';
                        break;
                default:
                        ind = '?';
                }

                addch(ind);
        }

        mvprintw(YMAX - 1, 0, "%ld/%ld %s", win->sel + 1, win->nents,
                 win->ents[win->sel].statstr);
}

/* TODO: get rid of the `switch`, use vfprintf */
static void
notify(int flag, const char *str)
{
        move(YMAX - 1, 0);
        clrtoeol();

        switch (flag) {
        case MSG_EXEC:
                printw(msgs[MSG_EXEC], str);
                break;
        case MSG_FAIL: /* FALLTHROUGH */
        case MSG_SORT: /* FALLTHROUGH */
                addstr(msgs[flag]);
                break;
        default:
                addstr(str);
        }
}

/* TODO: fix backspace and change name */
static char *
promptstr(const char *msg)
{
        char buf[BUFSIZ], *str;
        int len = 0, c;

        notify(-1, msg);
        echo();
        curs_set(1);

        while ((c = getch()) != '\n') {
                switch (c) {
                case KEY_BACKSPACE:     /* FALLTHROUGH */
                case KEY_DC:            /* FALLTHROUGH */
                case '\b':              /* FALLTHROUGH */
                        if (len > 0)
                                len--;
                        break;
                /* FIXME: why is this slow? */
                case ESC:
                        return NULL;
                default:
                        buf[len++] = c;
                }
        }

        buf[len] = '\0';
        str = emalloc(len + 1);
        strcpy(str, buf);

        curs_set(0);
        noecho();

        return str;
}

static int
confirmact(const char *str)
{
        if (f_noconfirm)
                return 1;
        notify(MSG_EXEC, str);
        return (getch() == 'y');
}

static int
spawn(char *cmd)
{
        char *args[] = {getenv(envs[ENV_SHELL]), "-c", cmd, NULL};
        struct sigaction oldsighup;
        struct sigaction oldsigtstp;
        pid_t pid;
        int status;

        switch (pid = fork()) {
        case -1:
                return 1;
        case 0:
                execvp(*args, args);
                _exit(EXIT_SUCCESS);
                break;
        default:
                endwin();
                while (wait(&status) != pid)
                        ;
                sigaction(SIGHUP, &oldsighup, NULL);
                sigaction(SIGTSTP, &oldsigtstp, NULL);
                break;
        }
        return 0;
}

static void
nav(const Arg *arg)
{
        Entry *ent = &win->ents[win->sel];
        char buf[BUFSIZ];

        switch (arg->n) {
        case NAV_LEFT:
                echdir("..");
                f_redraw = 1;
                break;
        case NAV_RIGHT:
                if (ent->flags & DT_DIR)
                        echdir(ent->name);
                /* TODO: handle links to dirs */

                if (ent->flags & DT_REG) {
                        sprintf(buf, "%s %s", cmds[CMD_OPEN],
                                win->ents[win->sel].name);
                        /* TODO: escape this buf! */
                        if (!spawn(buf))
                                notify(MSG_FAIL, NULL);
                }
                f_redraw = 1;
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
                win->sel = win->nents - 1;
                break;
        case NAV_SELECT:
                win->ents[win->sel].selected ^= 1;
                if (win->ents[win->sel++].selected)
                        win->nsel++;
                else
                        win->nsel--;
                break;
        case NAV_SHOWALL:
                f_showall ^= 1;
                f_redraw = 1;
                break;
        case NAV_INFO:
                f_info ^= 1;
                f_redraw = 1;
                break;
        case NAV_REDRAW:
                f_redraw = 1;
                break;
        case NAV_EXIT:
                f_running = 0;
                break;
        }
}

static void
cd(const Arg *arg)
{
        echdir(arg->s);
        f_redraw = 1;
}

static void
run(const Arg *arg)
{
        char buf[BUFSIZ];
        char tmp[BUFSIZ];
        int i = 0;

        sprintf(buf, "%s", (const char *)arg->v);

        /*TODO: make prettier */
        if (win->nsel > 0) {
                for (; i < win->nents; i++) {
                        if (win->ents[i].selected) {
                                escape(tmp, win->ents[i].name);
                                sprintf(buf + strlen(buf), " %s ", tmp);
                        }
                }
        } else {
                escape(tmp, win->ents[win->sel].name);
                sprintf(buf + strlen(buf), " %s ", tmp);
        }

        f_noconfirm = 0;
        if (confirmact(buf)) {
                spawn(buf);
                f_redraw = 1;
        }
}

static void
builtinrun(const Arg *arg)
{
        Arg prog;

        switch (arg->n) {
        case RUN_EDITOR:
                prog.s = getenv(envs[ENV_EDITOR]);
                break;
        case RUN_PAGER:
                prog.s = getenv(envs[ENV_PAGER]);
                break;
        case RUN_OPENWITH:
                if ((prog.s = promptstr(msgs[MSG_OPENWITH])) == NULL)
                        return;
                break;
        case RUN_RENAME:
                /* FIXME */
                /*sprintf(prog.s, "%s %s %s", cmds[CMD_MV], tmp,*/
                        /*promptstr(msgs[MSG_RENAME]));*/
                return;
                break;
        default:
                return;
        }

        f_noconfirm = 0;
        run(&prog);
        f_redraw = 1;
}

static void
sort(const Arg *arg)
{
        notify(MSG_SORT, NULL);
        switch (getch()) {
        case 'n':
                f_namesort = 1;
                f_sizesort = 0;
                sortfn = f_revsort ? namecmp : revnamecmp;
                break;
        case 's':
                f_sizesort = 1;
                f_namesort = 0;
                sortfn = f_revsort ? sizecmp : revsizecmp;
                break;
        case 'r':
                /* FIXME: what the fuck? make it choose a function now */
                f_revsort ^= 1;
                break;
        }
        ENTSORT(win->ents, win->nents);
}

static void
prompt(const Arg *arg)
{
        char buf[BUFSIZ];

        sprintf(buf, "%s", promptstr(msgs[MSG_PROMPT]));
        if (confirmact(buf)) {
                spawn(buf);
                f_redraw = 1;
        }
}
static void
selcorrect(void)
{
        if (win->sel < 0)
                win->sel = 0;
        else if (win->sel > win->nents - 1)
                win->sel = win->nents - 1;

}

static void
entcleanup(Entry *ent)
{
        int i = 0;

        if (win->ents != NULL) {
                for (; i < win->nents; i++)
                        if (win->ents[i].name != NULL)
                                free(win->ents[i].name);
                free(win->ents);
        }
}

static void
escape(char *buf, const char *str)
{
        for (; *str; str++) {
                switch (*str) {
                case ' ':  /* FALLTHROUGH */
                case '\'': /* FALLTHROUGH */
                case '(':  /* FALLTHROUGH */
                case ')':  /* FALLTHROUGH */
                        *buf++ = '\\';
                }
                *buf++ = *str;
        }
        *buf = '\0';
}

static void
xdelay(useconds_t delay)
{
        refresh();
        usleep(delay);
}

static void
echdir(const char *path)
{
        if (chdir(path) != 0) {
                notify(MSG_FAIL, NULL);
                xdelay(DELAY_MS << 2);
        }
}

static void *
emalloc(size_t nb)
{
        void *p;

        if ((p = malloc(nb)) == NULL)
                die("sfm: emalloc:");
        return p;
}

static void
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

static void
sfmrun(void)
{
        char cwd[PATH_MAX] = {0};
        int c, i;

        while (f_running) {
                erase();

                if (f_redraw) {
                        if ((curdir = getcwd(cwd, sizeof(cwd))) == NULL)
                                die("sfm: can't get cwd");

                        entcleanup(win->ents);
                        win->nents = entcount(curdir);
                        if ((win->ents = entget(curdir, win->nents)) == NULL)
                                die("sfm: can't get entries");

                        f_redraw = 0;
                        refresh();
                }

                selcorrect();
                curscroll = win->sel > YMAX - 4 ? SCROLLOFF : 0;
                entprint(curscroll);

                /*TODO: signal/timeout */
                if ((c = getch()) != ERR)
                        for (i = 0; i < ARRLEN(keys); i++)
                                if (c == keys[i].key)
                                        keys[i].func(&(keys[i].arg));
        }
}

static void
cleanup(void)
{
        entcleanup(win->ents);
        free(win);
        endwin();
}

int
main(int argc, char *argv[])
{
        f_redraw = 1;
        f_namesort = 1;
        sortfn = namecmp;

        cursesinit();

        win = emalloc(sizeof(Win));
        win->ents = NULL;
        win->sel = win->nsel = win->nents = 0;

        sfmrun();
        cleanup();

        return 0;
}
