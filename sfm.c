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

#ifndef ESC
#define ESC 27
#endif /* ESC */
#ifndef DEL
#define DEL 127
#endif /* DEL */

#define DELAY_MS 350000

#define CTRL(x)         ((x) & 0x1f)
#define YMAX            (getmaxy(stdscr))
#define XMAX            (getmaxx(stdscr))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define ARRLEN(x)       (sizeof(x) / sizeof(x[0]))
#define ISDIGIT(x)      ((unsigned int)(x) - '0' <= 9)
#define ENTSORT(e, n)   (qsort((e), (n), sizeof(*(e)), sortfn))

#define SEL_CORRECT                                                     \
        win->sel = ((win->sel < 0) ? 0 : (win->sel > win->nf - 1)       \
                    ? win->nf - 1 : win->sel);

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
        Entry   *ents;
        ulong    nf;
        long     sel;
        long     nsel;
} Win;

typedef union {
        int n;
        const char *s;
        const void *v;
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
        NAV_FPREVIEW,
        NAV_INFO,
        NAV_REDRAW,
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
        [MSG_EXEC] = "execute '%s' (y/n)?",
        [MSG_SORT] = "'n'ame 's'ize 'r'everse",
        [MSG_PROMPT] = ":",
        [MSG_FAIL] = "action failed"
};

/* globals variables */
static Win *win = NULL;         /* main display */
static char *curdir = NULL;     /* current directory */
static uchar f_showall = 0;     /* show hidden files */
static uchar f_redraw = 0;      /* redraw screen */
static uchar f_fpreview = 0;    /* preview files */
static uchar f_info = 0;        /* show info about entries */

/* function pointers */
static int (*sortfn)(const void *x, const void *y);

/* function declarations */
static void      cursesinit(void);
static int       entcount(char *);
static Entry    *entget(char *, ulong);
static void      entprint(void);
static void      statusbar(void);
static void      statsget(Entry *);
static void      filepreview(void);
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
static void      selectitem(const Arg *);
static void      quit(const Arg *);
static void      entcleanup(Entry *);
static void      escape(char *, const char *);
static void      xdelay(useconds_t);
static void      echdir(const char *);
static void     *emalloc(size_t);
static void      die(const char *, ...);

#include "config.h"

/* function implementations */
static inline int
sortname(const void *x, const void *y)
{
        return (strcmp(((Entry *)x)->name, ((Entry *)y)->name));
}

static inline int
sortsize(const void *x, const void *y)
{
        return -(((Entry *)x)->stat.st_size - ((Entry *)y)->stat.st_size);
}

/* TODO fix */
static inline int
sortrev(const void *x, const void *y)
{
        return -sortfn(x, y);
}

void
cursesinit(void)
{
        if (!initscr())
                die("sfm: initscr failed");
        noecho();
        cbreak();
        curs_set(0);
        keypad(stdscr, 1);

        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);

        win = emalloc(sizeof(Win));
        win->ents = NULL;
        win->sel = win->nsel = win->nf = 0;
}

int
entcount(char *path)
{
        DIR *dir;
        struct dirent *dent;
        int n = 0;

        /*TODO: handle error properly */
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

Entry *
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

                /* TODO: keep inode */
                ents[i].nlen = strlen(dent->d_name);
                ents[i].name = emalloc(ents[i].nlen + 1);
                strcpy(ents[i].name, dent->d_name);

                stat(ents[i].name, &ents[i].stat);
                ents[i].tm = localtime(&ents[i].stat.st_ctime);
                ents[i].atime = asctime(ents[i].tm);
                statsget(&ents[i]);

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
                }

                i++;

        }
        (void)closedir(dir);
        ENTSORT(ents, n);
        return ents;
}

/*TODO: handle f_info*/
void
entprint(void)
{
        int i = 0;

        for (; i < win->nf && i < YMAX; i++) {
                if (i == win->sel)
                        attron(A_REVERSE);

                move(i + 2, 0);
                addch(win->ents[i].selected ? '+' : ' ');

                attron(win->ents[i].attrs);
                printw(" %s", win->ents[i].name);
                attroff(A_REVERSE | win->ents[i].attrs);
        }

        mvprintw(YMAX - 1, 0, "%ld/%ld %s", win->sel + 1, win->nf,
                 win->ents[win->sel].statstr);
}

