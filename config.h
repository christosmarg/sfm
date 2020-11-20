#ifndef CONFIG_H
#define CONFIG_H

#define SHCMD(cmd) { .f = (const char *)(cmd) }

static int nctx = 4;                    /* number of available contexts */
static chtype winborder = A_INVIS;      /* window border */

/*
 * when mod is set to 0, it means there's no mod
 *
 * key can be any ascii character or ncurses key macro
 *
 * available functions:
 * nav:         handles navigation
 * cd:          go to specified directory
 * run:         run a predefined shell command. the selected items
 *              will act as input to the command
 * prompt:      execute any shell command on the fly
 * select:      select item
 * quit:        exit program
 *
 * args:
 * n:           used for nav functions
 * d:           used for cd
 * f:           used for commands, NULL when no command is needed
 */
static Key keys[] = {
        /* mod         key             func             arg */
        {  0,          KEY_LEFT,       nav,             {.n = NAV_LEFT} },
        {  0,          'h',            nav,             {.n = NAV_LEFT} },
        {  0,          KEY_RIGHT,      nav,             {.n = NAV_RIGHT} },
        {  0,          'l',            nav,             {.n = NAV_RIGHT} },
        {  0,          '\n',           nav,             {.n = NAV_RIGHT} },
        {  0,          KEY_UP,         nav,             {.n = NAV_UP} },
        {  0,          'k',            nav,             {.n = NAV_UP} },
        {  0,          KEY_DOWN,       nav,             {.n = NAV_DOWN} },
        {  0,          'j',            nav,             {.n = NAV_DOWN} },
        { 'g',         'g',            nav,             {.n = NAV_TOP} },
        {  0,          'G',            nav,             {.n = NAV_BOTTOM} },
        {  0,          '.',            nav,             {.n = NAV_SHOWALL} },
        {  0,          'f',            nav,             {.n = NAV_FPREVIEW} },
        {  0,          'r',            nav,             {.n = NAV_REDRAW} },
        { 'g',         'u',            cd,              {.d = "/usr"} },
        { 'g',         '@',            cd,              {.d = "/"} },
        {  0,          '~',            cd,              {.d = "/home/christos"} },
        { 'g',         'n',            cd,              {.d = "/mnt/christos_ntfs/christos"} },
        {  0,          ' ',            select,          {.f = NULL} },
        { 'd',         'D',            run,             SHCMD("rm -rf") },
        {  0,          ':',            prompt,          {.f = NULL} },
        {  0,          'q',            quit,            {.f = NULL} },
};

#endif /* CONFIG_H */
