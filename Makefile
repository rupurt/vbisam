# Title:	Makefile
# Copyright:	(C) 2003 Trevor van Bremen
# Author:	Trevor van Bremen
# Created:	11Dec2003
# Description:
#	This is the main makefile that BUILDS all this stuff (I hope)
# Version:
#	$Id: Makefile,v 1.4 2004/01/03 02:28:48 trev_vb Exp $
# Modification History:
#	$Log: Makefile,v $
#	Revision 1.4  2004/01/03 02:28:48  trev_vb
#	TvB 02Jan2004 WAY too many changes to enumerate!
#	TvB 02Jan2003 Transaction processing done (excluding iscluster)
#	
#	Revision 1.3  2003/12/23 03:24:43  trev_vb
#	TvB 22Dec2002 Added in -fPIC to CFLAGS for portability + changed default libdir
#	
#	Revision 1.2  2003/12/22 04:42:50  trev_vb
#	TvB 21Dec2003 Added in a cvs header section
#	
#
SHELL	= /bin/sh
DEPS	= isinternal.h vbisam.h Makefile
LIB	= vbisam
ALB	= /usr/lib/lib$(LIB).a
SLB	= /usr/lib/lib$(LIB).so
# ****************************************************************************
# CFLAGS info
# ===========
# -fPIC		Seems to be needed to build libraries on MANY systems
# -DEBUG:	Includes some (useful?) debugging functions
# -DDEV:	Forces node size to 256 bytes
#		Otherwise uses 1kB (32bit versions) or 4kB (64bit versions)
# -O3 -s:	Optimizes code and strips symbols (Better / Faster code?)
# -g:		Produces code with which gdb can be used
# -pg:		Generate gprof-able code
# -D_FILE_OFFSET_BITS=64:
#		Produces 64-bit file-I/O code (Files incompatable with 32-bit)
# ****************************************************************************
CFLAGS	= -fPIC -Wall -DDEBUG -O3 -s -D_FILE_OFFSET_BITS=64
CFLAGS	= -fPIC -Wall -DDEV -DDEBUG -D_FILE_OFFSET_BITS=64 -pg -g
CFLAGS	= -fPIC -Wall -DDEBUG -pg -g
CFLAGS	= -fPIC -Wall -DDEBUG -O3 -s
CFLAGS	= -fPIC -Wall -pg -g
CFLAGS	= -fPIC -Wall -O3 -s

SRCS	= \
	isDecimal.c \
	isHelper.c \
	isaudit.c \
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

all:	${ALB} ${SLB} IsamTest

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

