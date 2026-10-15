/*
 * $Id: printer.c,v 1.1 1996/04/01 08:25:07 jopi Exp jopi $
 *
 * Commodore compatible Matrix Printer Emulator.
 *
 * See README for copyright notice.
 *
 * Restricted Epson compatible Serial printer emulator.
 * Only text mode is supported.
 *
 *
 * Written by
 *   Jarkko Sonninen  (sonninen@lut.fi)
 *   Jouko Valta      (jopi@stekt.oulu.fi)
 *
 *
 * $Log: printer.c,v $
 * Revision 1.1  1996/04/01  08:25:07  jopi
 * Initial revision
 *
 *
 */


#define STANDALONE_PRINTER
#define	PETLPVERSION	1.1

/* #define DEBUG_PRINTER */
#ifdef STANDALONE_PRINTER
#define POSTSCRIPT
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include "config.h" 
#include "types.h"
#include "proto.h" 
#include "serial.h" 
#include "printer.h"
#include "charsets.h"
#include "proto.h"
/* #include "patchlevel.h" */

#define PRN_DEFAULTMODE  FT_LATIN_1


#ifdef POSTSCRIPT
#define	COMMANDMODE()	do_commandmode(printer);
#else
#define	COMMANDMODE()
#endif


/* Extern functions */

#ifdef POSTSCRIPT
extern int  to_psvect ( int lang, int lwcase, BYTE c );
extern int  writeps ( PRINTER *printer, int width, int height, BYTE *string );
extern int  drawps ( FILE *out, int x, int y, int width, int height, int cpi, int a, BYTE *string );
extern int  drawbar ( FILE *out, int width, int height, BYTE value );

extern void pr_set_reverse ( FILE *out, int t );
extern void pr_set_size ( FILE *out, int t );
extern void pr_set_colour ( FILE *out, int c );
extern void pr_set_font ( FILE *out, int t );
extern void pr_set_lf ( FILE *out, int n );
#endif


#ifndef __CEXTRACT__

#ifdef STANDALONE_PRINTER
static int  do_selftest ( PRINTER *printer );
#endif

extern int  initialize_printer ( int dev, char *lang, char *locale );
extern void detach_printout ( PRINTER *printer );
extern int  attach_printout ( PRINTER *printer, char *name, int mode );

extern int  open_printer ( void *printer, char *name, int length, int secondary );
extern int  close_printer ( void *printer, int secondary );
extern int  read_printer ( void *printer, BYTE *data, int secondary );
extern int  write_printer ( void *printer, BYTE data, int secondary );
extern void flush_printer ( void *printer, int secondary );

static int  output_line ( PRINTER *printer );
static int  printercommand ( PRINTER *printer, BYTE *buf, int len );
static void do_commandmode ( PRINTER *printer );
extern void do_newline ( PRINTER *printer, int lf );
static void do_formfeed ( PRINTER *printer );

int  confirm ( char *q );
extern int  parse_prn_fmt ( char *f );

#endif /* __CEXTRACT__ */


static char *filetypes[] = {
    "binary", "log", "petcii", "ascii", "Latin/1", "PS", "", "", "image"
 };

#define MAX_FT 6

#define INBUFSIZ  (1024*64)


/* ------------------------------------------------------------------------- */

#ifdef STANDALONE_PRINTER

static PRINTER *PrinterData = NULL;
static int   petcii = 1;

