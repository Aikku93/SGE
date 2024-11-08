.phony: clean

#------------------------------------------------#
# Directories
#------------------------------------------------#

LIBFOURIERDIR  := libfourier
LIBFOURIERFILE := $(LIBFOURIERDIR)/release/libfourier.a

# Note: The common library .a file is placed in $(OBJDIR)
OBJDIR := build
RELDIR := release
LIBDIR := $(LIBFOURIERDIR)/release $(OBJDIR)
INCDIR := include ../libsge/include $(LIBFOURIERDIR)/include

LIBS           := fourier sgeCommon
TARGETS        := $(RELDIR)/sgetool
#COMMON_SRCDIR  := libsge libsge/mml libsge/midi libtools
COMMON_SRCDIR  := libsge libsge/mml libtools
SGETOOL_SRCDIR := src_sgetool

#------------------------------------------------#
# Cross-compilation, compile flags
#------------------------------------------------#

ifeq ($(strip $(ARCHCROSS)),)
	ARCHCROSS :=
endif
ifeq ($(strip $(ARCHFLAGS)),)
	ARCHFLAGS :=
endif

CCFLAGS  := $(ARCHFLAGS) -O2 -Wall -Wextra -ffunction-sections -fdata-sections $(foreach dir, $(INCDIR), -I$(dir))
CXXFLAGS := $(CCFLAGS)
LDFLAGS  := $(ARCHFLAGS) -static -s -Wl,--gc-sections $(foreach dir, $(LIBDIR), -L$(dir)) $(foreach lib, $(LIBS), -l$(lib))

#------------------------------------------------#
# Tools
#------------------------------------------------#

CC  := $(ARCHCROSS)gcc
CXX := $(ARCHCROSS)g++
LD  := $(ARCHCROSS)g++
AR  := $(ARCHCROSS)ar

#------------------------------------------------#
# Source files
#------------------------------------------------#

COMMON_TARGET   := $(OBJDIR)/libsgeCommon.a
COMMON_CFILES   := $(foreach dir, $(COMMON_SRCDIR), $(wildcard $(dir)/*.c))
COMMON_CXXFILES := $(foreach dir, $(COMMON_SRCDIR), $(wildcard $(dir)/*.cpp))
COMMON_OFILES   := $(addsuffix .o, $(addprefix $(OBJDIR)/, $(COMMON_CFILES) $(COMMON_CXXFILES)))

SGETOOL_CFILES   := $(wildcard $(SGETOOL_SRCDIR)/*.c)
SGETOOL_CXXFILES := $(wildcard $(SGETOOL_SRCDIR)/*.cpp)
SGETOOL_OFILES   := $(addsuffix .o, $(addprefix $(OBJDIR)/, $(SGETOOL_CFILES) $(SGETOOL_CXXFILES)))

DFILES := $(COMMON_OFILES:.o=.d) $(SGETOOL_OFILES:.o=.d)

#------------------------------------------------#
# make all
#------------------------------------------------#

all : $(TARGETS)

#------------------------------------------------#
# Common library
#------------------------------------------------#

$(COMMON_TARGET) : $(COMMON_OFILES)
	@echo Building common library...
	@mkdir -p $(dir $@)
	@$(AR) rcs $@ $^

#------------------------------------------------#
# sgetool
#------------------------------------------------#

$(RELDIR)/sgetool : $(SGETOOL_OFILES) $(COMMON_TARGET) $(LIBFOURIERFILE)
	@echo Building $@...
	@mkdir -p $(dir $@)
	@$(LD) $(LDFLAGS) -o $@ $^

#------------------------------------------------#
# libfourier
#------------------------------------------------#

$(LIBFOURIERFILE):
	$(MAKE) -C libfourier

#------------------------------------------------#
# Generic rules
#------------------------------------------------#

$(OBJDIR)/%.c.o : %.c
	@echo $<
	@mkdir -p $(dir $@)
	@$(CC) $(CCFLAGS) -MD -MP -MF $(OBJDIR)/$<.d -c -o $@ $<

$(OBJDIR)/%.cpp.o : %.cpp
	@echo $<
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -MD -MP -MF $(OBJDIR)/$<.d -c -o $@ $<

#------------------------------------------------#
# Dependencies
#------------------------------------------------#

-include $(DFILES)

#------------------------------------------------#
# make clean
#------------------------------------------------#

clean :
	@rm -rf $(OBJDIR) $(RELDIR)
	@$(MAKE) -C libfourier clean

#------------------------------------------------#
