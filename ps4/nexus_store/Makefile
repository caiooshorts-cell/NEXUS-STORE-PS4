# NEXUS STORE - native PS4 homebrew application
TITLE       := NEXUS STORE
VERSION     := 0.10
TITLE_ID    := NXUS00001
CONTENT_ID  := IV0000-NXUS00001_00-NEXUSSTORE000000

# OpenOrbis + SDL2 (software framebuffer and DualShock 4 input).
LIBS        := -lc -lkernel -lc++ -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad -lSceSysmodule -lSceFreeType -lSDL2

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := nexus_store
INTDIR      := $(PROJDIR)/x64/Debug
CXXFILES    := $(wildcard $(PROJDIR)/*.cpp)
OBJS        := $(patsubst $(PROJDIR)/%.cpp,$(INTDIR)/%.o,$(CXXFILES))
LIBMODULES  := $(wildcard sce_module/*)

CXXFLAGS    := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

CCX         := clang++
LD          := ld.lld
CDIR        := linux
PKGTOOL     := $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core
CREATE_FSELF:= $(TOOLCHAIN)/bin/$(CDIR)/create-fself
CREATE_GP4  := $(TOOLCHAIN)/bin/$(CDIR)/create-gp4

.PHONY: all clean
all: $(CONTENT_ID).pkg

$(INTDIR):
	mkdir -p $(INTDIR)

$(INTDIR)/%.o: $(PROJDIR)/%.cpp | $(INTDIR)
	$(CCX) $(CXXFLAGS) -o $@ $<

eboot.bin: $(OBJS)
	$(LD) $(INTDIR)/*.o -o $(INTDIR)/$(PROJDIR).elf $(LDFLAGS)
	$(CREATE_FSELF) -in=$(INTDIR)/$(PROJDIR).elf -out=$(INTDIR)/$(PROJDIR).oelf --eboot "eboot.bin" --paid 0x3800000000000011

sce_sys/param.sfo: Makefile
	mkdir -p sce_sys
	$(PKGTOOL) sfo_new $@
	$(PKGTOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKGTOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKGTOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKGTOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKGTOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKGTOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKGTOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES)
	$(CREATE_GP4) -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(CONTENT_ID).pkg: pkg.gp4
	$(PKGTOOL) pkg_build $< .

clean:
	rm -f $(CONTENT_ID).pkg pkg.gp4 sce_sys/param.sfo eboot.bin
	rm -f $(INTDIR)/$(PROJDIR).elf $(INTDIR)/$(PROJDIR).oelf $(OBJS)
