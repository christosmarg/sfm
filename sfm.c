/* See LICENSE file for copyright and license details. */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <limits.h>
#include <locale.h>
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
        char             statstr[36]; /* XXX no? */
        char             date[12];
        char             sizestr[12];
        char            *name;
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
        DIR_OR_DIRLNK   = 1 << 0,
        HARD_LNK        = 1 << 1,
};

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

/* Colors */
enum {
        C_BLK = 1, /* Block device */
        C_CHR, /* Character device */
        C_DIR, /* Directory */
        C_EXE, /* Executable file */
        C_FIL, /* Regular file */
        C_HRD, /* Hard link */
        C_LNK, /* Symbolic link */
        C_MIS, /* Missing file OR file details */
        C_ORP, /* Orphaned symlink */
        C_PIP, /* Named pipe (FIFO) */
        C_SOC, /* Socket */
        C_UND, /* Unknown OR 0B regular/exe file */
        C_INF, /* Information */
};

/* function declarations */
static void      cursesinit(void);
static ulong     entcount(char *);
static Entry    *entget(char *, ulong);
static void      entprint(void);
static char     *fmtsize(size_t);
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
static void      entcleanup(void);
static void      escape(char *, const char *);
static void      xdelay(useconds_t);
static void      echdir(const char *);
static void     *emalloc(size_t);
static void      die(const char *, ...);
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
        [MSG_SORT] = "'n'ame 's'ize 'd'ate 'r'everse",
        [MSG_PROMPT] = ":",
        [MSG_FAIL] = "action failed"
};

/* globals variables */
static Win *win = NULL;         /* main display */
static char *curdir = NULL;     /* current directory */
static int cur = 0;             /* cursor position */
static int curscroll = 0;       /* cursor scroll */
static int scrolldir;           /* scroll direction */

/* flags */
static uchar f_showall = 0;     /* show hidden files */
static uchar f_redraw = 0;      /* redraw screen */
static uchar f_info = 0;        /* show info about entries */
static uchar f_noconfirm = 0;   /* exec without confirmation */
static uchar f_namesort = 0;    /* sort entries by name */
static uchar f_sizesort = 0;    /* sort entries by size */
static uchar f_datesort = 0;    /* sort entries by date */
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
datecmp(const void *x, const void *y)
{
        return (strcmp(((Entry *)x)->date, ((Entry *)y)->date));
}

static inline int
revdatecmp(const void *x, const void *y)
{
        return -datecmp(x, y);
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
        int i = 1;

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
                init_pair(i, colors[i], COLOR_BLACK);
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
        struct tm *tm;
        Entry *ents;
        int i = 0;
        char type;
        
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
                tm = localtime(&ents[i].stat.st_ctime);
                strftime(ents[i].date, 12, "%F", tm);
                strcpy(ents[i].sizestr, fmtsize(ents[i].stat.st_size));

                ents[i].flags = 0;
                /* FIXME: resets on every redraw, keep track somehow */
                ents[i].selected = 0;
                ents[i].flags |= dent->d_type;

                /* TODO: use fstatat(3) */

                /* FIXME: links don't work */
                switch (ents[i].stat.st_mode & S_IFMT) {
                case S_IFREG:
                        type = '-';
                        break;
                case S_IFDIR:
                        type = 'd';
                        break;
                case S_IFLNK:
                        type = 'l';
                        break;
                case S_IFSOCK:
                        type = 's';
                        break;
                case S_IFIFO:
                        type = 'p';
                        break;
                case S_IFBLK:
                        type = 'b';
                        break;
                case S_IFCHR:
                        type = 'c';
                        break;
                default:
                        type = '?';
                        break;
                }

                /* seperate field for lsperms? */
                sprintf(ents[i].statstr, "%c%c%c%c%c%c%c%c%c%c %s %s",
                        type,
                        ents[i].stat.st_mode & S_IRUSR ? 'r' : '-',
                        ents[i].stat.st_mode & S_IWUSR ? 'w' : '-',
                        ents[i].stat.st_mode & S_IXUSR ? 'x' : '-',
                        ents[i].stat.st_mode & S_IRGRP ? 'r' : '-',
                        ents[i].stat.st_mode & S_IWGRP ? 'w' : '-',
                        ents[i].stat.st_mode & S_IXGRP ? 'x' : '-',
                        ents[i].stat.st_mode & S_IROTH ? 'r' : '-',
                        ents[i].stat.st_mode & S_IWOTH ? 'w' : '-',
                        ents[i].stat.st_mode & S_IXOTH ? 'x' : '-',
                        ents[i].sizestr, ents[i].date);

                i++;

        }
        (void)closedir(dir);
        ENTSORT(ents, n);

        return ents;
}

