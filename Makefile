#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# ROMFS is the directory containing data to be added to RomFS, relative to the Makefile (Optional)
#
# NO_ICON: if set to anything, do not use icon.
# NO_NACP: if set to anything, no .nacp file is generated.
# APP_TITLE is the name of the app stored in the .nacp file (Optional)
# APP_AUTHOR is the author of the app stored in the .nacp file (Optional)
# APP_VERSION is the version of the app stored in the .nacp file (Optional)
# APP_TITLEID is the titleID of the app stored in the .nacp file (Optional)
# ICON is the filename of the icon (.jpg), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.jpg
#     - icon.jpg
#     - <libnx folder>/default_icon.jpg
#
# CONFIG_JSON is the filename of the NPDM config file (.json), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.json
#     - config.json
#   If a JSON file is provided or autodetected, an ExeFS PFS0 (.nsp) is built instead
#   of a homebrew executable (.nro). This is intended to be used for sysmodules.
#   NACP building is skipped as well.
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
APP_AUTHOR	:=	minbbb
BUILD_NRO	:=	build_nro
ifeq ($(strip $(MAKECMDGOALS)),nro)
BUILD		:=	$(BUILD_NRO)
else
BUILD		:=	build
endif
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include
#ROMFS	:=	romfs

SYSMODULE ?= 1

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE) -D__SWITCH__

ifeq ($(strip $(SYSMODULE)),1)
CFLAGS	+=	-DSYSMODULE
endif

ifdef ENABLE_LOGGING
CFLAGS	+=	-DENABLE_LOGGING
endif

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lnx

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(LIBNX)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(strip $(filter-out $(BUILD) $(BUILD_NRO),$(notdir $(CURDIR)))),)
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(CONFIG_JSON)),)
	jsons := $(wildcard *.json)
	ifneq (,$(findstring $(TARGET).json,$(jsons)))
		export APP_JSON := $(TOPDIR)/$(TARGET).json
	else
		ifneq (,$(findstring config.json,$(jsons)))
			export APP_JSON := $(TOPDIR)/config.json
		endif
	endif
else
	export APP_JSON := $(TOPDIR)/$(CONFIG_JSON)
endif

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.jpg)
	ifneq (,$(findstring $(TARGET).jpg,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).jpg
	else
		ifneq (,$(findstring icon.jpg,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.jpg
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_ICON)),)
	export NROFLAGS += --icon=$(APP_ICON)
endif

ifeq ($(strip $(NO_NACP)),)
	export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
endif

ifneq ($(APP_TITLEID),)
	export NACPFLAGS += --titleid=$(APP_TITLEID)
endif

ifneq ($(ROMFS),)
	export NROFLAGS += --romfsdir=$(CURDIR)/$(ROMFS)
endif

.PHONY: all sysmodule nro clean $(BUILD)

#---------------------------------------------------------------------------------
all: sysmodule

# Web assets embedded verbatim as C string literals at build time (see gen_header).
WEB_ASSETS_H := \
	$(BUILD)/index_html.h \
	$(BUILD)/styles_css.h \
	$(BUILD)/app_js.h \
	$(BUILD)/state_js.h \
	$(BUILD)/utils_js.h \
	$(BUILD)/ui_js.h \
	$(BUILD)/viewer_js.h \
	$(BUILD)/events_js.h

sysmodule: $(BUILD) $(WEB_ASSETS_H)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile \
		APP_JSON=$(CURDIR)/$(TARGET).json SYSMODULE=1

nro: $(BUILD) $(WEB_ASSETS_H)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile \
		APP_JSON= SYSMODULE=

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

# gen_header: sed escapes \ and " per line and preserves line breaks as \n, so any
# valid css/js/html is safe. $(1) is the C identifier (e.g. styles_css, app_js).
define gen_header
	@echo '#ifndef $(1)_H' > $@
	@echo '#define $(1)_H' >> $@
	@echo 'static const char $(1)[] =' >> $@
	@sed 's/\\/\\\\/g; s/"/\\"/g; s/.*/"&\\n"/' $< >> $@
	@echo ';' >> $@
	@echo '#endif' >> $@
endef

$(BUILD)/index_html.h: web/index.html | $(BUILD)
	@$(call gen_header,index_html)

$(BUILD)/styles_css.h: web/css/styles.css | $(BUILD)
	@$(call gen_header,styles_css)

$(BUILD)/app_js.h: web/js/app.js | $(BUILD)
	@$(call gen_header,app_js)

$(BUILD)/state_js.h: web/js/state.js | $(BUILD)
	@$(call gen_header,state_js)

$(BUILD)/utils_js.h: web/js/utils.js | $(BUILD)
	@$(call gen_header,utils_js)

$(BUILD)/ui_js.h: web/js/ui.js | $(BUILD)
	@$(call gen_header,ui_js)

$(BUILD)/viewer_js.h: web/js/viewer.js | $(BUILD)
	@$(call gen_header,viewer_js)

$(BUILD)/events_js.h: web/js/events.js | $(BUILD)
	@$(call gen_header,events_js)

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(BUILD_NRO) $(TARGET).nro $(TARGET).nacp $(TARGET).elf \
		$(TARGET).nsp $(TARGET).nso $(TARGET).npdm $(TARGET).lst $(TARGET).map \
		$(TARGET)-sysmodule.elf $(TARGET)-nro.elf


#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

# Each build mode links its own ELF (`minFS-sysmodule.elf` for the sysmodule,
# `minFS-nro.elf` for the dev NRO) at the repo root. A shared `minFS.elf`
# written by both modes could let a final get linked from the wrong object dir
# when switching modes without a clean; per-mode intermediates rule that out
# while keeping incremental builds within a mode working.
MODE_ELF	:=	$(OUTPUT)-$(if $(SYSMODULE),sysmodule,nro).elf

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
ifeq ($(strip $(APP_JSON)),)

all	:	$(OUTPUT).nro

# switch_rules defines `%.nro: %.elf` with `elf2nro $< $@`. When an explicit
# rule only lists prerequisites and no recipe, GNU make still applies that
# pattern rule and prepends its `%.elf` prerequisite (the shared `minFS.elf`)
# to the goal, so `$<` resolves to `minFS.elf` rather than $(MODE_ELF) - and
# that shared intermediate then gets re-linked from $(OFILES) and auto-deleted
# on every build, defeating the per-mode-ELF split below. Giving the goal its
# own recipe (kept in sync with switch_rules) overrides the pattern rule for
# this exact target, so the conversion input is always the current mode's own
# ELF and no shared intermediate is created or deleted.
$(OUTPUT).nro	:	$(MODE_ELF)
	@elf2nro $< $@ $(NROFLAGS)
	@echo built ... $(notdir $@)

ifeq ($(strip $(NO_NACP)),)
$(OUTPUT).nro	:	$(OUTPUT).nacp
endif

else

all	:	$(OUTPUT).nsp

$(OUTPUT).nsp	:	$(OUTPUT).nso $(OUTPUT).npdm

# Same override rationale as the `.nro` rule above: without an explicit recipe
# this target would fall under switch_rules' `%.nso: %.elf` pattern and convert
# the shared `minFS.elf` instead of $(MODE_ELF).
$(OUTPUT).nso	:	$(MODE_ELF)
	@elf2nso $< $@
	@echo built ... $(notdir $@)

endif

$(MODE_ELF)	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
