CC=gcc
SRCS= \
	mm_init.c \
	mm_contenttype.c \
	mm_context.c \
	mm_error.c \
	mm_flatten.c \
	mm_header.c \
	mm_mem.c \
	mm_mimepart.c \
	mm_parse.c \
	mm_util.c \
	minimime.c
DEFINES=
# uncomment the following if you need to debug memory leaks within the
# library.
DEFINES+= -D__HAVE_LEAK_DETECTION
#DEFINES+= -DMM_DEBUG
CFLAGS=-Wall -ggdb -g3 -I. $(DEFINES)
LDFLAGS=
LIBS=
OBJS=$(SRCS:.c=.o)
HDRS=mm.h mm_util.h
TARGET=minimime

all: minimime

minimime: $(OBJS) sysdep
	$(CC) $(LDFLAGS) $(OBJS) sys/mmsysdep.a $(LIBS) -o $(TARGET)

sysdep: $(OBJS)
ifneq ($(test -f sys/mmsysdep.a && echo -n 1), 1)
	echo "Building system dependencies..."
	cd sys && make
endif

$(OBJS): $(HDRS) GNUmakefile 

clean:
	rm -f *.o
	rm -f *.core
	rm -f $(TARGET)
	cd sys && make clean
