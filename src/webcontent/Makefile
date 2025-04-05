#
# Makefile
#

CONTENT	= index.h pi1541-logo.h style.h config.h edit-config.h #tuning.h ledoff.h ledon.h favicon.h

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
	
%.h: %.ico
	@echo "  GEN   $@"
	@./converttool -b $< > $@
	
$(CONTENT): converttool

converttool: converttool.c
	@echo "  TOOL  $@"
	@gcc -o converttool converttool.c

clean:
	rm -f $(CONTENT) converttool