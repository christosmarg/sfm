#ifndef CONFIG_H
#define CONFIG_H

#define SHCMD(cmd) { .v = (const char *)(cmd) }

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
 * builtinrun:  runs a specific command that is already built inside
 *              inside the file manager. The command is run on the
 *              currently selected entry.
                available commands:
                1. RUN_PAGER
                2. RUN_EDITOR
                3. RUN_RENAME
 * prompt:      execute any shell command on the fly
 * selectitem:  select item
 * quit:        exit program
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
        { 'g',         'g',            nav,             {.n = NAV_TOP} },
        {  0,          'G',            nav,             {.n = NAV_BOTTOM} },
        {  0,          '.',            nav,             {.n = NAV_SHOWALL} },
        {  0,          'f',            nav,             {.n = NAV_FPREVIEW} },
        {  0,          'i',            nav,             {.n = NAV_INFO} },
        //{  0,          'r',            nav,             {.n = NAV_REDRAW} },
        { 'g',         'u',            cd,              {.s = "/usr"} },
        {  0,          '~',            cd,              {.s = "/home/christos"} },
        { 'g',         'n',            cd,              {.s = "/mnt/christos_ntfs/christos"} },
        {  0,          ' ',            selectitem,      {.v = NULL} },
        {  0,          'p',            builtinrun,      {.n = RUN_PAGER} },
        {  0,          'e',            builtinrun,      {.n = RUN_EDITOR} },
        //{  0,          'r',            builtinrun,      {.n = RUN_RENAME} },
        { 'd',         'D',            run,             SHCMD("rm -rf") },
        {  0,          ':',            prompt,          {.v = NULL} },
        {  0,          'q',            quit,            {.v = NULL} },
};

#endif /* CONFIG_H */
