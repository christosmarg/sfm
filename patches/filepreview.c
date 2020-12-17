static void
filepreview(void)
{
        WINDOW *fw;
        Entry *ent = &win->ents[win->sel];
        FILE *fp;
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
