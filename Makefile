# C64 GrooveGoat build
#
#   make TARGET=linux     native Linux build (needs SDL2 headers + libSDL2)
#   make TARGET=appimage  Linux build for the AppImage (static SDL2, see tools/package.sh)
#   make TARGET=win       Windows x64 cross build (llvm-mingw + SDL2 mingw devel)
#   make TARGET=macnative native macOS build with Homebrew SDL2
#   make TARGET=mac       macOS Apple Silicon cross build (clang + lld + SDK + SDL2.framework)
#   make TARGET=macintel  macOS Intel (x86_64) cross build, macOS 10.13 or newer
#
# Paths to the toolchains can be overridden on the command line.

TARGET ?= linux
TOOLS ?= $(abspath ../tools)
VERSION = 0.92b

BUILDDIR = build/$(TARGET)
SRCDIR = src

CSRC = goattrk2.c gsong.c gorder.c gpattern.c ginstr.c gtable.c gplay.c gdisplay.c \
       gfile.c greloc.c ghelp.c gsound.c gconsole.c gundo.c gplatform.c gaudio.c \
       gfileio.c gdialog.c gres.c gsonglen.c gautotest.c gexport.c gvideo.c gadsr.c ginslib.c grandins.c gbackup.c gpiano.c \
       gsummary.c gwsummary.c gfsummary.c gcmdsummary.c gsumui.c gstepedit.c gstepui.c gspeedui.c gmidiparse.c gmidi.c
CXXSRC = gsid.cpp
RESIDSRC = resid/envelope.cpp resid/extfilt.cpp resid/filter.cpp resid/pot.cpp resid/sid.cpp \
       resid/wave.cpp resid/wave6581__ST.cpp resid/wave6581_P_T.cpp resid/wave6581_PS_.cpp \
       resid/wave6581_PST.cpp resid/wave8580__ST.cpp resid/wave8580_P_T.cpp resid/wave8580_PS_.cpp \
       resid/wave8580_PST.cpp resid/voice.cpp \
       resid-fp/envelopefp.cpp resid-fp/extfiltfp.cpp resid-fp/filterfp.cpp resid-fp/potfp.cpp \
       resid-fp/sidfp.cpp resid-fp/wavefp.cpp resid-fp/voicefp.cpp
ASMSRC = asm/asmtab.c asm/chnkpool.c asm/expr.c asm/lexyy.c asm/log.c asm/membuf.c \
       asm/membufio.c asm/namedbuf.c asm/parse.c asm/pc.c asm/vec.c

OPT = -O2
COMMONFLAGS = $(OPT) -I$(SRCDIR) -I$(SRCDIR)/asm -DGG_VERSION=\"$(VERSION)\" \
       -Wno-unused-result -Wno-format-truncation -Wno-deprecated-declarations
CFLAGS_EXTRA =
CXXFLAGS_EXTRA = -fno-exceptions -fno-rtti

ifeq ($(TARGET),linux)
CC = gcc
CXX = g++
SDLINC ?= $(TOOLS)/SDL2src/include
SDLLIB ?= /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0
COMMONFLAGS += -I$(SDLINC) -D_REENTRANT -fno-stack-protector -U_FORTIFY_SOURCE
LDFLAGS = -Wl,-rpath,'$$ORIGIN/lib' -Wl,--as-needed
LIBS = $(SDLLIB) -lm -ldl -lpthread
EXE = $(BUILDDIR)/groovegoat
endif

# Linux AppImage: SDL 2 linked statically (built with dynamically loaded
# X11 / ALSA / PulseAudio backends), so the only dependency is glibc
ifeq ($(TARGET),appimage)
CC = gcc
CXX = g++
SDLSTATIC ?= $(TOOLS)/sdlstatic
COMMONFLAGS += -I$(SDLSTATIC)/include/SDL2 -D_REENTRANT -fno-stack-protector -U_FORTIFY_SOURCE
LDFLAGS = -Wl,--as-needed -static-libstdc++ -static-libgcc
LIBS = $(SDLSTATIC)/lib/libSDL2.a -lm -ldl -lpthread -lrt
EXE = $(BUILDDIR)/groovegoat
CSRC += glibccompat.c
endif

ifeq ($(TARGET),win)
MINGW ?= $(TOOLS)/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64
SDLMINGW ?= $(TOOLS)/SDL2-2.32.10/x86_64-w64-mingw32
CC = $(MINGW)/bin/x86_64-w64-mingw32-clang
CXX = $(MINGW)/bin/x86_64-w64-mingw32-clang++
WINDRES = $(MINGW)/bin/x86_64-w64-mingw32-windres
COMMONFLAGS += -I$(SDLMINGW)/include/SDL2 -Dmain=SDL_main -D__WIN32__
LDFLAGS = -mwindows -static
LIBS = -L$(SDLMINGW)/lib -lmingw32 -lSDL2main -lSDL2 -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 \
       -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lcfgmgr32
EXE = $(BUILDDIR)/GrooveGoat.exe
WINRES = $(BUILDDIR)/groovegoat_res.o
endif