void
statusbar(void)
{
        char host[HOST_NAME_MAX];
        char user[LOGIN_NAME_MAX];

        gethostname(host, HOST_NAME_MAX);
        getlogin_r(user, LOGIN_NAME_MAX);
        attron(A_BOLD);
        mvprintw(0, 0, "%s@%s:%s", user, host, curdir);
        attroff(A_BOLD);
        mvhline(1, 0, ACS_HLINE, XMAX);
}

void
statsget(Entry *ent)
{
        sprintf(ent->statstr, "%c%c%c%c%c%c%c%c%c%c %ldB %s",
                (S_ISDIR(ent->stat.st_mode)) ? 'd' : '-',
                ent->stat.st_mode & S_IRUSR ? 'r' : '-',
                ent->stat.st_mode & S_IWUSR ? 'w' : '-',
                ent->stat.st_mode & S_IXUSR ? 'x' : '-',
                ent->stat.st_mode & S_IRGRP ? 'r' : '-',
                ent->stat.st_mode & S_IWGRP ? 'w' : '-',
                ent->stat.st_mode & S_IXGRP ? 'x' : '-',
                ent->stat.st_mode & S_IROTH ? 'r' : '-',
                ent->stat.st_mode & S_IWOTH ? 'w' : '-',
                ent->stat.st_mode & S_IXOTH ? 'x' : '-',
                ent->stat.st_size, ent->atime);
}

void
filepreview(void)
{
        WINDOW *fw;
        FILE *fp;
        Entry *ent = &win->ents[win->sel];
        char buf[BUFSIZ];
        size_t maxlen = XMAX >> 1;
        int ln = 0;

        if (ent->flags & DT_REG) {
                if ((fp = fopen(ent->name, "r")) == NULL)
                        return;
                fw = newwin(YMAX - 2, maxlen, 2, maxlen);
                while (fgets(buf, BUFSIZ, fp) && ln < YMAX) {
                        if (strlen(buf) > maxlen)
                                buf[maxlen] = '\0';
                        mvwaddstr(fw, ln++, 0, buf);
                }
                fclose(fp);
                wrefresh(fw);
                f_redraw = 1;
        }
}

void
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
char *
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

int
confirmact(const char *str)
{
        notify(MSG_EXEC, str);
        return (getch() == 'y');
}

int
spawn(char *cmd)
{
        char *args[] = {getenv(envs[ENV_SHELL]), "-c", cmd, NULL};
        struct sigaction oldsighup;
        struct sigaction oldsigtstp;
        pid_t pid;
        int status;

        endwin();

        switch (pid = fork()) {
        case -1:
                return 1;
        case 0:
                execvp(*args, args);
                _exit(EXIT_SUCCESS);
                break;
        default:
                while (wait(&status) != pid)
                        ;
                sigaction(SIGHUP, &oldsighup, NULL);
                sigaction(SIGTSTP, &oldsigtstp, NULL);
                break;
        }
        return 0;
}

void
nav(const Arg *arg)
{
        /*TODO: try make to fallthrough */
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

                if (ent->flags & DT_REG) {
                        sprintf(buf, "%s %s", cmds[CMD_OPEN],
                                win->ents[win->sel].name);
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
                win->sel = win->nf - 1;
                break;
        case NAV_SHOWALL:
                f_showall ^= 1;
                f_redraw = 1;
                break;
        case NAV_FPREVIEW:
                f_fpreview ^= 1; 
                f_redraw = 1;
                break;
        case NAV_INFO:
                f_info ^= 1;
                f_redraw = 1;
                break;
        case NAV_REDRAW:
                f_redraw = 1;
                break;
        }
}

void
cd(const Arg *arg)
{
        echdir(arg->s);
        f_redraw = 1;
}

void
selectitem(const Arg *arg)
{
        win->ents[win->sel].selected ^= 1;
        if (win->ents[win->sel].selected)
                win->nsel++;
        else
                win->nsel--;
        win->sel++;
}

