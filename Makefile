# SwitchBox Lite - Nintendo Switch homebrew scaffold.
# Build in a devkitPro/MSYS2 shell with switch-dev installed: make

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. Install devkitPro switch-dev first.")
endif

include $(DEVKITPRO)/libnx/switch_rules

TARGET      := switchbox
BUILD       := build
SOURCES     := source
INCLUDES    := include
DATA        := data
ROMFS       := romfs

APP_TITLE   := SwitchBox Lite
APP_AUTHOR  := SwitchBox contributors
APP_VERSION := 0.1.0

ARCH        := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS      := -g -Wall -O2 -ffunction-sections $(ARCH) $(DEFINES)
CFLAGS      += $(INCLUDE)
CXXFLAGS    := $(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions
ASFLAGS     := -g $(ARCH)
LDFLAGS     = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS        := -lnx

# Optional network support for remote TVBox configs/APIs and M3U playlists.
# Usage: make USE_CURL=1
# Requires a Switch libcurl portlib and its transitive TLS/zlib dependencies.
ifeq ($(USE_CURL),1)
CFLAGS      += -DSWITCHBOX_USE_CURL
CXXFLAGS    += -DSWITCHBOX_USE_CURL
LIBS        := -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz $(LIBS)
endif

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                 $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES    := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SRC)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(SOURCES),-I$(CURDIR)/$(dir)) \
                  -I$(CURDIR)/$(BUILD)

.PHONY: all clean run

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).nacp

run: all
	@echo "Copy $(TARGET).nro to /switch/switchbox/ on your SD card."

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).nro: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
