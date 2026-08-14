# WiiLink Patcher Wii - devkitPro/libogc build
.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC (source /etc/profile.d/devkit-env.sh)")
endif

include $(DEVKITPPC)/wii_rules

TARGET      := wiilink-patcher-wii
BUILD       := build
SOURCES     := source
DATA        :=
INCLUDES    := source

CFLAGS      := -g -O2 -Wall -Wextra -Wshadow -Wformat=2 $(MACHDEP) $(INCLUDE)
CXXFLAGS    := $(CFLAGS)
LDFLAGS      = -g $(MACHDEP) -Wl,-Map,$(OUTPUT).elf.map
LIBS        := -lbz2 -lfat -lwiiuse -lbte -logc -lm
LIBDIRS     := $(PORTLIBS)

ifeq ($(DEBUG),1)
CFLAGS      += -DDEBUG_SCREEN_DEFAULT=1 -DDEBUG_BUILD=1
endif

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT     := $(CURDIR)/$(TARGET)
export VPATH      := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR    := $(CURDIR)/$(BUILD)

CFILES            := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES            := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES_UPPER      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))

ifeq ($(strip $(CPPFILES)),)
export LD         := $(CC)
else
export LD         := $(CXX)
endif

export OFILES     := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o) $(SFILES_UPPER:.S=.o)
export INCLUDE    := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                     $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                     -I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS   := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(LIBOGC_LIB)

.PHONY: all clean package debug
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).dol $(OUTPUT).elf.map package

package: all
	@rm -rf package
	@mkdir -p package/wiilink-patcher-wii
	@cp $(TARGET).dol package/wiilink-patcher-wii/boot.dol
	@cp wiilink-patcher/meta.xml wiilink-patcher/icon.png package/wiilink-patcher-wii/
	@rm -f wiilink-patcher-wii-$(shell grep APP_VERSION source/config.h | cut -d'"' -f2).zip
	@cd package && zip -9 -r ../wiilink-patcher-wii-$(shell grep APP_VERSION source/config.h | cut -d'"' -f2).zip . >/dev/null
	@echo "Created minimal wiilink-patcher-wii package"

debug:
	@$(MAKE) clean
	@$(MAKE) DEBUG=1 all
	@cp $(TARGET).dol $(TARGET)-debug.dol

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
