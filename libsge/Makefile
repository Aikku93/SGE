#------------------------------------------------#
export RELEASEDIR := $(CURDIR)/release
#------------------------------------------------#

PLATFORMS := gba nds7 nds9

#------------------------------------------------#

all : $(PLATFORMS)

$(RELEASEDIR):; @mkdir -p $(RELEASEDIR)

#------------------------------------------------#

.phony : gba

gba: | $(RELEASEDIR)
	$(MAKE) -f gba.mak

#------------------------------------------------#

.phony : nds7

nds7: | $(RELEASEDIR)
	$(MAKE) -f nds.mak ARMTARGET=7

#------------------------------------------------#

.phony : nds9

nds9: | $(RELEASEDIR)
	$(MAKE) -f nds.mak ARMTARGET=9

#------------------------------------------------#

.phony : clean

clean:
	$(MAKE) clean -f gba.mak
	$(MAKE) clean -f nds.mak ARMTARGET=7
	$(MAKE) clean -f nds.mak ARMTARGET=9
	rm -rf $(RELEASEDIR)

#------------------------------------------------#
