##############################################################################
#
# File:        makefile
#
# Description: This makefile builds the WLD for FM sample application
#
# Copyright � 2021 SafeNet, Inc. All rights reserved.
#
# All rights reserved. This file contains information that is
# proprietary to SafeNet, Inc. and may not be distributed
# or copied without written consent from SafeNet, Inc.
#
##############################################################################

# default the FMSDK path
FMSDK?=/usr/safenet/lunafmsdk

LUNASDK=/usr/safenet/lunaclient

DEFINES=-DOS_LINUX -DOS_UNIX -D__EXTENSIONS__ -D_RWSTD_MULTI_THREAD -D_REENTRANT -D_THREAD_SAFE -DLUNA_LITTLE_ENDIAN -DUSE_PTHREADS -DLUNA_LP64_CORRECT -DDEBUG -D_USESSL -DDISABLE_CA_EXT

# linux with GNU C defines
CC=gcc
CPP=g++
CFLAGS=-fPIC -x c -c -Wall

ifeq ($(DEBUG),)
CFLAGS+=-O2
EXTRALFLAGS=
else
CFLAGS+=-ggdb
EXTRALFLAGS=-ggdb
endif

EXTRALIBS=-lc -lpthread -ldl -lrt
INCLUDES=-I../include -I$(LUNASDK)/samples/include -I$(FMSDK)/include/fm/host -I$(FMSDK)/include

# specify a different output directory on make command line to chage o/p folder
OUTDIR?=output

# normal rules
$(OUTDIR)/obj/%.o : %.c  $(OUTDIR)/obj
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES)  $< -o$@

# define primary target
all: $(OUTDIR)/bin $(OUTDIR)/bin/wldapp

# rules to create output dirs
$(OUTDIR)/obj:
	mkdir -p $@

$(OUTDIR)/bin:
	mkdir -p $@

OBJS=\
	$(OUTDIR)/obj/wld.o \
	$(OUTDIR)/obj/main.o

LIB_CRYPTOKI=Cryptoki2_64

$(OUTDIR)/bin/wldapp: $(OBJS)
	$(CPP) -o$@ $(EXTRALFLAGS) $^ -L$(LUNASDK)/lib -l$(LIB_CRYPTOKI) -lethsm $(EXTRALIBS) -Wl,-rpath=$(LUNASDK)/lib

clean:
	-rm -r $(OUTDIR)/bin $(OUTDIR)/obj $(OUTDIR)