int petlp_main(int argc, char **argv)
{
	FILE *source = stdin;
	BYTE *s;
	char *progname, *outfilename = NULL, c;
	int i, fil = 0, test = 0;
	int secondary = 0, n;
	int pi1541 = 0;

	set_locale(NULL);
	initialize_printer(0, NULL, NULL);

	fprintf(stderr, "\n\t%s V%4.2f -- Commodore Matrix Printer Emulator.\n",
			argv[0], (float)PETLPVERSION);

	/* Parse arguments */

	progname = argv[0];
	while (--argc && ((*++argv)[0] == '-'))
	{
		if (!strcmp(argv[0], "--"))
		{
			--argc;
			++argv;
			break;
		}

		if (!strcmp(argv[0], "-c"))
		{
			PrinterData->mode &= ~T_CLEAN;
			continue;
		}
		else if (!strcmp(argv[0], "-nc"))
		{
			PrinterData->mode |= T_CLEAN;
			continue;
		}

		else if (!strcmp(argv[0], "-test"))
		{
			++test;
			continue;
		}

		/* Charset */

		if (!strcmp(argv[0], "-upper"))
		{
			secondary = 0;
			continue;
		}
		else if (!strcmp(argv[0], "-lower") || !strcmp(argv[0], "-business"))
		{
			secondary = 7;
			continue;
		}

		else if (!strcmp(argv[0], "-ascii") || !strcmp(argv[0], "-latin"))
		{
			petcii = 0; /* Ascii/Latin input */
			PrinterData->lang = CS_USA;
			PrinterData->mode |= T_ASCIIMODE;
			continue;
		}
		else if (!strcmp(argv[0], "-graph"))
		{ /* Latin + graph */

			continue;
		}

		else if (!strcmp(argv[0], "-lang"))
		{
			if (argc > 1 &&
				(PrinterData->lang = str_to_lang(argv[1])) >= 0)
			{
				--argc;
				++argv;
				continue;
			}
			/* Fall to error */
		}

		/* Dimensions */

		else if (!strcmp(argv[0], "-left"))
		{
			if (argc > 1 &&
				sscanf(argv[1], "%d", &(PrinterData->mgn_left)) == 1)
			{
				--argc;
				++argv;
				continue;
			}
			/* Fall to error */
		}

		else if (!strcmp(argv[0], "-right"))
		{
			if (argc > 1 &&
				sscanf(argv[1], "%d", &(PrinterData->mgn_right)) == 1)
			{
				--argc;
				++argv;
				continue;
			}
			/* Fall to error */
		}

		else if (!strcmp(argv[0], "-top"))
		{
			if (argc > 1 &&
				sscanf(argv[1], "%d", &(PrinterData->mgn_top)) == 1)
			{
				--argc;
				++argv;
				continue;
			}
			/* Fall to error */
		}

		else if (!strcmp(argv[0], "-bot"))
		{
			if (argc > 1 &&
				sscanf(argv[1], "%d", &(PrinterData->mgn_bottom)) == 1)
			{
				--argc;
				++argv;
				continue;
			}
			/* Fall to error */
		}

		/* File */

		else if (!strcmp(argv[0], "-F"))
		{ /* Output file format */
			if (argc > 1)
			{
				if ((PrinterData->of_format = parse_prn_fmt(argv[1])) >= 0)
				{
					--argc;
					++argv;
					continue;
				}
			}
			else
				fprintf(stderr, "\nFormat parameter missing\n");
			/* Fall to error */
		}

		else if (!strcmp(argv[0], "-o"))
		{
			if (argc > 1)
			{
				outfilename = argv[1];
				--argc;
				++argv;
				continue;
			}
			fprintf(stderr, "\nOutput filename missing\n");
			/* Fall to error */
		}

		else if (!strcmp(argv[0], "-P") || !strcmp(argv[0], "-printer"))
		{
			if (argc > 1)
			{
				PrinterData->lpName = argv[1];
				--argc;
				++argv;

				fprintf(stderr,
						"One cannot always win. Not even every time.\n");

				continue;
			}
			fprintf(stderr, "\nPrinter name missing\n");
			/* Fall to error */
		}
		else if (!strcmp(argv[0], "-pi1541"))
		{
			fprintf(stderr, "\n\t%s V%4.2f -- Commodore Matrix Printer Emulator.\n",
					progname, (float)PETLPVERSION);
			--argc;
			++argv;
			pi1541 = 1;
			continue;
		}

		if (!strcmp(argv[0], "-help") ||
			!strcmp(argv[0], "-?") ||
			!strncmp(argv[0], "-v", 2))
		{ /* version ID */
			fprintf(stderr,
					"\n\t%s V%4.2f -- Commodore Matrix Printer Emulator.\n",
					progname, (float)PETLPVERSION);

			/* Fall to error for Usage */
		}
		else
			fprintf(stderr, "Bailing out near '%s'\n", argv[0]);

		fprintf(stderr,
				"\nUsage: %7s  [-c | -nc] [-latin | -graph ] [-lang langname ]\
\n\
\t\t[-upper | -lower | -business ]\n\
\t\t[-left n ] [-right n ] [-top n ] [-bot n ]\n\
\t\t[-o outfile ] [-F format ] [-P printer ] [--] [file list]\n\n",
				progname);

		fprintf(stderr, "\n\
   -c\t\tcontrols (interpret also control codes)\n\
   -nc\t\tno controls (suppress control codes in printout)\n\
   -upper\tselect graphics character set\n\
   -lower\tselect business character set\n\
   -business\tselect business character set\n\
   -lang\tspecify International character set\n\
   -ascii\tselect ASCII input (resets -lang to US-ASCII)\n\
   -latin\tsame as -ascii\n\
   -F output-format\n\
\n");

		fprintf(stderr, "Available output formats are:\n");
		for (i = 0; i < MAX_FT; i++)
		{
			if (strlen(filetypes[i]))
			{
				fprintf(stderr, "%s ", filetypes[i]);
			}
		}
		fprintf(stderr, "\n\n");
		exit(1);
	}

	/*
	 * Check parameters
	 */

	if (PrinterData->of_format == FT_RAW || PrinterData->of_format == FT_HEXDUMP
#ifndef POSTSCRIPT
		|| PrinterData->of_format == FT_PS
#endif
	)
	{
		fprintf(stderr, "\n%s: Output format invalid.\n", progname);
		exit(1);
	}

	if (!outfilename)
		outfilename = "-"; /* stdout */

	if (attach_printout(PrinterData, outfilename, PrinterData->of_format))
		exit(-1);

	if (open_printer(PrinterData, NULL, 0, secondary) != SERIAL_OK)
	{
		fprintf(stderr,
				"\n%s: Can't open output file %s\n", progname, outfilename);
		exit(1);
	}

	/*
	 * Self test
	 */

	if (test)
	{
		do_selftest(PrinterData);

		if (PrinterData->of_format == FT_PS)
			do_formfeed(PrinterData); /* eject paper */

		close_printer(PrinterData, secondary);
		return (0);
	}

	if (pi1541)
		return 0;

	/*
	 * Loop all files
	 */

	do
	{
		PrinterData->state &= ~T_QUOTE;

		if (!argc)
			source = stdin;
		else
		{
			if ((source = fopen(argv[0], "rb")) == NULL)
			{
				fprintf(stderr,
						"\n%s: Can't open file %s\n", progname, argv[0]);
				exit(1);
			}
		}

		/* Program file in binary format ? */

		if ((c = getc(source)) == EOF || ungetc(c, source) == EOF ||
			!c || c == 1)
		{
			fprintf(stderr, "\nInput file '%s' of invalid type.\n", argv[0]);
			continue; /* next file */
		}

		/* Print */

		s = PrinterData->inbuf;
		while (PrinterData->inptr < INBUFSIZ &&
			   (n = (int)fread(s, 1, INBUFSIZ - PrinterData->inptr, source)) > 0)
		{

			PrinterData->inptr += n;
			output_line(PrinterData);
			s = PrinterData->inbuf + PrinterData->inptr;
			/* #ifdef DEBUG_PRINTER
					fprintf(stderr, "read %d bytes, inptr %d.\n",
						n, PrinterData->inptr);
			#endif
			*/
		}

		if (argc > 1 || PrinterData->of_format == FT_PS)
			do_formfeed(PrinterData); /* eject paper */

		if (argc)
			fclose(source);

	} while (fil && --argc && ++argv); /* next file */

	close_printer(PrinterData, secondary);
	return (0);
}

