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
#define PATH_MAX        1024
#endif /* PATH_MAX */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX   _POSIX_HOST_NAME_MAX
#endif /* HOST_NAME_MAX */
#ifndef LOGIN_NAME_MAX
#define LOGIN_NAME_MAX  _POSIX_LOGIN_NAME_MAX
#endif /* LOGIN_NAME_MAX */
#ifndef DT_DIR
#define DT_DIR          4
#endif /* DT_DIR */
#ifndef DT_REG
#define DT_REG          8
#endif /* DT_REG */

#define YMAX            (getmaxy(stdscr))
#define XMAX            (getmaxx(stdscr))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define ARRLEN(x)       (sizeof(x) / sizeof(x[0]))
#define ISDIGIT(x)      ((unsigned int)(x) - '0' <= 9)
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
        char            *atime;
        char            *name;
        ushort           nlen;
        uchar            flags;
        uchar            selected;
} Entry;

typedef struct {
        WINDOW  *w;
        Entry   *ents;
        ulong    nf;
        long     sel;
        int      len;
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
        NAV_LEFT,
        NAV_RIGHT,
        NAV_UP,
        NAV_DOWN,
        NAV_TOP,
        NAV_BOTTOM,
        NAV_SHOWALL,
        NAV_FPREVIEW,
        NAV_REDRAW,
};

enum {
        ENV_SHELL,
        ENV_EDITOR,
};

/* globals */
static Win *win = NULL;         /* main display */
static uchar f_showall = 0;     /* show hidden files */
static uchar f_redraw = 0;      /* redraw screen */
static uchar f_fpreview = 0;    /* preview files */

static char *envs[] = {
        [ENV_SHELL] = "SHELL",
        [ENV_EDITOR] = "EDITOR",
};

/* function pointers */

/* function declarations */
static void      cursesinit(void);
static int       entcount(char *);
static Entry    *entget(char *, ulong);
static void      entprint(Win *);
static void      pathdraw(const char *);
static void      statsprint(void);
static void      filepreview(void);
static int       confirmact(const char *);
static void      spawn(char *);
static void      nav(const Arg *);
static void      cd(const Arg *);
static void      run(const Arg *);
static void      prompt(const Arg *);
static void      select(const Arg *);
static void      quit(const Arg *);
static void      entcleanup(Entry *);
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

        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);

        win = emalloc(sizeof(Win));
        win->ents = NULL;
        win->sel = 0;
        win->len = XMAX >> 1;
        win->w = newwin(YMAX - 2, win->len, 2, 0);
        box(win->w, winborder, winborder);
        /*scrollok(win->w, 1);*/
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

                ents[i].nlen = dent->d_namlen;
                ents[i].name = emalloc(ents[i].nlen + 1);
                strcpy(ents[i].name, dent->d_name);
                stat(ents[i].name, &ents[i].stat);
                ents[i].tm = localtime(&ents[i].stat.st_ctime);
                ents[i].atime = asctime(ents[i].tm);
                ents[i].flags |= dent->d_type;
                ents[i].selected = 0;

                i++;

        }
        (void)closedir(dir);
        return ents;
}

void
entprint(Win *win)
{
        char buf[FILENAME_MAX];
        int i = 0;

        for (; i < win->nf && i < YMAX; i++) {
                if (i == win->sel)
                        wattron(win->w, A_REVERSE);
                if (win->ents[i].flags & DT_DIR)
                        wattron(win->w, A_BOLD | COLOR_PAIR(1));
                sprintf(buf, "%-*s", win->len, win->ents[i].name);

                wmove(win->w, i, 0);
                if (win->ents[i].selected)
                        waddch(win->w, '+');
                wprintw(win->w, " %s ", buf);

                switch (win->ents[i].flags) {
                /*TODO: no hardcoding please.*/
                case DT_DIR:
                        mvwprintw(win->w, i, win->len - 13, "%12d ",
                                  entcount(win->ents[i].name));
                        break;
                case DT_REG:
                        mvwprintw(win->w, i, win->len - 15, "%12d B ",
                                  win->ents[i].stat.st_size);
                        break;
                }
                wattroff(win->w, A_BOLD | A_REVERSE | COLOR_PAIR(1));
                refresh();
                wrefresh(win->w);
        }
}

