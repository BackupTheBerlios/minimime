# $Id: Makefile,v 1.1 2004/05/03 22:05:54 jfi Exp $
# MiniMIME makefile
#
.include "../Make.conf"
CC?=gcc
LD?=ld
DEFINES?=
SRCS= \
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

# uncomment the following if you need to debug memory leaks within the
# library.
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

minimime: $(LIBNAME) minimime.o
#$(CC) $(CFLAGS) -o minimime.o -c minimime.c 
	$(CC) -o $(TARGET) minimime.o -L. -lmmime

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
	rm -f *.o
	rm -f *.core
	rm -f $(TARGET)
	rm -f $(LIBNAME)
	rm -f .depend