int pi1541_printline(BYTE *line, int len)
{
	if (!PrinterData)
		return -1;

	if (PrinterData->inptr + len > INBUFSIZ)
		return -1;

	memcpy(PrinterData->inbuf + PrinterData->inptr, line, len);
	PrinterData->inptr += len;

	output_line(PrinterData);

	return 0;
}

int pi1541_close(void)
{
	if (!PrinterData)
		return -1;
	if (PrinterData->of_format == FT_PS)
		do_formfeed(PrinterData); /* eject paper */

	close_printer(PrinterData, 0);
	return 0;
}

static int  do_selftest (PRINTER *printer)
{
    BYTE *p;
    int c, n, i;


    /*
     * Write system test data
     */

    p = printer->inbuf;

    for (c = 0; c < P_MAX_COLOUR; ++c) {
	*p++ = 27;
	*p++ = 114;
	*p++ = c;

	for (n = 0, i = '!' + c; n < 80; ++n, ++i) {
	    if (i > 127) i = '!';
	    *p++ = i;
	}
	*p++ = 13;
    }

    *p++ = '\n';

    /*for (n = 0, i = 'A' + c; n < 80; ++n, ++i)
	    *p++ = i;*/

    printer->inptr = P_MAX_COLOUR * 84 + 1;

    output_line(printer);
    return (0);
}

#endif  /* STANDALONE_PRINTER */


/* ------------------------------------------------------------------------- */

int     initialize_printer(int dev, char *lang, char *locale)
{
    PRINTER *printer;

    /*
     * Create instance of the printer
     */

    printer = (PRINTER *)malloc(sizeof(PRINTER));
    assert(printer);
    memset (printer, 0, sizeof(PRINTER));	/* init all pointers */

    printer->inbuf =(BYTE *)malloc(INBUFSIZ +10);  /* graphics max 1920 dots */

    printer->type      = DT_PRINTER;
    printer->of_format = PRN_DEFAULTMODE;

    printer->mgn_right = 80;
    printer->mgn_bottom= 64;
    printer->lf_advance= 36;  /* 4,2 mm */
    printer->page_h    = PS_HEIGHT -30;
    printer->page_w    = PS_WIDTH;

    sprintf(printer->ActiveName, "/tmp/hmm_%d", dev);	/* default */
    /*printer->lp_command = ;*/

    if (!lang || (printer->lang = str_to_lang(lang)) < 0)
	printer->lang = PRN_CS_DEFAULT;

#ifdef STANDALONE_PRINTER
    PrinterData = printer;

#else
    if (attach_serial_device(dev, (char *)printer, "Printer", read_printer,
	    write_printer, open_printer, close_printer, flush_printer)) {
	printf("Could not initialize printer #%d ????\n", dev);
	return(-1);
    }
    else {
	printf("Printer #%d ready.\n", dev);
    }
#endif
    return 0;
}


