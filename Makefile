# Makefile to compile Baby-Git program in Unix-like systems.
#
# Synopsis:
#
# For Linux distributions and MSYS2:
#
# $ make
# $ make install
# $ make clean
#
# For FreeBSD:
#
# $ gmake
# $ gmake install
# $ gmake clean

SHELL   = /bin/sh
INSTALL = install
prefix  = $(HOME)
bindir  = $(prefix)/bin

CC      = cc
CFLAGS  = -g -Wall -O3
LDLIBS  = -lcrypto -lz

OBJ_DIR    = obj
TARGET_DIR = target
BASE_RCOBJ   = read-cache.o
BASE_OBJS    = init-db.o update-cache.o write-tree.o commit-tree.o read-tree.o \
               cat-file.o show-diff.o
RCOBJ   = $(addprefix $(OBJ_DIR)/, $(BASE_RCOBJ))
OBJS    = $(addprefix $(OBJ_DIR)/, $(BASE_OBJS))
PROGS  := $(addprefix $(TARGET_DIR)/, $(subst .o,,$(BASE_OBJS)))

ifeq ($(OS),Windows_NT)
    CFLAGS += -D BGIT_WINDOWS
else
    SYSTEM := $(shell uname -s)

    ifeq ($(SYSTEM),Linux)
        CFLAGS += -D BGIT_UNIX
    else
        ifeq ($(SYSTEM),FreeBSD)
            CFLAGS += -D BGIT_UNIX
        else
            ifeq ($(SYSTEM),Darwin)
                OPENSSL_PATH := $(shell brew --prefix openssl)
                CFLAGS += -D BGIT_DARWIN -I$(OPENSSL_PATH)/include
                LDFLAGS = -L$(OPENSSL_PATH)/lib
            endif
        endif
    endif
endif

.PHONY : all install clean backup test dirs

.SECONDARY: $(OBJS) $(RCOBJ)

all    : dirs $(PROGS)

dirs:
	@mkdir -p $(OBJ_DIR) $(TARGET_DIR)

$(TARGET_DIR)/%: $(OBJ_DIR)/%.o $(RCOBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: %.c cache.h
	$(CC) $(CFLAGS) -c -o $@ $<


install : $(PROGS)
	$(INSTALL) $(PROGS) $(bindir)

clean   :
	rm -rf $(OBJ_DIR) $(TARGET_DIR)

backup  : clean
	cd ..; tar czvf babygit.tar.gz baby-git --exclude .git* \
	    --exclude .dircache --exclude temp_git_file*

test    :
	@echo "SYSTEM = $(SYSTEM)"
	@echo "CC = $(CC)"
	@echo "PROGS = $(PROGS)" 