ifneq ($(filter mac macintel,$(TARGET)),)
CC = clang
CXX = clang++
MACSDK ?= $(TOOLS)/macsdk
SDLFW ?= $(TOOLS)/sdlmac/SDL2
ifeq ($(TARGET),macintel)
MACMIN = 10.13
MACTARGET = -target x86_64-apple-macos$(MACMIN) -isysroot $(MACSDK)
else
MACMIN = 11.0
MACTARGET = -target arm64-apple-macos$(MACMIN) -isysroot $(MACSDK)
endif
COMMONFLAGS += $(MACTARGET) -F$(SDLFW) -I$(SDLFW)/SDL2.framework/Headers -D__MACOSX__
CXXFLAGS_EXTRA += -nostdinc++ -I$(SRCDIR)/macstub
LDFLAGS = $(MACTARGET) -fuse-ld=lld -F$(SDLFW) -Wl,-rpath,@executable_path/../Frameworks \
       -Wl,-platform_version,macos,$(MACMIN),14.0 -Wl,-dead_strip
LIBS = -framework SDL2 -lSystem
EXE = $(BUILDDIR)/groovegoat
endif

# Native macOS build on the Mac itself (SDL2 from Homebrew)
ifeq ($(TARGET),macnative)
CC = clang
CXX = clang++
SDLPREFIX ?= $(shell brew --prefix sdl2 2>/dev/null || echo /opt/homebrew)
COMMONFLAGS += -I$(SDLPREFIX)/include/SDL2 -D_REENTRANT -D__MACOSX__ -DGG_COREMIDI -Dvtbl2q_u8=vqtbl2_u8
LDFLAGS = -L$(SDLPREFIX)/lib -Wl,-rpath,$(SDLPREFIX)/lib
LIBS = -lSDL2 -lm -framework CoreMIDI -framework CoreFoundation
EXE = $(BUILDDIR)/groovegoat
endif

# Audio/video codecs: LAME (MP3, LGPL), faac (AAC, LGPL)
LAMESRC = $(notdir $(wildcard $(SRCDIR)/codecs/lame/*.c))
FAACSRC = $(notdir $(wildcard $(SRCDIR)/codecs/faac/*.c))
CODECOBJS = $(addprefix $(BUILDDIR)/codecs/lame/,$(LAMESRC:.c=.o)) $(addprefix $(BUILDDIR)/codecs/faac/,$(FAACSRC:.c=.o)) \
       $(BUILDDIR)/codecs/h264impl.o $(BUILDDIR)/codecs/mp4impl.o

OBJS = $(addprefix $(BUILDDIR)/,$(CSRC:.c=.o) $(CXXSRC:.cpp=.o) $(RESIDSRC:.cpp=.o) $(ASMSRC:.c=.o)) $(CODECOBJS)

all: $(EXE)

$(SRCDIR)/gres.c: tools/mkres.py tools/mklogo.py $(SRCDIR)/player.s $(SRCDIR)/altplayer.s $(SRCDIR)/mplayer.s $(SRCDIR)/frontend.s docs/manual.txt docs/history.txt docs/credits.txt
	python3 tools/mkres.py

$(BUILDDIR)/codecs/lame/%.o: $(SRCDIR)/codecs/lame/%.c $(SRCDIR)/codecs/lame/config.h
	@mkdir -p $(dir $@)
	$(CC) -I$(SRCDIR)/codecs/lame $(COMMONFLAGS) -w -DHAVE_CONFIG_H -c -o $@ $<

$(BUILDDIR)/codecs/faac/%.o: $(SRCDIR)/codecs/faac/%.c $(SRCDIR)/codecs/faac/faac_config.h
	@mkdir -p $(dir $@)
	$(CC) -I$(SRCDIR)/codecs/faac $(COMMONFLAGS) -w -include $(SRCDIR)/codecs/faac/faac_config.h -c -o $@ $<

# minih264 (H.264) and minimp4 (MP4 muxer), both CC0, single-header libraries
$(BUILDDIR)/codecs/%impl.o: $(SRCDIR)/codecs/%impl.c $(SRCDIR)/codecs/minih264/minih264e.h $(SRCDIR)/codecs/minimp4/minimp4.h
	@mkdir -p $(dir $@)
	$(CC) $(COMMONFLAGS) -w -c -o $@ $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(wildcard $(SRCDIR)/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(COMMONFLAGS) $(CFLAGS_EXTRA) -c -o $@ $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(wildcard $(SRCDIR)/*.h)
	@mkdir -p $(dir $@)
	$(CXX) $(COMMONFLAGS) $(CXXFLAGS_EXTRA) -c -o $@ $<

$(BUILDDIR)/groovegoat_res.o: res/groovegoat.rc res/groovegoat.ico
	@mkdir -p $(dir $@)
	$(WINDRES) -I res -o $@ res/groovegoat.rc

ifneq ($(filter mac macintel,$(TARGET)),)
$(EXE): $(OBJS) $(BUILDDIR)/macstub.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
$(BUILDDIR)/macstub.o: $(SRCDIR)/macstub/cxxrt.c
	@mkdir -p $(dir $@)
	$(CC) $(COMMONFLAGS) -c -o $@ $<
else
$(EXE): $(OBJS) $(WINRES)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)
	@if [ "$(TARGET)" = macnative ]; then ln -sfn ../../instruments $(BUILDDIR)/instruments; ln -sfn ../../examples $(BUILDDIR)/examples; fi
endif

clean:
	rm -rf build

.PHONY: all clean