/* ------------------------------------------------------------------------- */

int  confirm(char *q)
{
    char ans;


    do {
	printf (q);
	scanf ("%c*", &ans);
	ans = tolower(ans);
    } while (ans != 'y' && ans != 'n');

    return (ans == 'y');
}


/* Parse file format argument. If not recognized, -1 is returned. */

int  parse_prn_fmt(char *f)
{
    int n, i = MAX_FT -1;

    n = strlen(f);
    for (; (i >= 0) && strncmp(filetypes[i], f, n); --i);

    return (i);
}


/* ------------------------------------------------------------------------- */

/*
 * Functions to attach blank files for printing.
 */

void    detach_printout(PRINTER *printer)
{

    if (!printer)
	return;

    if (printer->FileDs != NULL) {

	if (!confirm("There are # pages to be printed on 's'. Proceed printing (y/n) ? "))
	    printf("Printer file aborted.\n");

	printf("Detaching printer file %s\n", printer->ActiveName);
	close_printer((void *)printer, -1);   /* Panic. (SA >= 0 for close) */
    }
}


int     attach_printout(PRINTER *printer, char *name, int mode)
{
    /*char buf[256];*/
    int  format;


    detach_printout(printer);

    if (!name) {
	printf("No name, detaching image.\n");
	return (-1);
    }

    if ((format = mode & 15) > MAX_FT) {
	fprintf (stderr, "Illegal fileformat specified.\n");
	return (-1);
    }
    printer->of_format = format;

/*
   print_page(printer);
*/

#ifdef STANDALONE_PRINTER
    if (*name == '-') {
	printer->FileDs = stdout;
    }
    else
#endif

    if ((printer->FileDs = fopen (name, "wb")) == NULL) {
	perror (name);
	return (-1);
    }

    strcpy(printer->ActiveName, name);
    fprintf(stderr, "Printing to %s file '%s'.\n",
	   filetypes[format], (*name == '-' ? "on stdout" : name));


#ifdef POSTSCRIPT
    if (printer->of_format == FT_PS)
#ifdef STANDALONE_PRINTER
	init_ps(printer->FileDs, petcii, 1, printer->lang, 0 /*dinmode*/);
#else
	init_ps(printer->FileDs, 1, 1, printer->lang, 0);
#endif
#endif  /* POSTSCRIPT */

    return (0);
}


/*
static int  print_page(PRINTER *printer)
{

    if ((format == FT_PS || format == FT_ASCII || format == FT_LATIN_1 ) &&
	printer->lpCommand && *printer->lpCommand) {
	sprintf (buf, "%s %s", printer->lpCommand, printer->lpName);
    }
    printf("Sending to printer '%s'.\n", name);

		if (printer->ActiveName)
		    remove (printer->ActiveName);
}
*/


/* ------------------------------------------------------------------------- */

/*
 * Serial Bus Interface
 */


int     open_printer(void *prn, char *name, int length, int secondary)
{
    PRINTER *printer = (PRINTER *)prn;

    printer->state &= ~T_CASE;
    printer->state |= ((secondary == 7) ? T_LOWC : T_UPPC);

    /*if (*(printer->ActiveName))
	remove (printer->ActiveName);
    else
	sprintf(printer->ActiveName, "/tmp/hmm");*/

    if (printer->FileDs == NULL &&
	attach_printout(printer, "/tmp/hmm", PRN_DEFAULTMODE) < 0) {
	fprintf(stderr, "Printer: Cannot create file '%s'\n",
		printer->ActiveName);

	return (SERIAL_ERROR);
    }

    return (SERIAL_OK);
}


int     close_printer(void *prn, int secondary)
{
    PRINTER *printer = (PRINTER *)prn;

    if (printer->FileDs) {
	if (printer->inptr)
	    output_line(printer);

	fclose (printer->FileDs);

	if (secondary < 0) {	/* Print if SA >= 0 for close */
	    fprintf(stderr, "\nPrinter file aborted.\n");

	}
	else {			/* Normal CLOSE */
	}
	printer->FileDs = NULL;
    }  /* if FileDs */

    return SERIAL_OK;
}


int     read_printer(void *printer, unsigned char *data, int secondary)
{
    return SERIAL_EOF;
}


