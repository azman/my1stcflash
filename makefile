# makefile for my1stcflash

PROJECT = my1stcflash
MAINPRO = $(PROJECT)
OBJECTS = my1comlib.o my1cons.o my1text.o my1list.o my1stc.o $(PROJECT).o
EXTPATH = ../my1termu/src
EX2PATH = ../my1codelib/src
VERSION ?= $(shell date +%Y%m%d)
PLATBIN ?= $(shell uname -m)
PACKDIR = $(PROJECT)-$(shell cat VERSION)
PACKDAT = README LICENSE CHANGELOG
ARCHIVE = tar cjf
ARCHEXT = .tar.bz2

COPY = cp
DELETE = rm -rf
CFLAGS += -Wall --static -I$(EXTPATH) -I$(EX2PATH)
LFLAGS +=
OFLAGS =
XFLAGS =
PFLAGS = -DPROGVERS=\"$(VERSION)\"

ifeq ($(DO_MINGW),YES)
	MAINPRO = $(PROJECT).exe
	PLATBIN = mingw
	ARCHIVE = zip -r
	ARCHEXT = .zip
	XTOOL_DIR	?= /home/share/tool/mingw
	XTOOL_TARGET	= $(XTOOL_DIR)
	CROSS_COMPILE	= $(XTOOL_TARGET)/bin/i686-pc-mingw32-
	TARGET_ARCH =
	CFLAGS += -I$(XTOOL_DIR)/include -DDO_MINGW $(TARGET_ARCH)
	LDFLAGS += -L$(XTOOL_DIR)/lib
	LDFLAGS += -Wl,-subsystem,windows
endif
DISTVER = $(VERSION)-$(PLATBIN)
PACKDAT += $(MAINPRO)

CC = $(CROSS_COMPILE)gcc
CPP = $(CROSS_COMPILE)g++
RES = $(CROSS_COMPILE)windres
debug: XFLAGS += -DMY1DEBUG
pack: ARCNAME = $(PROJECT)-$(DISTVER)-$(shell date +%Y%m%d)$(ARCHEXT)
version: PFLAGS = -DPROGVERS=\"$(shell cat VERSION)\"

default: main

all: main

main: $(MAINPRO)

new: clean all

debug: new

pack: version
	mkdir -pv $(PACKDIR)
	$(COPY) $(PACKDAT) $(PACKDIR)/
	$(DELETE) $(ARCNAME)
	$(ARCHIVE) $(ARCNAME) $(PACKDIR)

version: new

$(MAINPRO): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $+ $(LFLAGS) $(OFLAGS)

%.o: src/%.c src/%.h
	$(CC) $(CFLAGS) $(XFLAGS) $(PFLAGS) -c $<

%.o: src/%.c
	$(CC) $(CFLAGS) $(XFLAGS) $(PFLAGS) -c $<

%.o: src/%.cpp src/%.hpp
	$(CPP) $(CFLAGS) $(XFLAGS) $(PFLAGS) -c $<

%.o: src/%.cpp src/%.h
	$(CPP) $(CFLAGS) $(XFLAGS) $(PFLAGS) -c $<

%.o: src/%.cpp
	$(CPP) $(CFLAGS) $(XFLAGS) $(PFLAGS) -c $<

%.o: $(EXTPATH)/%.c $(EXTPATH)/%.h
	$(CC) $(CFLAGS) $(XFLAGS) -c $<

%.o: $(EX2PATH)/%.c $(EX2PATH)/%.h
	$(CC) $(CFLAGS) $(XFLAGS) -c $<

clean:
	-$(DELETE) $(MAINPRO) $(OBJECTS) $(PROJECT)* *.o *.bz2 *.exe *.zip
