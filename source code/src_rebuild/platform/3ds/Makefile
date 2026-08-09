#---------------------------------------------------------------------------------
# REDRIVER2 3DS homebrew scaffold
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set. Please install devkitPro/devkitARM)
endif

ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set. Please install devkitARM)
endif

include $(DEVKITARM)/3ds_rules

TARGET      := redriver2_3ds
BUILD       := build
ROOT        := ../..
GAME_REGION ?= NTSC_VERSION
SMOKE_TEST  ?= 0
QUICK_DRIVE ?= 0
SKIP_INTRO  ?= 1
RENDER_SCALE ?= 3
DRAW_DISTANCE ?= 34
SPOOL_BOUNDARY ?= 7
TEXTURE_DETAIL ?= 1
FOG ?= 1
FOG_TOP_ALPHA ?= 156
FOG_R ?= 96
FOG_G ?= 102
FOG_B ?= 108
CAR_LOD_DISTANCE ?= 2600

APP_TITLE       ?= REDRIVER2 3DS
APP_DESCRIPTION ?= Driver 2 3DS port
APP_AUTHOR      ?= OpenDriver2
APP_ICON        ?= $(CTRULIB)/default_icon.png
CIA_RSF         ?= packaging/$(TARGET).rsf

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

DEFINES := \
  -D__3DS__ \
  -D$(GAME_REGION) \
  -DNDEBUG \
  -DBUILD_CONFIGURATION_STRING=\"3DS\" \
  -DGAME_VERSION_N=\"8.0-3ds\" \
  -DREDRIVER2_3DS_SMOKE_TEST=$(SMOKE_TEST) \
  -DREDRIVER2_3DS_QUICK_DRIVE=$(QUICK_DRIVE) \
  -DREDRIVER2_3DS_SKIP_INTRO=$(SKIP_INTRO) \
  -DREDRIVER2_3DS_RENDER_SCALE=$(RENDER_SCALE) \
  -DREDRIVER2_3DS_DRAW_DISTANCE=$(DRAW_DISTANCE) \
  -DREDRIVER2_3DS_SPOOL_BOUNDARY=$(SPOOL_BOUNDARY) \
  -DREDRIVER2_3DS_TEXTURE_DETAIL=$(TEXTURE_DETAIL) \
  -DREDRIVER2_3DS_FOG=$(FOG) \
  -DREDRIVER2_3DS_FOG_TOP_ALPHA=$(FOG_TOP_ALPHA) \
  -DREDRIVER2_3DS_FOG_R=$(FOG_R) \
  -DREDRIVER2_3DS_FOG_G=$(FOG_G) \
  -DREDRIVER2_3DS_FOG_B=$(FOG_B) \
  -DREDRIVER2_3DS_CAR_LOD_DISTANCE=$(CAR_LOD_DISTANCE) \
  -DREDRIVER2_3DS_FLIP_Y=1 \
  -DREDRIVER2_3DS_FAST_BLEND=1 \
  -DUSE_CD_FILESYSTEM=0 \
  -DUSE_EXTENDED_PRIM_POINTERS=1 \
  -DUSE_PGXP=0

INCLUDES := \
  -I$(CURDIR) \
  -I$(ROOT) \
  -I$(ROOT)/Game \
  -I$(ROOT)/PsyCross/include \
  -I$(ROOT)/PsyCross/include/psx \
  -I$(ROOT)/PsyCross/src \
  -I$(ROOT)/utils \
  -I$(CTRULIB)/include \
  -I$(CURDIR)/shims

CFLAGS   := -g -Wall -Wno-narrowing -O3 -fomit-frame-pointer -mword-relocations -ffunction-sections -fsigned-char $(ARCH) $(DEFINES) $(INCLUDES)
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11
ASFLAGS  := -g $(ARCH)
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections
LIBS     := -lctru -lm
LIBDIRS  := $(CTRULIB)
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

APP_CPP := \
  redriver2_3ds.cpp \
  psyx_3ds_core.cpp \
  psyx_3ds_render_stub.cpp \
  psyx_3ds_spual_stub.cpp \
  psyx_3ds_pad.cpp \
  psyx_3ds_fs.cpp \
  pc_debug_stubs.cpp \
  fmv_stub.cpp

GAME_C   := $(call rwildcard,$(ROOT)/Game/,*.c)
GAME_CPP := $(call rwildcard,$(ROOT)/Game/,*.cpp)

PSYX_C := \
  $(ROOT)/PsyCross/src/gpu/PsyX_GPU.cpp \
  $(ROOT)/PsyCross/src/gte/half_float.cpp \
  $(ROOT)/PsyCross/src/gte/PsyX_GTE.cpp \
  $(ROOT)/PsyCross/src/psx/ABS.C \
  $(ROOT)/PsyCross/src/psx/INLINE_C.C \
  $(ROOT)/PsyCross/src/psx/LIBAPI.C \
  $(ROOT)/PsyCross/src/psx/LIBCD.C \
  $(ROOT)/PsyCross/src/psx/LIBETC.C \
  $(ROOT)/PsyCross/src/psx/LIBGPU.C \
  $(ROOT)/PsyCross/src/psx/LIBGTE.C \
  $(ROOT)/PsyCross/src/psx/LIBMCRD.C \
  $(ROOT)/PsyCross/src/psx/LIBPAD.C \
  $(ROOT)/PsyCross/src/psx/LIBSN.C \
  $(ROOT)/PsyCross/src/psx/LIBSPU.C

UTIL_CPP := \
  $(ROOT)/utils/audio_source/snd_wav_cache.cpp \
  $(ROOT)/utils/audio_source/snd_wav_source.cpp \
  $(ROOT)/utils/audio_source/snd_al_source.cpp \
  $(ROOT)/utils/riff.cpp \
  $(ROOT)/utils/targa.cpp

SOURCES := $(APP_CPP) $(GAME_C) $(GAME_CPP) $(PSYX_C) $(UTIL_CPP)
OFILES  := $(addprefix $(BUILD)/,$(addsuffix .o,$(basename $(notdir $(SOURCES)))))
VPATH   := $(sort $(dir $(SOURCES)))
DEPENDS := $(OFILES:.o=.d)

.PHONY: all clean rebuild cia

all: $(TARGET).3dsx

cia: $(TARGET).cia

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CXX) $(CXXFLAGS) -x c++ -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD)/%.o: %.C | $(BUILD)
	$(CXX) $(CXXFLAGS) -x c++ -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(AS) $(ASFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
	$(CXX) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@

$(TARGET).smdh: $(APP_ICON)
	smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" "$(APP_ICON)" $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	3dsxtool $< $@ --smdh=$(TARGET).smdh

$(TARGET).cia: $(TARGET).elf $(TARGET).smdh $(CIA_RSF)
	makerom -f cia -o $@ -target t -elf $< -rsf $(CIA_RSF) -desc app:4 -icon $(TARGET).smdh -major 0 -minor 1 -micro 0

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).cia $(TARGET).smdh

rebuild: clean all

-include $(DEPENDS)