int     write_printer(void *prn, unsigned char data, int secondary)
{
    PRINTER *printer = (PRINTER *)prn;

    /*
     * First all incoming data is buffered, until a whole line is received.
     * Then control is passed to parser routine to search for commands and
     * print out the rest.
     */


    switch (printer->of_format) {
      case FT_RAW:
	return ((fputc(data & 0xff, printer->FileDs) < 0)
		? SERIAL_ERROR : SERIAL_OK);

      case FT_HEXDUMP:
	fprintf(printer->FileDs, "%02x ", data & 0xff);
	printer->inbuf[printer->inptr++] = p_toascii(data, 0);
	if (printer->inptr > 15) {
	    fprintf(printer->FileDs, " %s\n", printer->inbuf);
	    printer->inptr = 0;
	}
	return (SERIAL_OK);

      default:
	printer->inbuf[printer->inptr++] = data;

	if (data == 0x0d)
	    output_line(printer);
	return (printer->inptr >= INBUFSIZ ? SERIAL_ERROR : SERIAL_OK);
    }
}


void    flush_printer(void *prn, int secondary)
{
    PRINTER *printer = (PRINTER *)prn;

    if (printer->inptr)
	output_line(printer);		/* Check for lines ready */

    if (((PRINTER *)printer)->FileDs)
	fflush (((PRINTER *)printer)->FileDs);
    return;
}


/* ------------------------------------------------------------------------- */

