pkgconfig	:=pkg-config
prefix		:=/usr/local
bindir		:=$(prefix)/bin
sbindir		:=$(prefix)/sbin
crashdir	:=/var/crash
udevrulesdir	:=/etc/udev/rules.d/
unpackdir	:=/tmp
etnaviv_dir	:=/shared/etna_viv
etnaviv_inc	:=$(etnaviv_dir)/src/etnaviv
libdrm_cflags	:=$(shell $(pkgconfig) --cflags libdrm)
libdrm_ldflags	:=$(shell $(pkgconfig) --libs libdrm)

CPPFLAGS	:=-D_GNU_SOURCE -D_LARGEFILE64_SOURCE -Iinclude
CFLAGS_COMMON	:=-O2 -Wall -std=c99
CFLAGS		=$(CFLAGS_COMMON) $(CFLAGS_$(notdir $@))
LDLIBS		=$(LDLIBS_$(notdir $@))
SED		:=sed
SEDARGS		:=s|@sbindir@|$(sbindir)|g;s|@crashdir@|$(crashdir)|g;s|@unpackdir@|$(unpackdir)|g
BINPROGS	:=bin2img detile/viv-demultitile diff/viv-cmd-diff info/viv_info
SBINPROGS	:=dump/viv-unpack udev/devcoredump
UDEVRULES	:=udev/99-local-devcoredump.rules
PROGS		:=$(BINPROGS) $(SBINPROGS) $(UDEVRULES)

all:	$(PROGS)

install: all
	install -m 755 -o root -g root $(SBINPROGS) $(sbindir)
	install -m 644 -o root -g root $(UDEVRULES) $(udevrulesdir)
	install -m 755 -o root -g root $(BINPROGS) $(bindir)

uninstall:
	$(RM) $(patsubst %,$(sbindir)/%,$(notdir $(SBINPROGS)))
	$(RM) $(patsubst %,$(udevrulesdir)/%,$(notdir $(UDEVRULES)))
	$(RM) $(patsubst %,$(bindir)/%,$(notdir $(BINPROGS)))

clean:
	$(RM) $(PROGS) *.[oas] */*.[oas]

%:	%.in
	$(SED) "$(SEDARGS)" $< > $@

info/features.h: include/hw/common.xml.h
	{ \
	for n in chipFeatures chipMinorFeatures0 chipMinorFeatures1 chipMinorFeatures2 chipMinorFeatures3 chipMinorFeatures4 chipMinorFeatures5; do \
	echo "static struct feature vivante_$${n}[] __maybe_unused = {"; \
	echo "#define FEATURE(x) { $${n}_##x, #x }"; \
	sed -n "s/#define $${n}_\([^[:space:]]*\).*/\tFEATURE(\1),/p" $<; \
	echo "#undef FEATURE"; \
	echo "};"; \
	done; \
	} > $@

detile/viv-demultitile.o: detile/viv-demultitile.c

detile/viv-demultitile: detile/viv-demultitile.o

diff/viv-cmd-diff.o: diff/viv-cmd-diff.c include/hw/state.xml.h

diff/viv-cmd-diff: diff/viv-cmd-diff.o

dump/viv-unpack.o: dump/viv-unpack.c \
	include/hw/state.xml.h include/etnaviv_dump.h

dump/viv-unpack: dump/viv-unpack.o

LDLIBS_viv_info		:=$(libdrm_ldflags)
info/viv_info: info/viv_info.o

CFLAGS_viv_info.o	:=$(libdrm_cflags)
info/viv_info.o: info/viv_info.c info/features.h include/etnaviv_drm.h