static void
entprint(void)
{
        Entry *ent;
        int i = 0;
        uint attrs;
        uchar color;
        char ind;

        attron(A_BOLD | COLOR_PAIR(C_DIR));
        addstr(curdir);
        attroff(A_BOLD | COLOR_PAIR(C_DIR));

        /* TODO: change 4 to line ignore constant */
        for (; i < win->nents && i <= YMAX - 4; i++) {
                ent = &win->ents[i + curscroll];
                ind = ' ';
                attrs = 0;
                color = 0;

                move(i + 2, 0);

                if (i == cur)
                        attrs |= A_REVERSE;

                if (f_info) {
                        attron(COLOR_PAIR(C_INF));
                        printw("%s  %c%c%c  %7s  ",
                                ent->date,
                                '0' + ((ent->stat.st_mode >> 6) & 7),
                                '0' + ((ent->stat.st_mode >> 3) & 7),
                                '0' + (ent->stat.st_mode & 7),
                                ent->sizestr);
                        attroff(COLOR_PAIR(C_INF));
                }

                addch(ent->selected ? '+' : ' ');

                switch (ent->stat.st_mode & S_IFMT) {
                case S_IFDIR:
                        ind = '/';
                        color = C_DIR;
                        attrs |= A_BOLD;
                        break;
                case S_IFREG:
                        color = C_FIL;
                        if (ent->stat.st_mode & 0100) {
                                ind = '*';
                                color = C_EXE;
                        }
                        break;
                case S_IFLNK:
                        ind = (ent->flags & DT_DIR) ? '/' : '@';
                        color = C_LNK;
                        if (S_ISDIR(ent->stat.st_mode))
                                attrs |= A_BOLD;
                        break;
                case S_IFSOCK:
                        ind = '=';
                        color = C_SOC;
                        break;
                case S_IFIFO:
                        ind = '|';
                        color = C_PIP;
                        break;
                case S_IFBLK:
                        color = C_BLK;
                        break;
                case S_IFCHR:
                        color = C_CHR;
                        break;
                default:
                        ind = '?';
                        color = C_UND;
                        break;
                }

                attrs |= COLOR_PAIR(color);
                attron(attrs);
                addstr(ent->name);
                attroff(attrs);

                addch(ind);
        }

        mvprintw(YMAX - 1, 0, "%ld/%ld %s", win->sel + 1, win->nents,
                 win->ents[win->sel].statstr);
}

static char *
fmtsize(size_t sz)
{
        static char buf[12];
        int i = 0;

        for (; sz > 1024; i++)
                sz >>= 10;
        /* TODO: handle floating point parts */
        sprintf(buf, "%ld%c", sz, "BKMGTPEZY"[i]);

        return buf;
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
                if (ent->flags & DT_REG && ent->flags & ~DT_LNK) {
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
                /* 
                 * Sign indicates direction, number indicates how many
                 * new directories we should show on every scroll.
                 */
                scrolldir = -1;
                break;
        case NAV_DOWN:
                win->sel++;
                scrolldir = 1;
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
                f_sizesort = f_datesort = 0;
                break;
        case 's':
                f_sizesort = 1;
                f_namesort = f_datesort = 0;
                break;
        case 'd':
                f_datesort = 1;
                f_namesort = f_sizesort = 0;
        case 'r':
                f_revsort ^= 1;
                break;
        }

        if (!f_revsort) {
                if (f_namesort)
                        sortfn = namecmp;
                else if (f_sizesort)
                        sortfn = sizecmp;
                else if (f_datesort)
                        sortfn = datecmp;
        } else {
                if (f_namesort)
                        sortfn = revnamecmp;
                else if (f_sizesort)
                        sortfn = revsizecmp;
                else if (f_datesort)
                        sortfn = revdatecmp;
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

        /* TODO: count `onscreen` */
        /* FIXME: BUGS! NAV_BOTTOM doesn't work. Resets after using exec  */
        if (win->sel > YMAX - 4 - SCROLLOFF) {
                cur = YMAX - 4 - SCROLLOFF;
                /*if (win->sel == win->nents - 1)*/
                        /*return;*/
                /*else*/
                curscroll += scrolldir;
        } else {
                cur = win->sel;
                curscroll = 0;
        }
}

static void
entcleanup(void)
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
cleanup(void)
{
        entcleanup();
        free(win);
        endwin();
}

int
main(int argc, char *argv[])
{
        char cwd[PATH_MAX] = {0};
        int ch, i;

        win = emalloc(sizeof(Win));
        win->ents = NULL;
        win->sel = win->nsel = win->nents = 0;

        f_redraw = 1;
        f_namesort = 1;
        sortfn = namecmp;

        if (!setlocale(LC_ALL, ""))
                die("sfm: no locale support");

        cursesinit();

        while (f_running) {
                erase();

                if (f_redraw) {
                        if ((curdir = getcwd(cwd, sizeof(cwd))) == NULL)
                                die("sfm: can't get cwd");

                        entcleanup();
                        win->nents = entcount(curdir);
                        if ((win->ents = entget(curdir, win->nents)) == NULL)
                                die("sfm: can't get entries");

                        f_redraw = 0;
                        refresh();
                }

                /* TODO: change name */
                selcorrect();
                entprint();

                /*TODO: signal/timeout */
                if ((ch = getch()) != ERR)
                        for (i = 0; i < ARRLEN(keys); i++)
                                if (ch == keys[i].key)
                                        keys[i].func(&(keys[i].arg));
        }

        cleanup();

        return 0;
}