static int  output_line (PRINTER *printer)
{
    char  *tmp, buf[12];
    BYTE  *p;
    BYTE   quote = 0;	/* flag */
    int    cnt, col, len = 0;


    /*
     * In order to print the text it must be cleaned and
     * converted into ASCII
     */

    if (!printer->inptr)
	return (0);

#ifdef STANDALONE_PRINTER
    quote = (printer->state & T_QUOTE);
#endif

    p = printer->inbuf;
    cnt = 0;

    if ((col = printer->column) < printer->mgn_left)
	do_newline(printer, 0);


    do {
	len = 0;

/*#ifdef DEBUG_PRINTER
    fprintf (stderr, "%2d/%2d '%c'\n", cnt, printer->inptr, *p);
#endif
*/

	/* Special characters */

	switch (*p) {

	  case 9:	/* CBM: 7-bit graphics  ASCII: TAB */
	    if (printer->mode & T_ASCIIMODE) {

		col = (col + 8) & 0xf8;
		printer->column = (printer->column + 8) & 0xf8;
		continue;
	    }
#ifdef POSTSCRIPT
	  case 8:	/* 7-bit graphics */
	    if (!quote) {
		COMMANDMODE();
		printer->graphics = *p & 1;
		printer->gr_pitch = (*p == 8 ? 60 : 120);
		printer->gr_cnt = 0;
		continue;
	    }
#endif
	    break;

	  case 10:	/* LF  -- quote mode ignored */
	    do_newline(printer, 1);
	    continue;

	  case 11:	/* Vertical TAB */
	     if (!quote) {
		 do_newline(printer, 1);
		 continue;
	    }
	    break;

	  case 12:	/* Form Feed */
	     if (!quote) {
		do_formfeed(printer);
		continue;
	    }
	    break;

	  case 13:	/* CRLF  -- quote mode turned off */
	    quote = 0;
	    printer->state &= ~(T_OLUPPC | T_OLLOWC);

	    COMMANDMODE();

	    if (printer->reverse) {
		printer->reverse = 0;
#ifdef POSTSCRIPT
		pr_set_reverse(printer->FileDs, 0);

	    if (printer->mode & T_ASCIIMODE)	/* ASCII: Cancel expanded printing */
		pr_set_size(printer->FileDs, 0);
#endif
	    }

	    do_newline(printer, 1);	/* "1" here means DIP 1 is ON */
	    continue;


	  case 14:	/* Expanded Printing (Ascii: for one line) */
	  case 15:	/* Cancel Expanded Printing */
	    if (!quote) {
		if (printer->graphics) {
				/* Send image and turn off 7-bit Graphics */

#ifdef POSTSCRIPT
		    if (printer->gr_cnt > 0)
			drawps (printer->FileDs,
				printer->column *7, printer->line, printer->gr_cnt,
				7, printer->gr_pitch, 0, printer->gr_buf);

		    COMMANDMODE();
		    pr_set_reverse(printer->FileDs, 0);
#endif
		    printer->graphics = 0;
		}
#ifdef POSTSCRIPT
		/* Set / Cancel Expanded Printing */
		pr_set_size(printer->FileDs, !(*p & 1));
#endif
	    }
	    break;

	  case 16:	/* Set TABs */
	    if (!quote) {
		COMMANDMODE();
		if ((len = printer->inptr - cnt) > 0)
		    len = printercommand(printer, p, len);
		else
		    len = -1;
	    }
	    break;


	  case 26:	/* Repeat 7-bit Graphics Pattern (inside 7-bit cmmd) */
	    if (!quote && printer->graphics) {
		if (printer->inptr - cnt < 3) {
		    len = -1;
		    break;
		}

#ifdef POSTSCRIPT
		if (!printer->gr_cnt && ( (p[3] & 254) == 14))

		    /*drawps (printer->FileDs,
			    printer->column *7, printer->line, printer->gr_cnt,
			    7, printer->gr_pitch, 0, p +3);*/


		    /* Draw Bar */

		/*len = drawbar (printer->FileDs,
			       *(p+1), 7, *(p+2), printer->inptr - cnt);
		 */
		    ;
		else
#endif
		    len = 2;
	    }
	    break;


	  case 27:	/* ESC */
	    if (quote) {
		if (printer->mode & T_CLEAN) {
		    /* C128 escape -- two bytes must be skipped to clean */
		    len = 1;
		} else {
		    fprintf(printer->FileDs, "(esc)");
		}
	    } else {
		/* escape -- printer command is to follow */
		COMMANDMODE();
		if ((len = printer->inptr - cnt -1) > 0)
		    len = printercommand(printer, p +1, len);
		else
		    len = -1;
	    }
	    break;


	    /*
	     * PETCII's ON/OFF Codes
	     */

	  case 34:	/* quotes */
	    if (!printer->graphics)
		quote ^= T_QUOTE;
	    break;

	  case 145:	/* no quote: uppercase for one line; quote: "(up)" */
	    if (printer->graphics)
		break;
	  case 17:	/* no quote: lowercase for one line; quote: "(down)" */
	    if (!quote) {
		printer->state &= ~(T_OLUPPC | T_OLLOWC);
		printer->state |= ((*p == 0x11) ? T_OLLOWC : T_OLUPPC);
		continue;
	    }
	    break;

	  case 18:	/* no quote: reverse on, ASCII: condensed off */
	    if (!quote) {
		printer->reverse = 1;
		printer->gr_cnt = 0;
#ifdef POSTSCRIPT
		COMMANDMODE();
		pr_set_reverse(printer->FileDs, 1);
#endif
		continue;
	    }
	    break;

	  case 146:	/* no quote: reverse off */
	    if (!quote && !printer->graphics) {
		printer->reverse = 0;
#ifdef POSTSCRIPT
		COMMANDMODE();
		pr_set_reverse(printer->FileDs, 0);
#endif
		continue;
	    }
	} /* switch (*p) */



	/*
	 * Filters
	 */

	if (len < 0) {
#ifdef DEBUG_PRINTER
	    fprintf (stderr, "DEBUG: Insufficient data for %02X\n", *p);
#endif
	    break;		/* insufficient data */
	}
	if (len > 0) {
	    p += len;
	    cnt += len;
	    continue;		/* command succeeded */
	}


	if (printer->graphics) {	/* Image data for 7-bit graphics */
	    if (*p & 128 && printer->gr_cnt < GR_BUFSIZE)
		printer->gr_buf[printer->gr_cnt++] = *p;
	    continue;
	}


	/*
	 * PETSCII to ASCII conversion.
	 * Also, unprintable characters still remaining are converted to
	 * the best representation available. Alas, there is not just one or
	 * two conversions, rather a complete chaos...
	 */

	/* Control codes and CBM-x Graphics are converted to strings */

	tmp = NULL;

#ifdef STANDALONE_PRINTER
	if (petcii) {
#endif
	    if ((*p & 0x7F) < 0x20)
		tmp = ctrl_to_str(printer->mode & T_CLEAN, *p);

	    else if ((printer->of_format != FT_PS || !(printer->graphics)) &&
		     ((*p >= 0xA0 && *p < 0xC0) || (*p >= 0xE0 && *p <= 0xFE)))
		tmp = cbm_to_str(*p);
	    else {
		tmp = buf;
		*buf = *p;
		buf[1] = 0;

		if (!(printer->state & (T_LOWC | T_OLLOWC)) &&
		    (printer->of_format != FT_PS && !(printer->graphics))) {
			sprintf (buf, "<SHIFT-%c>", *p);
		}
		else {
		    if (printer->of_format == FT_ASCII ||
			printer->of_format == FT_LATIN_1)

			*buf = p_toichar(printer->lang,
				     printer->state & (T_LOWC | T_OLLOWC), *p);
#ifdef POSTSCRIPT
		    else
			*buf = to_psvect(printer->lang,
				     printer->state & (T_LOWC | T_OLLOWC), *p);
#endif
		}
	    }
#ifdef STANDALONE_PRINTER
	}
	else {				/* Latin */
	    tmp = buf;
	    *buf = *p;
	    buf[1] = 0;

	    if (printer->of_format == FT_PETSCII)
		buf[0] = p_topetcii(*p);
	} /* petcii */
#endif

#ifdef POSTSCRIPT
	if (printer->of_format == FT_PS)
	    writeps (printer, 0, 0, (BYTE *)tmp);
	else
#endif
	{   col += fprintf (printer->FileDs, tmp);

	    if (*p == '\n' || col >= printer->mgn_right) {
		do_newline(printer, 1);
		col = printer->mgn_left;
	    }

	    /*printer->column = col;*/
	}

    } while (p++ && ++cnt < printer->inptr);


    /*
     * Clean Input Buffer
     * memmov() in "misc.c" is similar to the official memmove(), and both
     * will copy overlapping areas correctly.
     */

#ifdef DEBUG_PRINTER
    fprintf(stderr, "left %d bytes.\n", printer->inptr - cnt);
#endif

    if (cnt < printer->inptr) {
	memmove (printer->inbuf, p, (size_t)(printer->inptr - cnt));
	printer->inptr -= cnt;
    }
    else
	printer->inptr = 0;


#ifdef STANDALONE_PRINTER
    if (quote)
	printer->state |= T_QUOTE;
    else
	printer->state &= ~T_QUOTE;
#endif

    return 0;
}


