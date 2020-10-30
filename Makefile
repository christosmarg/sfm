# See LICENSE file for copyright and license details.
# sfm - simple file manager
.POSIX:

include config.mk

BIN = sfm
DIST = ${BIN}-${VERSION}
MAN1 = ${BIN}.1

EXT = c
SRC = sfm.c
OBJ = ${SRC:.${EXT}=.o}

all: options ${BIN}

options:
	@echo ${BIN} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

${OBJ}: config.h config.mk

${BIN}: ${OBJ}
	${CC} ${LDFLAGS} ${OBJ} -o $@

.${EXT}.o:
	${CC} -c ${CFLAGS} $<

dist: clean
	${MKDIR} ${DIST}
	${CP} -R config.h config.mk Makefile ${SRC} ${DIST}
	${TAR} ${DIST}.tar ${DIST}
	${GZIP} ${DIST}.tar
	${RM_DIR} ${DIST}

run:
	./${BIN}

install: all
	#${MKDIR} ${DESTDIR}${BIN_DIR} ${DESTDIR}${MAN_DIR}
	${MKDIR} ${DESTDIR}${BIN_DIR}
	${CP} ${BIN} ${BIN_DIR}
	#${CP} ${MAN1} ${DESTDIR}${MAN_DIR}
	#sed "s/VERSION/${VERSION}/g" < ${MAN1} > ${DESTDIR}${MAN_DIR}/${MAN1}
	chmod 755 ${DESTDIR}${BIN_DIR}/${BIN}
	#chmod 644 ${DESTDIR}${MAN_DIR}/${MAN1}

uninstall:
	${RM} ${DESTDIR}${BIN_DIR}/${BIN}
	#${RM} ${DESTDIR}${MAN_DIR}/${MAN1}

clean:
	${RM} ${BIN} ${OBJ} ${DIST}.tar.gz

.PHONY: all options clean dist install uninstall run
