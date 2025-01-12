TOPDIR = $(realpath .)

SUBDIRS = tools lib system boilerplate
EXTRA-FILES = tags cscope.out
CLEAN-FILES = bootloader.bin a500rom.bin addchip.bootblock.bin vbrmove

all: a500rom.bin bootloader.bin addchip.bootblock.bin vbrmove build

addchip.bootblock.bin: VASMFLAGS += -phxass -cnop=0

include $(TOPDIR)/build/common.mk

FILES := $(shell find include lib system -type f -iname '*.[ch]')

tags:
	ctags -R $(FILES)

cscope.out:
	cscope -b $(FILES)

gdb-dashboard:
	wget -O $@ https://raw.githubusercontent.com/cyrus-and/gdb-dashboard/master/.gdbinit
