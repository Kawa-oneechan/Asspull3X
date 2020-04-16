LDFLAGS=$(DEFINES) $(INCLUDES) -lSDL2 -lGL -lm
TARGET=clunibus

CSRC=$(wildcard *.c) \
     musashi/m68kcpu.c \
     musashi/m68kfpu.c \
     musashi/m68kops.c
CXXSRC=$(wildcard *.cpp)

OBJ=$(CSRC:.c=.o) $(CXXSRC:.cpp=.o)

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
	DEFINES=-DWIN32 -DCLANG
	INCLUDES=-IC:/libs/SDL2-2.0.7/include/
	RUN=$(TARGET).exe
else
	ifeq ($(platform),x)
		ifeq ($(PREFIX),)
			PREFIX=/usr/local
		endif
	endif
	RUN=./$(TARGET)
endif

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	$(delete) $(OBJ) $(TARGET)

.PHONY: run
run: $(TARGET)
	$(RUN)

.PHONY: install
install: $(TARGET)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $< $(DESTDIR)$(PREFIX)/bin/clunibus

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/clunibus