void
run(const Arg *arg)
{
        char buf[BUFSIZ];
        char tmp[BUFSIZ];
        int i = 0;

        sprintf(buf, "%s", (const char *)arg->v);

        /*TODO: make prettier */
        if (win->nsel > 0) {
                for (; i < win->nf; i++) {
                        if (win->ents[i].selected) {
                                escape(tmp, win->ents[i].name);
                                sprintf(buf + strlen(buf), " %s ", tmp);
                        }
                }
        } else {
                escape(tmp, win->ents[win->sel].name);
                sprintf(buf + strlen(buf), " %s ", tmp);
        }

        if (confirmact(buf)) {
                spawn(buf);
                f_redraw = 1;
        }
}

void
builtinrun(const Arg *arg)
{
        char buf[BUFSIZ];
        char tmp[512];
        char *prog = NULL;

        /*TODO: better implementation */
        escape(tmp, win->ents[win->sel].name);

        switch (arg->n) {
        case RUN_EDITOR:
                prog = getenv(envs[ENV_EDITOR]);
                break;
        case RUN_PAGER:
                prog = getenv(envs[ENV_PAGER]);
                break;
        case RUN_OPENWITH:
                if ((prog = promptstr(msgs[MSG_OPENWITH])) == NULL)
                        return;
                break;
        case RUN_RENAME:
                /* TODO: check for NULL here too */
                sprintf(buf, "%s %s %s", cmds[CMD_MV], tmp,
                        promptstr(msgs[MSG_RENAME]));
                goto end; /* A GOTO! */
        default:
                return;
        }

        sprintf(buf, "%s %s", prog, tmp);
end:
        spawn(buf);
        /*free(prog);*/
        f_redraw = 1;
}

void
sort(const Arg *arg)
{
        notify(MSG_SORT, NULL);

        switch (getch()) {
        case 'n':
                sortfn = sortname;
                break;
        case 's':
                sortfn = sortsize;
                break;
        case 'r':
                /*sortfn = sortrev;*/
                break;
        }
        ENTSORT(win->ents, win->nf);
}

void
prompt(const Arg *arg)
{
        char buf[BUFSIZ];

        sprintf(buf, "%s", promptstr(msgs[MSG_PROMPT]));
        if (confirmact(buf)) {
                spawn(buf);
                f_redraw = 1;
        }
}

void
quit(const Arg *arg)
{
        entcleanup(win->ents);
        free(win);
        endwin();
        exit(EXIT_SUCCESS);
}

void
entcleanup(Entry *ent)
{
        int i = 0;

        if (win->ents != NULL) {
                for (; i < win->nf; i++)
                        if (win->ents[i].name != NULL)
                                free(win->ents[i].name);
                free(win->ents);
        }
}

void
escape(char *buf, const char *str)
{
        int i = 0, pos = 0;

        for (; i < strlen(str); i++) {
                switch (str[i]) {
                case ' ': /* FALLTHROUGH */
                case '(': /* FALLTHROUGH */
                case ')': /* FALLTHROUGH */
                        buf[pos++] = '\\';
                }
                buf[pos++] = str[i];
        }
        buf[pos] = '\0';
}

void
xdelay(useconds_t delay)
{
        refresh();
        usleep(delay);
}

void
echdir(const char *path)
{
        if (chdir(path) != 0) {
                notify(MSG_FAIL, NULL);
                xdelay(DELAY_MS << 2);
        }
}

void *
emalloc(size_t nb)
{
        void *p;

        if ((p = malloc(nb)) == NULL)
                die("sfm: emalloc:");
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
        char cwd[PATH_MAX] = {0};
        int c, i;

        cursesinit();
        f_redraw = 1;
        sortfn = sortname;

        for (;;) {
                erase();

                if (f_redraw) {
                        if ((curdir = getcwd(cwd, sizeof(cwd))) == NULL)
                                die("sfm: can't get cwd");
                        entcleanup(win->ents);
                        win->nf = entcount(curdir);
                        if ((win->ents = entget(curdir, win->nf)) == NULL)
                                die("sfm: can't get entries");
                        f_redraw = 0;
                        refresh();
                }

                SEL_CORRECT;
                statusbar();
                entprint();
                if (f_fpreview)
                        filepreview();

                /*TODO: signal/timeout */
                c = getch();
                for (i = 0; i < ARRLEN(keys); i++)
                        if (c == keys[i].key)
                                keys[i].func(&(keys[i].arg));
        }

        return 0;
}
