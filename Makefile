SHELL	= /bin/sh
DEPS	= isinternal.h vbisam.h Makefile
LIB	= vbisam
ALB	= /usr/lib/lib$(LIB).a
SLB	= /usr/lib/lib$(LIB).so
# ****************************************************************************
# CFLAGS info
# ===========
# -DEBUG:	Includes some (useful?) debugging functions
# -DDEV:	Forces node size to 256 bytes
#		Otherwise uses 1kB (32bit versions) or 4kB (64bit versions)
# -O3 -s:	Optimizes code and strips symbols (Better / Faster code?)
# -g:		Produces code with which gdb can be used
# -D_FILE_OFFSET_BITS=64:
#		Produces 64-bit file-I/O code (Files incompatable with 32-bit)
# ****************************************************************************
CFLAGS	= -Wall -DDEBUG -O3 -s -D_FILE_OFFSET_BITS=64
CFLAGS	= -Wall -DDEV -DDEBUG -D_FILE_OFFSET_BITS=64 -g
CFLAGS	= -Wall -DDEBUG -g
CFLAGS	= -Wall -DDEBUG -O3 -s
CFLAGS	= -Wall -g
CFLAGS	= -Wall -O3 -s
CFLAGS	= -Wall -DDEBUG -g
CFLAGS	= -Wall -DDEBUG -O3 -s
SRCS	= \
	isDecimal.c \
	isHelper.c \
	isbuild.c \
	isdelete.c \
	isopen.c \
	isread.c \
	isrewrite.c \
	istrans.c \
	iswrite.c \
	vbDataIO.c \
	vbIndexIO.c \
	vbKeysIO.c \
	vbLocking.c \
	vbLowLevel.c \
	vbMemIO.c
OBJS	= ${SRCS:.c=.o}

all:	${ALB} ${SLB} IsamTest CvtTo64

.o:
	cc $(CFLAGS) -o $@ $< -l$(LIB)

${ALB}:	${OBJS} Makefile
	${AR} srv $@ $?

${SLB}:	${OBJS} Makefile
	${LD} -shared -o $@ ${OBJS}

clean:
	rm -f *.o

CvtTo64:	CvtTo64.o $(SLB)

CvtTo64.o:	CvtTo64.c $(DEPS)

IsamTest:	IsamTest.o $(SLB)

IsamTest.o:	IsamTest.c $(DEPS)

isDecimal.o:	isDecimal.c $(DEPS) decimal.h

isHelper.o:	isHelper.c $(DEPS)

isbuild.o:	isbuild.c $(DEPS)

isdelete.o:	isdelete.c $(DEPS)

isopen.o:	isopen.c $(DEPS)

isread.o:	isread.c $(DEPS)

isrewrite.o:	isrewrite.c $(DEPS)

istrans.o:	istrans.c $(DEPS)

iswrite.o:	iswrite.c $(DEPS)

vbDataIO.o:	vbDataIO.c $(DEPS)

vbIndexIO.o:	vbIndexIO.c $(DEPS)

vbKeysIO.o:	vbKeysIO.c $(DEPS)

vbLocking.o:	vbLocking.c $(DEPS)

vbLowLevel.o:	vbLowLevel.c $(DEPS)

vbMemIO.o:	vbMemIO.c $(DEPS)

