BIN = ze03
OBJS = main.o
CFLAGS = -Wall -ansi -pedantic
LIBS =

all: ${BIN}

.c.o:
	${CC} -c ${CFLAGS} -o $@ $<

${BIN}: ${OBJS}
	${CC} -o $@ $^ ${LIBS}

clean:
	rm -f ${BIN} ${OBJS}
