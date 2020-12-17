#ifndef CONFIG_H
#define CONFIG_H

/* TODO: useless? */
#define CD(dir) {.s = (dir)}
#define SHCMD(cmd) {.s = (cmd)}

//static int dircolor = CYAN;
//static int filecolor = GREEN;
//static int linkcolor = YELLOW;

/*
 * when mod is set to 0, it means there's no mod
 *
 * key can be any ascii character or ncurses key macro
 *
 * available functions:
 * nav:         handles navigation
 *              available flags:
 *              1. NAV_LEFT
 *              2. NAV_RIGHT
 *              3. NAV_UP
 *              4. NAV_DOWN
 *              5. NAV_TOP
 *              6. NAV_BOTTOM
 *              7. NAV_SELECT
 *              8. NAV_SHOWALL
 *              9. NAV_INFO
 *              10. NAV_REDRAW
 *              11. NAV_EXIT
 *
 * cd:          go to specified directory
 *
 * run:         run a predefined shell command. the selected items
 *              will act as input to the command
 *
 * builtinrun:  runs a specific command that is already built inside
 *              inside the file manager. The command is run on the
 *              currently selected entry.
 *              available commands:
 *              1. RUN_PAGER
 *              2. RUN_EDITOR
 *              3. RUN_OPENWITH
 *              4. RUN_RENAME
 *
 * prompt:      execute any shell command on the fly
 *
 * selectitem:  select item
 *
 * args:
 * n:           used for arguments that need a numeric value
 * s:           used for cd 
 * v:           used for commands, NULL when no command is needed
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
        {  0,          'g',            nav,             {.n = NAV_TOP} },
        {  0,          'G',            nav,             {.n = NAV_BOTTOM} },
        {  0,          ' ',            nav,             {.n = NAV_SELECT} },
        {  0,          '.',            nav,             {.n = NAV_SHOWALL} },
        {  0,          'i',            nav,             {.n = NAV_INFO} },
        {  0,          CTRL('r'),      nav,             {.n = NAV_REDRAW} },
        {  0,          'q',            nav,             {.n = NAV_EXIT} },
        {  0,          '~',            cd,              CD("/home/christos") },
        {  0,          CTRL('n'),      cd,              CD("/mnt/christos_ntfs/christos") },
        /* TODO: get rid of builtinrun */
        {  0,          'p',            builtinrun,      {.n = RUN_PAGER} },
        {  0,          'e',            builtinrun,      {.n = RUN_EDITOR} },
        {  0,          'o',            builtinrun,      {.n = RUN_OPENWITH} },
        {  0,          'r',            builtinrun,      {.n = RUN_RENAME} },
        {  0,          'x',            run,             SHCMD("rm -rf") },
        {  0,          'w',            run,             SHCMD("mpv") },
        {  0,          's',            sort,            {.v = NULL} },
        {  0,          ':',            prompt,          {.v = NULL} },
};

#endif /* CONFIG_H */
