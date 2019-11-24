OBJS = Asspull3X.cpp device.cpp ini.cpp keyboard.cpp memory.cpp sound.cpp ui.cpp video.cpp musashi/m68kcpu.c musashi/m68kops.c nativefiledialog/src/nfd_dummy.cpp

LIBS = -lSDL2 -lGL

TARGET = Asspull3X

CFLAGS = /std:c++11

DEFINES = 
INCLUDES = 

# platform detection shamelessly stolen from byuu
ifeq ($(platform),)
  uname := $(shell uname -a)
  ifeq ($(uname),)
    platform := win
    delete = del $(subst /,\,$1)
  else ifneq ($(findstring Windows,$(uname)),)
    platform := win
    delete = del $(subst /,\,$1)
  else ifneq ($(findstring NT,$(uname)),)
    platform := win
    delete = del $(subst /,\,$1)
  else ifneq ($(findstring CYGWIN,$(uname)),)
    platform := win
    delete = del $(subst /,\,$1)
  else ifneq ($(findstring Darwin,$(uname)),)
    platform := osx
    delete = rm -f $1
  else
    platform := x
    delete = rm -f $1
  endif
endif

ifeq ($(platform),win)
	DEFINES = -DWIN32 -DCLANG
	INCLUDES = -IC:/libs/SDL2-2.0.7/include/
endif

all: $(OBJS)
	clang $(OBJS) -w $(DEFINES) $(INCLUDES) $(LIBS) -lstdc++ -lm -o $(TARGET)
