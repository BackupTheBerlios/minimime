# $Id: Makefile,v 1.3 2004/06/01 09:39:48 jfi Exp $
# MiniMIME makefile
#
.include "../Make.conf"
CC?=gcc
LD?=ld
YACC?=yacc
FLEX?=flex
DEFINES?=
SRCS= \
	y.tab.c \
	lex.mm_yy.c \
	mm_init.c \
	mm_base64.c \
	mm_codecs.c \
	mm_contenttype.c \
	mm_context.c \
	mm_error.c \
	mm_flatten.c \
	mm_header.c \
	mm_mem.c \
	mm_mimepart.c \
	mm_param.c \
	mm_parse.c \
	mm_util.c \
	mm_warnings.c

HAVE_DEBUG?=0	

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

depend: .depend
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
#$(CC) $(CFLAGS) -o minimime.o -c minimime.c 
	$(CC) -o $(TARGET) minimime.o -L. -lmmime

lex.mm_yy.o: parser.l
	$(FLEX) -Pmm_yy parser.l
	$(CC) -c $(CFLAGS) lex.mm_yy.c

y.tab.o: parser.y
	$(YACC) -d -pmm_yy parser.y
	$(CC) -c $(CFLAGS) y.tab.c

test: minimime 
	@echo "****************************************************************"
	@echo Running testsuite for libminimime...
	@echo "****************************************************************"
	@export LD_LIBRARY_PATH=$(.CURDIR)/src
	@./test.sh

install: $(LIBNAME)
	test -d $(PREFIX) || mkdir -p $(PREFIX)
	install -m 644 -o root -g 0 -c $(LIBNAME) /usr/local/lib

clean:
	rm -f lex.mm_yy.c
	rm -f y.tab.*
	rm -f *.o
	rm -f *.core
	rm -f $(TARGET)
	rm -f $(LIBNAME)
	rm -f ${ARNAME}
	rm -f .depend

