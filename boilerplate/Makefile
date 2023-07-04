TOPDIR := $(realpath ..)

DELTA := 0
VQ := 0
KLANG := 0

MODULE := JazzCat-DerKlang

RLE2PNG = rle2png.py

OBJECTS := data/$(MODULE).trk.o

ifeq ($(DELTA), 1)
OBJECTS += data/$(MODULE)-Delta.smp.o
else ifeq ($(VQ), 1)
OBJECTS += data/$(MODULE)-VQ.smp.o
else ifeq ($(KLANG), 1)
OBJECTS += data/$(MODULE).o 
else
OBJECTS += data/$(MODULE).smp.o
endif

LIBS := libpt

BUILD-FILES := audiovq

CLEAN-FILES := main.o \
	data/$(MODULE)*.o data/$(MODULE).{smp,trk}* \
	data/ghostown-logo-crop.c \
	data/ghostown-logo-0[1-3].c \
	data/pal-gold*.c \

SOURCES := main.c logo.c
MAIN := #

PNG2C.ghostown-logo-crop := --pixmap ghostown_logo,192x131x4,+onlydata,+displayable
PNG2C.ghostown-logo-01 := --palette ghostown_logo_1_pal,8
PNG2C.ghostown-logo-02 := --palette ghostown_logo_2_pal,8
PNG2C.ghostown-logo-03 := --palette ghostown_logo_3_pal,8

PNG2C.pal-gold := --palette pal_gold,16,+shared
PNG2C.pal-gold-dark := --palette pal_gold_dark,16,+shared
PNG2C.pal-gold-light := --palette pal_gold_light,16,+shared

CPPFLAGS += -DINTRO -DVQ=$(VQ) -DDELTA=$(DELTA) -DKLANG=$(KLANG) -I.

include $(TOPDIR)/build/effect.mk

data/%.trk data/%.smp: data/%.mod
	$(PYTHON3) data/ptsplit.py $^
	mv data/$*.mod.trk data/$*.trk
	mv data/$*.mod.smp data/$*.smp

OBJCOPY-CHIP := --rename-section .data=.datachip,alloc,load,data,contents

data/%.trk.o: data/%.trk
	@echo "[OBJCOPY] $(DIR)$< -> $(DIR)$@"
	$(OBJCOPY) -I binary -O amiga \
	  --redefine-sym _binary_data_$(subst -,_,$*)_trk_start=_Module \
	  --redefine-sym _binary_data_$(subst -,_,$*)_trk_end=_ModuleEnd \
	  --redefine-sym _binary_data_$(subst -,_,$*)_trk_size=_ModuleSize \
	  $^ $@

data/%-Delta.smp: data/%.smp
	@echo "[DELTA] $(DIR)$< -> $(DIR)$@"
	$(PYTHON3) delta.py $< $@

data/%-VQ.smp: data/%.smp audiovq
	@echo "[AUDIOVQ] $(DIR)$< -> $(DIR)$@"
	./audiovq $< $@

data/%.smp.o: data/%.smp
	@echo "[OBJCOPY] $(DIR)$< -> $(DIR)$@"
	$(OBJCOPY) $(OBJCOPY-CHIP) -I binary -O amiga \
	  --redefine-sym _binary_data_$(subst -,_,$*)_smp_start=_Samples \
	  --redefine-sym _binary_data_$(subst -,_,$*)_smp_end=_SamplesEnd \
	  --redefine-sym _binary_data_$(subst -,_,$*)_smp_size=_SamplesSize \
	  $^ $@

data/%.png: data/%.rle
	@echo "[RLE] $(DIR)$< -> $(DIR)$@"
	$(PYTHON3) $(RLE2PNG) $< $@ || (rm -f $@ && exit 1)

REPOHEAD := $(shell git rev-parse --short HEAD)

%.exe.packed: %.exe
	@echo "[PACK] $(DIR)$< -> $(DIR)$@"
	Shrinkler -o -f 0xdff180 \
	  -t "Electric Lifeforms by Ghostown (build: $(REPOHEAD))" $< $@

audiovq: audiovq.c
	@echo "[HOSTCC] $(DIR)$< -> $(DIR)$@"
	cc -O2 -march=native -Wall -o $@ $^ -lm
