# $Id: Makefile,v 1.9 2004/06/24 07:25:34 jfi Exp $
# MiniMIME makefile
#
.include "../Make.conf"
CC?=gcc
LD?=ld
YACC?=yacc
FLEX?=flex
DEFINES?=
SRCS= \
	mimeparser.tab.c \
	mimeparser.yy.c \
	mm_init.c \
	mm_base64.c \
	mm_codecs.c \
	mm_contenttype.c \
	mm_context.c \
	mm_envelope.c \
	mm_error.c \
	mm_header.c \
	mm_mem.c \
	mm_mimepart.c \
	mm_mimeutil.c \
	mm_param.c \
	mm_parse.c \
	mm_util.c \

HAVE_DEBUG?=1

.if !$(HAVE_STRLCAT)
SRCS+= strlcat.c
.else
DEFINES+= -DHAVE_STRLCAT
.endif

.if !$(HAVE_STRLCPY) 
SRCS+= strlcpy.c
.else
DEFINES+= -DHAVE_STRLCPY
.endif

.if $(HAVE_DEBUG)
DEBUG=-ggdb -g3
.else
DEBUG=
.endif

.if make(all) || make(minimime) || make(debug)
SRCS+= minimime.c
.endif

# Uncomment the following if you need to debug memory leaks within the
# library. ATTENTION: the code is not suitable for production purposes
# and could behave unexpectedly. You have been warned ;-).

#DEFINES+= -D__HAVE_LEAK_DETECTION
#DEFINES+= -DMM_DEBUG

CFLAGS+=-Wall $(DEBUG) -I. $(DEFINES)
LDFLAGS?=
LIBS?=
OBJS=$(SRCS:.c=.o)
VERSION=0.1
HDRS=mm.h mm_util.h
TARGET=minimime
LIBNAME=libmmime.so.$(VERSION)
ARNAME=libmmime.a	
PREFIX?=

all: $(LIBNAME)

depend: 
	mkdep $(CFLAGS) $(SRCS)

$(OBJS): $(HDRS) Makefile ../Make.conf

debug: $(OBJS)
	$(CC) -o minimime $(OBJS) $(CFLAGS) $(LDFLAGS) $(LIBS)	

$(LIBNAME): $(OBJS)
	$(LD) -shared -o $(LIBNAME) $(OBJS)
	ar -rv $(ARNAME) $(OBJS)
	ranlib $(ARNAME)

$(LIBNAME): $(OBJS)

$(ARNAME): $(OBJS)

minimime: $(LIBNAME) minimime.o
	$(CC) -o $(TARGET) minimime.o -L. -lmmime

mimeparser.yy.o: mimeparser.l
	$(FLEX) -Pmimeparser_yy -omimeparser.yy.c mimeparser.l
	$(CC) -c $(CFLAGS) -Wno-unused mimeparser.yy.c

mimeparser.tab.o: mimeparser.y
	$(YACC) -d -pmimeparser_yy -omimeparser.tab.c mimeparser.y
	$(CC) -c $(CFLAGS) -Wno-unused mimeparser.tab.c

test: $(LIBNAME)
	@cd tests && make
	@echo "****************************************************************"
	@echo Running testsuite for libminimime...
	@echo "****************************************************************"
	@export LD_LIBRARY_PATH=$(.CURDIR)/src
	@cd tests && ./test.sh

install: $(LIBNAME)
	test -d $(PREFIX) || mkdir -p $(PREFIX)
	install -m 644 -o root -g 0 -c $(LIBNAME) /usr/local/lib

clean:
	rm -f *.yy.c
	rm -f *.tab.*
	rm -f *.o
	rm -f *.core
	rm -f $(TARGET)
	rm -f $(LIBNAME)
	rm -f ${ARNAME}
	rm -f .depend
	cd tests && make clean
