COMPILER = acme
CFLAGS = -v1 --cpu 6502 -f cbm -o

.PHONY: all clean d64 c1541
PROGS = fb20-3k.prg fb20-8k.prg fb20-mc.prg fb20.prg fb64.prg fb64dtv.prg fb16.prg fb128.prg

all: $(PROGS) d64

d64: c1541 $(PROGS)
	c1541 -format "cbm-filebrowser,00" d64 CBM-FileBrowser.d64 
	c1541 CBM-FileBrowser.d64 -write fb64.prg fb64
	c1541 CBM-FileBrowser.d64 -write fb20-3k.prg fb20-3k
	c1541 CBM-FileBrowser.d64 -write fb20-8k.prg fb20-8k
	c1541 CBM-FileBrowser.d64 -write fb20-mc.prg fb20-mc
	c1541 CBM-FileBrowser.d64 -write fb20.prg fb20
	c1541 CBM-FileBrowser.d64 -write fb64dtv.prg fb64dtv
	c1541 CBM-FileBrowser.d64 -write fb16.prg fb16
	c1541 CBM-FileBrowser.d64 -write fb128.prg fb128

c1541:
	@which c1541 > /dev/null || (echo "c1541 not found! Please install VICE package." && exit 1)

fb20-3k.prg: vic20-3k.asm vic20-3k.def CBM-FileBrowser.asm
	$(COMPILER) $(CFLAGS) $@ $<

fb20-8k.prg: vic20-8k.asm vic20-8k.def CBM-FileBrowser.asm
	$(COMPILER) $(CFLAGS) $@ $<

fb20-mc.prg: vic20-mc.asm vic20-mc.def CBM-FileBrowser.asm
	$(COMPILER) $(CFLAGS) $@ $<

fb20.prg: vic20-unexp.asm vic20-unexp.def CBM-FileBrowser.asm
	$(COMPILER) $(CFLAGS) $@ $<

fb64.prg: c64.asm c64.def CBM-FileBrowser.asm
	$(COMPILER) $(CFLAGS) $@ $<

fb64dtv.prg: c64dtv.asm c64dtv.def CBM-FileBrowser.asm
	$(COMPILER) $(CFLAGS) $@ $<

fb16.prg: c16.asm c16.def CBM-FileBrowser.asm
	$(COMPILER) $(CFLAGS) $@ $<

fb128.prg: c128.asm c128.def CBM-FileBrowser.asm
	$(COMPILER) $(CFLAGS) $@ $<
clean:
	rm -f *.prg *.d64
