#
# Makefile
#

CONTENT	= index.h status.h pi1541-logo.h style.h update.h edit-config.h mount-imgs.h C64_Pro_Mono-STYLE.h #tuning.h ledoff.h ledon.h favicon.h

EXTRACLEAN = $(CONTENT) converttool

all: converttool $(CONTENT)

%.h: %.html
	@echo "  GEN   $@"
	@./converttool $< > $@

%.h: %.css
	@echo "  GEN   $@"
	@./converttool $< > $@

%.h: %.png
	@echo "  GEN   $@"
	@./converttool -b $< > $@

%.h: %.ttf
	@echo "  GEN   $@"
	@./converttool -b $< > $@
	
%.h: %.ico
	@echo "  GEN   $@"
	@./converttool -b $< > $@
	
$(CONTENT): converttool

converttool: converttool.c
	@echo "  TOOL  $@"
	@gcc -o converttool converttool.c

clean:
	rm -f $(CONTENT) converttool