/*TODO: change name?*/
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
statsprint(void)
{
        Entry *ent = &win->ents[win->sel];

        mvprintw(YMAX - 1, 0, "%ld/%ld %c%c%c%c%c%c%c%c%c%c %dB %s",
                 win->sel + 1, win->nf,
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
        size_t maxlen = win->len;
        int ln = 0;

        if (ent->flags & DT_REG) {
                if ((fp = fopen(ent->name, "r")) == NULL)
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

/* TODO: replace it, i don't like it. */
int
confirmact(const char *str)
{
        char c;

        move(YMAX - 1, 0);
        clrtoeol();
        mvprintw(YMAX - 1, 0, "execute '%s' (y/N)?", str);
        refresh();
        return (c = getch()) == 'y' ? 1 : 0;
}

void
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
                die("sfm: fork failed");
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
}

/*TODO: handle user given shell cmds*/
void
nav(const Arg *arg)
{
        /*TODO: try make to fallthrough */
        Entry *ent = &win->ents[win->sel];
        char buf[BUFSIZ];

        switch (arg->n) {
        case NAV_LEFT:
                /*TODO: andle error -1*/
                chdir("..");
                f_redraw = 1;
                break;
        case NAV_RIGHT:
                if (ent->flags & DT_DIR)
                        chdir(ent->name);
                if (ent->flags & DT_REG) {
                        sprintf(buf, "%s %s", "xdg-open", win->ents[win->sel].name);
                        spawn(buf);
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
        case NAV_REDRAW:
                f_redraw = 1;
                break;
        }
}

void
cd(const Arg *arg)
{
        chdir(arg->d);
        f_redraw = 1;
}

void
select(const Arg *arg)
{
        /* TODO: select all based on .f */
        win->ents[win->sel].selected ^= 1;
        win->sel++;
}

void
run(const Arg *arg)
{
        /*TODO: convert spaces to \_*/
        char buf[BUFSIZ];
        int i = 0;

        sprintf(buf, "%s", (const char *)arg->f);
        for (; i < win->nf; i++)
                if (win->ents[i].selected)
                        sprintf(buf + strlen(buf), " %s ", win->ents[i].name);

        if (confirmact(buf)) {
                spawn(buf);
                f_redraw = 1;
        }
}

/* TODO: fix backspace */
void
prompt(const Arg *arg)
{
        char buf[BUFSIZ];
        char in[BUFSIZ];
        int i = 0;
        char c;

        move(YMAX - 1, 0);
        clrtoeol();
        addch(':');
        refresh();
        echo();
        curs_set(1);
        for (; (c = getch()) != '\n'; i++)
                in[i] = c;
        in[strlen(in)] = '\0';
        curs_set(0);
        noecho();
        sprintf(buf, "%s", in);
        if (confirmact(buf)) {
                spawn(buf);
                f_redraw = 1;
        }
}

void
quit(const Arg *arg)
{
        if (win->w != NULL)
                delwin(win->w);
        entcleanup(win->ents);
        free(win);
        endwin();
        exit(0);
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
        char cwd[PATH_MAX] = {0}, *curdir = NULL;
        int c, i;

        cursesinit();
        f_redraw = f_fpreview = 1;

        for (;;) {
                erase();

                if (f_redraw) {
                        if ((curdir = getcwd(cwd, sizeof(cwd))) == NULL)
                                continue; /*TODO: die? */
                        entcleanup(win->ents);
                        win->nf = entcount(curdir);
                        if ((win->ents = entget(curdir, win->nf)) == NULL)
                                continue; /*TODO: is this actually useful? */
                        f_redraw = 0;
                }

                SEL_CORRECT;
                pathdraw(curdir);
                entprint(win);
                statsprint();
                if (f_fpreview)
                        filepreview();

                /*TODO: what the hell?*/
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
