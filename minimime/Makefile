# Top level Makefile
.include "Make.conf"

SUBDIRS=src

all: .DEFAULT

.DEFAULT:
.for dir in $(SUBDIRS)
	cd $(dir) && make $(.TARGETS)
.endfor	
