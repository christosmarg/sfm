#ifndef CONFIG_H
#define CONFIG_H

#define SHCMD(cmd) { .f = (const char*[]){"/usr/bin/sh", "-c", cmd, NULL} }

static chtype winborder = A_INVIS;

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
        { 'g',         'h',            cd,              {.d = "/home/christos"} },
        { 'g',         'u',            cd,              {.d = "/usr"} },
        { 'd',         'D',            spawn,           SHCMD("rm -rf %s") },
        {  0,          ':',            promptget,       {.f = NULL} },
        {  0,          'q',            quit,            {.f = NULL} },
};

#endif /* CONFIG_H */
