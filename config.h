#ifndef CONFIG_H
#define CONFIG_H

/* TODO: useless? */
#define CD(dir) {.s = (dir)}
#define SHCMD(cmd) {.s = (cmd)}

static int colors[] = {
        COLOR_CYAN,     /* directories */
        COLOR_GREEN,    /* regular files */
        COLOR_YELLOW,   /* symlinks */
};

static Key keys[] = {
        /* key             func             arg */
        {  KEY_LEFT,       nav,             {.n = NAV_LEFT} },
        {  'h',            nav,             {.n = NAV_LEFT} },
        {  KEY_RIGHT,      nav,             {.n = NAV_RIGHT} },
        {  'l',            nav,             {.n = NAV_RIGHT} },
        {  '\n',           nav,             {.n = NAV_RIGHT} },
        {  KEY_UP,         nav,             {.n = NAV_UP} },
        {  'k',            nav,             {.n = NAV_UP} },
        {  KEY_DOWN,       nav,             {.n = NAV_DOWN} },
        {  'j',            nav,             {.n = NAV_DOWN} },
        {  'g',            nav,             {.n = NAV_TOP} },
        {  'G',            nav,             {.n = NAV_BOTTOM} },
        {  ' ',            nav,             {.n = NAV_SELECT} },
        {  '.',            nav,             {.n = NAV_SHOWALL} },
        {  'i',            nav,             {.n = NAV_INFO} },
        {  CTRL('r'),      nav,             {.n = NAV_REDRAW} },
        {  'q',            nav,             {.n = NAV_EXIT} },
        {  '~',            cd,              CD("/home/christos") },
        {  CTRL('n'),      cd,              CD("/mnt/christos_ntfs/christos") },
        /* TODO: get rid of builtinrun */
        {  'p',            builtinrun,      {.n = RUN_PAGER} },
        {  'e',            builtinrun,      {.n = RUN_EDITOR} },
        {  'o',            builtinrun,      {.n = RUN_OPENWITH} },
        {  'r',            builtinrun,      {.n = RUN_RENAME} },
        {  'x',            run,             SHCMD("rm -rf") },
        {  's',            sort,            {.v = NULL} },
        {  ':',            prompt,          {.v = NULL} },
};

#endif /* CONFIG_H */
