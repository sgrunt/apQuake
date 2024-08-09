# GNU Makefile for vkQuake unix x86_64 targets.
# You need the SDL2 library fully installed.
# "make DEBUG=1" to build a debug client.
# "make SDL_CONFIG=/path/to/sdl2-config" for unusual SDL2 installations.
# "make DO_USERDIRS=1" to enable user directories support
# "make VULKAN_SDK=/path/to/sdk" if it is not already in path

# Enable/Disable user directories support
DO_USERDIRS = 0

### Enable/Disable codecs for streaming music support
USE_CODEC_WAVE = 1
USE_CODEC_FLAC = 0
USE_CODEC_MP3 = 1
USE_CODEC_VORBIS = 1
USE_CODEC_OPUS = 0
# either xmp or mikmod (or modplug)
USE_CODEC_MIKMOD = 0
USE_CODEC_XMP = 0
USE_CODEC_MODPLUG = 0
USE_CODEC_UMX = 0

# which library to use for mp3 decoding: mad or mpg123
MP3LIB = mad
# which library to use for ogg decoding: vorbis or tremor
VORBISLIB = vorbis

HOST_OS := $(shell uname|sed -e s/_.*//|tr '[:upper:]' '[:lower:]')

# ---------------------------
# build variables
# ---------------------------
CPUFLAGS =
LDFLAGS ?=
DFLAGS ?=
CFLAGS ?=
CXXFLAGS ?=

ifeq ($(DO_USERDIRS),1)
CFLAGS += -DDO_USERDIRS=1
CXXFLAGS += -DDO_USERDIRS=1
endif

# ---------------------------
# objects
# ---------------------------
SYSOBJ_NET := net_bsd.o net_udp.o
SYSOBJ_SYS := pl_linux.o sys_sdl.o sys_sdl_unix.o
DEFAULT_TARGET := apquake
BINTOC_EXE = ../Shaders/bintoc

# ---------------------------
# common
# ---------------------------
include common.make

# ---------------------------
# libraries
# ---------------------------
SDL_CONFIG ?= sdl2-config
SDL_CFLAGS := $(shell $(SDL_CONFIG) --cflags)
SDL_LIBS := $(shell $(SDL_CONFIG) --libs)

PKG_CONFIG ?= pkg-config
JSON_CFLAGS := $(shell $(PKG_CONFIG) jsoncpp --cflags)
JSON_LIBS := $(shell $(PKG_CONFIG) jsoncpp --libs)

APCPP_CFLAGS :=
APCPP_LIBS := -lAPCpp

CFLAGS += -Imimalloc
CXXFLAGS += -Imimalloc

ifneq ($(VULKAN_SDK),)
LDFLAGS += -L$(VULKAN_SDK)/lib
CFLAGS += -I$(VULKAN_SDK)/include
CXXFLAGS += -I$(VULKAN_SDK)/include
endif

ifeq ($(USE_CODEC_OPUS),1)
# opus and opusfile put their *.h under <includedir>/opus,
# but they include the headers without the opus directory
# prefix and rely on pkg-config. ewww...
CFLAGS += $(shell pkg-config --cflags opusfile)
CXXFLAGS += $(shell pkg-config --cflags opusfile)
CODECLIBS += $(shell pkg-config --libs opusfile)
endif

COMMON_LIBS:= -lm -lpthread

ifneq ($(HOST_OS),darwin)
COMMON_LIBS+= -lvulkan
CFLAGS+= -DLINUX
else
ifeq ($(shell test -d /opt/homebrew && echo true),true)
COMMON_LIBS+= -L/opt/homebrew/lib
CFLAGS+= -I/opt/homebrew/include
endif
COMMON_LIBS+= -lMoltenVK
endif

LIBS := $(COMMON_LIBS) $(CODECLIBS)

# ---------------------------
# targets / rules
# ---------------------------
all: $(DEFAULT_TARGET)

apquake: $(SHADER_OBJS) $(OBJS)
	$(LINKER) $(SHADER_OBJS) $(OBJS) $(LDFLAGS) $(LIBS) $(SDL_LIBS) $(JSON_LIBS) $(APCPP_LIBS) -o $@
	$(call DO_STRIP,$@)

release: apquake
debug:
	$(error Use "make DEBUG=1")

clean:
	$(RM) *.o *.d $(DEFAULT_TARGET) $(BINTOC_EXE) \
	../Shaders/Compiled/$(GLSLANG_OUT_FOLDER)/*.c \
	../Shaders/Compiled/$(GLSLANG_OUT_FOLDER)/*.spv \
	../Shaders/Compiled/$(GLSLANG_OUT_FOLDER)/*.d

prefix ?= /usr
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/games
sbindir ?= $(exec_prefix)/sbin
INSTALL ?= install
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?= ${INSTALL} -m 644
install: apquake
	$(INSTALL_PROGRAM) $(CURDIR)/apquake $(DESTDIR)$(bindir)/apquake