static int  printercommand(PRINTER *printer, BYTE *buf, int len)
{
    BYTE *p = 0;
    int   c = 0;
    int   bytes = 1;


    /*
     * Execute a printer command and return its length. Return value < 0
     * means insufficient data, in which case second attempt will be taken
     * later. Also note that the pending command survives flush(), switching
     * off-line and CLOSE, to be completed after connection is resumed.
     */

    /* Multi-byte escape sequences (i.e. printer commands) */

    if (len >= 2)
	c = buf[1] & 255;

#ifdef DEBUG_PRINTER
    fprintf (stderr, "Exec %02d %02d (%d bytes left)\n", *buf, c, len);
#endif


    switch (*buf) {

	/* Font */
	/* Commands 120, 107, 52x and 53 are ignored if NLQ is locked. */


      case 120:		/* Select draft/NLQ characters */
	if (len < 2) return (-1);
	bytes = 2;
	break;

      case 107:		/* Select NLQ type style */
	if (len < 2) return (-1);
	bytes = 2;
	break;

      case 52:		/* Select Italic Characters */
      case 53:		/* Select Upright Characters */
	break;

      case 69:		/* Emphasized printing */
      case 70:		/* Cancel emphasized printing */
      case 71:		/* Double-strike printing */
      case 72:		/* Cancel double-strike printing */
	break;

      case 45:		/* Underline */
	if (len < 2) return (-1);
	bytes = 2;

      case 83:		/* Superscript / Subscript */
	if (len < 2) return (-1);
	bytes = 2;

      case 84:		/* Cancel superscript or subscript */
	break;


	/* Charset */

      case 82:		/* Select international character set */
	if (len < 1) return (-1);
	if ((c &= 15) < NUM_LANGUAGES) {	/* Actually c < 10, but ... */
	    printer->lang = c;
	}
	bytes = 2;
	break;

      case 126:		/* Select normal/slash zero */
	if (len < 2) return (-1);
	bytes = 2;
	break;


	/* Size & Pitch */
	/* Commands 80, 77, 15, 112 and 33 ignored if Pitch is locked */


      case 80:		/* Pica pitch */
      case 77:		/* Elite pitch */
      case 15:		/* Condensed Printing */
	break;


      case 119:		/* Print normal height / double-height characters */
	c <<= 1;

      case 87:		/* Expanded printing on/off */
      case 104:		/* Select double or quadruple size */
	if (len < 2) return (-1);
#ifdef POSTSCRIPT
	pr_set_size(printer->FileDs, c);
#endif
	bytes = 2;
	break;

      case 112:		/* Select fixed/proportional spacing */
	if (len < 2) return (-1);
	bytes = 2;
	break;

      case 33:		/* Master Print Mode */
	if (len < 2) return (-1);
	bytes = 2;
	break;


	/* Vertical Position */

      case 48:		/* Set Line Spacing to 1/8 inch */
	printer->lf_advance = 27;
	break;

      case 49:		/* Set Line Spacing to 7/72 inch */
	printer->lf_advance = 21;
	break;

      case 50:		/* Set Line Spacing to 1/6 inch - default */
	printer->lf_advance = 36;
	break;

      case 65:		/* Set line spacing to n/72 inch */
	c *= 3;
	--bytes;
      case 51:		/* Set Line Spacing to n/216 inch */
	if (len < 2) return (-1);
	printer->lf_advance = c;
	++bytes;
	break;

      case 10:		/* Reverse LF */
	c = -(printer->lf_advance);
	len = 2;
	--bytes;
      case 74:		/* Perform one n/216-inch line feed */
	if (len < 2) return (-1);
	++bytes;
	printer->line += c;
	do_newline(printer, 0);
	break;

      case 67:		/* Set page length to n inches */
	bytes = c ? 2 : 3;
	if (len < bytes) return (-1);
	break;

      case 99:		/* Set top margin */
	if (len < 2) return (-1);

      case 78:		/* Set bottom margin */
	if (len < 2) return (-1);
	bytes = 2;

      case 79:		/* Cancel top and bottom margins */
	break;

      case 12:		/* Return to top of current page */
	printer->line = printer->mgn_top;
	do_newline(printer, 0);
	break;

      case 66:		/* Set vertical tab stops */
	if (len < 3) return (-1);
	bytes = 3;
	break;


	/* Horizontal Position */

      case 108:		/* Set left margin */
	if (len < 2) return (-1);
	bytes = 2;
	break;

      case 81:		/* Set right margin */
	if (len < 2) return (-1);
	bytes = 2;

      case 97:		/* Left/Right justify */
	if (len < 2) return (-1);
	bytes = 2;
	break;

      case 68:		/* ASCII: Set horizontal tab stops */
	if (len < 3) return (-1);
	bytes = 3;
	for (p = buf + 1; *p && ++bytes <= MAX_HTABS; p++) { /* command byte */

	}
	break;

      case 16:		/* Set TABs - 2 parameters */
	if (len < 3) return (-1);
	bytes = 3;
	break;


	/* Graphics */

      case 75:		/* Print single-density 8-bit graphics */
      case 76:		/* Print double-density 8-bit graphics */
      case 89:		/* Print double-density, double-speed 8-bit graphics */
      case 90:		/* Print quadruple-density 8-bit graphics */
	if (len < 3 || (c |= ((buf[2] & 255) << 8)) >= len)
	    return (-1);

	bytes = c;
	c = (((*p >> 3) & 3) ^ (*p & 1));

#ifdef POSTSCRIPT
	drawps (printer->FileDs,
		printer->column *7, printer->line, bytes, 8, c, 0, p +3);
#endif
	bytes += 3;
	break;

      case 42:		/* Select Graphics Mode */
	if (len < 4 || (bytes = (buf[2] & 255) | ((buf[3] & 255) << 8)) >= len)
	    return (-1);

#ifdef POSTSCRIPT
	drawps (printer->FileDs,
		printer->column *7, printer->line, bytes, 8, c, 0, p +4);
#endif
	bytes += 4;
	break;

      case 18:		/* Reversed 7-bit Graphics */
	printer->graphics = 18;
	printer->gr_pitch = 60;
	printer->gr_cnt = 0;
#ifdef POSTSCRIPT
	pr_set_reverse(printer->FileDs, 1);
#endif
	break;


	/* Misc */

      case 25:		/* Sheet Feeder */
	if (len < 2) return (-1);
	bytes = 2;
	break;

      case 58:		/* Copy standard characters to RAM */
	if (len < 4) return (-1);
	bytes = 4;
	break;

      case 37:		/* Select ROM/RAM Carset */
	if (len < 2) return (-1);

      case 38:		/* Define RAM Characters */
	if (len < 4 || (c |= (buf[2] & 255)) >= len)      /* <------======== */
	    return (-1);

	break;

      case 43:		 /* Define/Execute Macro Instruction */
	break;

      case 64:		/* Reset printer */
	break;

      case 93:		/* Select Commodore/ASCII operating mode */
	if (len < 2) return (-1);
	bytes = 2;
	break;

      case 114:		/* Select printing colour */
	if (len < 2) return (-1);
#ifdef POSTSCRIPT
	if ((c &= 15) <= 7)	/* Add printercode -> colorcode later */
	    pr_set_colour(printer->FileDs, c);
#endif
	bytes = 2;
    }  /* switch */

    return (bytes);
}


/* ------------------------------------------------------------------------- */

static void do_commandmode (PRINTER *printer)
{
    if (printer->of_format == FT_PS) {

	if (printer->ps_state & PS_SHOW)
	    fprintf (printer->FileDs, ") show\n");

	/*if (printer->ps_state & PS_DRAW)
	    end_graphics();*/
    }
    printer->ps_state &= ~(PS_SHOW | PS_DRAW);
}


/* do newline */
void do_newline (PRINTER *printer, int lf)
{
    int i;


    do_commandmode(printer);

    if (lf) {
	if (printer->of_format == FT_PS) {

	}
	else
	    fputc ('\n', printer->FileDs);


	printer->line += (printer->graphics ? 21 : printer->lf_advance);
    }

    if (printer->of_format != FT_PS)
	for (i = 0; i < printer->mgn_left; i++)
	    fputc (' ', printer->FileDs);

    printer->column = printer->mgn_left;
}


static void do_formfeed (PRINTER *printer)
{

    do_commandmode(printer);

    fprintf (printer->FileDs,
	     (printer->of_format == FT_PS ? "showpage\n" : "\f"));

    /*if (printer->of_format == FT_PS)
	initpage (printer->FileDs);*/

    printer->ps_state = PS_BLANK;
}

