
CFLAGS= -O3 -flto=auto -DNL10_STANDALONE #-std=c89

all: petlp vicelp
petlp: printer.o petlp.o pchar.o charsets.o
vicelp: vice-drv.o vice-helper.o drv-nl10.o drv-mps803.o drv-1520.o miniz-standalone.o
	cc -o vicelp $^
miniz-standalone.o: ../miniz.c ../miniz.h
	$(CC) $(CFLAGS) -c -o $@ ../miniz.c
clean: 
	rm -f *.o *.d *.pdf *.png *.ps petlp vicelp
