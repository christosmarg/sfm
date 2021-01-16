#ifndef CONFIG_H
#define CONFIG_H

/* c1 e2 27 2e 00 60 33 f7 c6 d6 ab c4 */
/* https://upload.wikimedia.org/wikipedia/commons/1/15/Xterm_256color_chart.svg */

static int colors[] = {
        [C_BLK] = 0x00, /* TODO: Block device */
        [C_CHR] = 0xe2, /* Character device */
        [C_DIR] = 0x27, /* Directory */
        [C_EXE] = 0xd6, /* Executable file */
        [C_FIL] = 0xff, /* Regular file */
        [C_HRD] = 0x00, /* TODO: Hard link */
        [C_LNK] = 0x33, /* Symbolic link */
        [C_MIS] = 0x00, /* TODO: Missing file OR file details */
        [C_ORP] = 0x00, /* TODO: Orphaned symlink */
        [C_PIP] = 0x00, /* TODO: Named pipe (FIFO) */
        [C_SOC] = 0x2e, /* Socket */
        [C_UND] = 0x00, /* TODO: Unknown OR 0B regular/exe file */
        [C_INF] = 0xf7, /* Information */
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
        {  '~',            cd,              {.s = "/home/christos"} },
        {  CTRL('b'),      cd,              {.s = "/storage"} },
        /* TODO: get rid of builtinrun */
        {  'p',            builtinrun,      {.n = RUN_PAGER} },
        {  'e',            builtinrun,      {.n = RUN_EDITOR} },
        {  'o',            builtinrun,      {.n = RUN_OPENWITH} },
        {  'r',            builtinrun,      {.n = RUN_RENAME} },
        {  'x',            run,             {.s = "rm -rf"} },
        {  's',            sort,            {.v = NULL} },
        {  ':',            prompt,          {.v = NULL} },
};

#endif /* CONFIG_H */
