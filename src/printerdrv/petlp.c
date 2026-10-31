/*
 * $Id: petlp.c,v 1.3 1996/04/01 09:01:41 jopi Exp jopi $
 *
 * Commodore compatible Matrix Printer Emulator.
 *
 * This file is code generator with PostScript(R) level 2.0 output.
 * The generated program implements  Commodore compatible Matrix Printer
 * Emulator for the printer specified by user.
 *
 *   Copyright (C) 1995 Jouko Valta and Hannu Helminen.
 *   All rights reserved.
 *
 *
 * Written by
 *   Jouko Valta     (jopi@stekt.oulu.fi)
 *   Hannu Helminen  (dm@stekt.oulu.fi)
 *
 *
 * Matrix Printer Emulator -- the PostScript(R) programs included
 *   Copyright (C) 1995 Jouko Valta and Hannu Helminen.
 *   All rights reserved.
 *
 * Bitmap Font Building from Bluebook example program_21.ps
 *   Copyright (C) 1985 Adobe Systems Incorporated.
 *   All rights reserved.
 *
 *
 * $Log: petlp.c,v $
 * Revision 1.3  1996/04/01  09:01:41  jopi
 * Page formatting
 *
 * Revision 1.2  1995/11/07  16:51:00  jopi
 * International characters
 * PS primitives
 *
 * Revision 1.1  1995/06/28  19:34:07  jopi
 * Initial revision
 *
 *
 */


#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>

#include "types.h"
#include "printer.h"
#include "charsets.h"


/* Variables */

extern printerfont7 pset[];

int PSLevel = 2;		/* Level 2.0 by default */


extern void do_newline ( PRINTER *printer, int lf );

/* Local */

static void do_header ( FILE *out );
static void pr_init_page ( PRINTER *printer );

static void do_fontdata ( FILE *out, int offset, int width, int height );
int  definefont ( FILE *out, int petcii, int csets, int lang, int dinmode );
extern int  to_psvect ( int lang, int lwcase, BYTE c );
extern int  writeps ( PRINTER *printer, int width, int height, BYTE *string );
extern int  drawps ( FILE *out, int x, int y, int width, int height, int cpi, int a, BYTE *string );
extern int  drawbar ( FILE *out, int width, int height, BYTE value );


extern void pr_set_reverse ( FILE *out, int t );
extern void pr_set_size ( FILE *out, int t );
extern void pr_set_colour ( FILE *out, int c );
extern void pr_set_font ( FILE *out, int t );
extern void pr_set_lf ( FILE *out, int n );


static int  ps_outline ( FILE *out );


/*
 * WARNING: Should you get obsession to start programming in PostScript,
 * we can assure you it's not difficult at all. Instead of trying to rip
 * these routines in vain, consult your manuals in fancy colours:
 *	o PostScript Language Reference Manual
 *	o PostScript Language Tutorial and Cookbook
 *	o PostScript Language: Program Design
 *	o Learning PostScript: a Visual Approach
 */


/* ------------------------------------------------------------------------- */

void init_ps(FILE *out, int petcii, int csets, int lang, int dinmode)
{

    do_header(out);
    definefont(out, petcii, csets, lang, dinmode);
    /*encoding(out, 0);*/

    fprintf(out, "/MPSfont findfont 7 scalefont setfont\n");

#ifdef DEBUG
    test_ps (out);		/* Print test picture */
    fprintf(out, "/MPSfont findfont 7 scalefont setfont\n");
#endif
}


void pr_init_page (PRINTER *printer)
{
    /*
     * Scale Images
     */

    fprintf (printer->FileDs, "\
%%5.66929 8.5039 scale	%% ensin skaalat kohdalleen\n\
%d 3 translate		%% siirret��n ensin v�h�n irti reunasta\n\
%%0 85 translate		%% siirret��n 85 rivi� yl�sp�in\n\n",
	     (printer->page_h < 600 ? 2 : 10));

    printer->ps_state |= PS_PAGEHDR;
    printer->line = printer->mgn_top;		/* Move onto top of page */

    /* ps_outline (printer->FileDs); */

    fprintf (printer->FileDs, "10 0 translate	%% merkki reunasta\n");

}


/*
 * Paper sizes:
 *  A4    595 x 842  pts
 * Legal  612 x 1008 pts
 * Letter 612 x 792  pts
 *
 * Font Sizes
 *  Pica	10 cpi   80 cpl   fontsize 7.2
 *  Elite	12 cpi   96 cpl   fontsize 6
 *  Cond. Pica	17 cpi  137 cpl
 *  Cond. Elite	20 cpi  160 cpl
 *
 */




static int  ps_outline (FILE *out)
{
    /* Draw page outline */

    return (fprintf(out, "\
0 setlinewidth\n\
    0 10  moveto  0 822 lineto\n\
  576 822 lineto 576 10 lineto\n\
closepath stroke\n\n"));
}


int  test_ps (FILE *out)
{

    /* test fonts */

    fprintf(out, "\
0 setlinewidth\n\
 20  20 moveto  20 822 lineto\n\
576 822 lineto 576  20 lineto\n\
closepath stroke\n\
 20 20  moveto 576 822 lineto stroke\n\
576 20  moveto  20 822 lineto stroke\n\
\n\
20 540 moveto (12345  PetLP test printing.  012345678901  PetLP test printing.  67890123) show\n\
72 500 moveto (The tendency of the best) show\n\
72 485 moveto (typography has been and) show\n\
72 470 moveto (still should be in the path of) show\n\
72 455 moveto (simplicity, legibility, and) show\n\
72 440 moveto (orderly arrangement.) show\n\n\
/MPSfont findfont 8 scalefont setfont\n\
72 420 moveto (Theodore low de vinne) show\n\
\n\
%% change fonts\n\
\n\
currentfont begin /Commodore ChangeCharset end\n\
72 380 moveto (test Commodore #$@ [\\\\]^_  {|}~) show\n\
currentfont begin /US-ASCII ChangeCharset end\n\
72 360 moveto (test US-ASCII  #$@ [\\\\]^_  {|}~) show\n\
currentfont begin /Germany ChangeCharset end\n\
72 340 moveto (test Germany   #$@ [\\\\]^_  {|}~) show\n\
currentfont begin /Denmark1 ChangeCharset end\n\
72 320 moveto (test Denmark1  #$@ [\\\\]^_  {|}~) show\n\
currentfont begin /France ChangeCharset end\n\
72 300 moveto (test France    #$@ [\\\\]^_  {|}~) show\n\
currentfont begin /Sweden1 ChangeCharset end\n\
72 280 moveto (test Sweden1   #$@ [\\\\]^_  {|}~) show\n\
currentfont begin /Italy ChangeCharset end\n\
72 260 moveto (test Italy     #$@ [\\\\]^_  {|}~) show\n\
currentfont begin /Spain ChangeCharset end\n\
72 240 moveto (test Spain     #$@ [\\\\]^_  {|}~) show\n\
currentfont begin /Denmark2 ChangeCharset end\n\
72 220 moveto (test Denmark2  #$@ [\\\\]^_  {|}~) show\n\
currentfont begin /Sweden2 ChangeCharset end\n\
72 200 moveto (test Sweden2   #$@ [\\\\]^_  {|}~) show\n\
%%currentfont begin /UK-ASCII ChangeCharset end\n\
%%72 180 moveto (test UK-ASCII  #$@ [\\\\]^_  {|}~) show\n\
\n\
showpage\n");

/*
  drawps(out, 50, 70, width, 8, 0, 0, ptr);
*/

    return (0);
}


/* ------------------------------------------------------------------------- */

static void do_header(FILE *out)
{
    time_t tloc;
    time(&tloc);	/* Get time */


    /* Create the PostScript(R) Header */

    fprintf (out, "\
%%!PS-Adobe-2.0\n\
%%%%Title: PetLP \n\
%%%%Creator: Project X64 with Printer emulation code generator.\n\
%%%%CreationDate: %s\
%%%%BoundingBox: 20 20 596 822\n\
%%\n%% Matrix Printer Emulator\n\
%%   Copyright (C) 1995 Jouko Valta and Hannu Helminen.\n\
%%   All rights reserved.\n%%\n\
%% Bitmap Font Building\n\
%%   Copyright (C) 1985 Adobe Systems Incorporated.\n\
%%   All rights reserved.\n\
%%\n\
%%%%EndComments\n\n\
/chbuf 1 string def		%% buffer one byte for Bar Drawing\n\
/grafbuf 2000 string def	%% buffer for Graphics Data\n\
/buf %d string def		%% buffer to hold Image String\n",
	    ctime(&tloc), 7);	/* 7 = height */

    fprintf (out, "/P67 {\n\
  6 7 1 [6 0 0 7 0 0] {currentfile buf readhexstring pop} image\n\
  1 0 translate			%% merkki eteenp{in\n\
} bind def\n\n");

    fprintf (out, "\
/RVS {\n  {neg 1 add} settransfer\n} bind def\n\
/RVSOFF {\n  {} settransfer\n} bind def\n\n");

}


/* ------------------------------------------------------------------------- */

static void do_fontdata (FILE *out, int offset, int width, int height)
{
    int     j;
    printerfont7 *cp;
    /*char   *p;*/

#if 0
    if (height == 7)
	cp = &pset[offset];
    else
#endif
	cp = &pset[offset];

/*                         0  1 2  3 4   5 6  7  8  9
 *                       width  b.box   size  x,y transl data
 */
	fprintf(out, "\t[1  0 %s 1 1  %d %d  0 %d <",
		cp->descender ? "-.2" : "0", width, height, cp->descender);

	for (j = height-1; j >= 0; j--) {
	    fprintf(out, "%02x", (cp->data[j] << 2) & 0xFF);
	}
	fprintf(out, "> ] def\n");
}


/*
 * This routine is called to estabilish Encoding vector on the printer for
 * the impossible mission to allow printing in different charsets.
 */

int definefont(FILE *out, int petcii, int csets, int lang, int dinmode)
{
#define NUM_ICHARS  37

    int   i, j;
    int width  = 6;
    int height = 7;
    int count  = 160 + NUM_ICHARS;


    char *punctnames[] = {
	"space", "exclam", "quotedbl", "hash", "dollar", "percent",
	"ampersand", "quoteright", "parentleft", "parentright", "asterisk",
	"plus", "comma", "minus", "period", "slash",
	"0", "1", "2", "3", "4", "5", "6", "7",
	"8", "9", "colon", "semicolon", "less", "equal", "greater", "question",
	"at"
    };

    char *hipunctnames[] = {
	"sqopen", "sterling", "sqclose", "uparrow", "leftarrow",
	"shplus", "cbmminus", "shminus", "shuparr", "cbmstar"
    };


    char *ICharNames[] = {
	"AE",		"Adieresis",	"Aring",
	"Eacute",	"Ntilde",	"Odieresis",
	"Oslash",	"Udieresis",	"adieresis",
	"ae",		"agrave",	"aring",
	"ccedilla",	"eacute",	"egrave",
	"germandbls",	"igrave",	"ntilde",
	"odieresis",	"ograve",	"oslash",
	"udieresis",	"ugrave",	"Pt",
	"currency",	"dieresis",	"exclamdown",
	"questiondown",	"ring",		"section",
	"backslash",	"bar",		"caret",
	"underscore",	"copen",	"cclose",
	"tilde"
    };


    char *InterNames[][11] = {
	{"Commodore",	"hash", "dollar", "at", "sqopen", "sterling",
	     "sqclose", "copen", "bar", "cclose", "tilde" },


	      /* shplus", "cbmminus", "shminus", "shuparr", "cbmstar" }, */ 


	{"US-ASCII",	"hash", "dollar", "at", "sqopen", "backslash", "sqclose",
			"copen", "bar", "cclose", "tilde" },
	{"Germany",	"hash", "dollar", "section", "Adieresis", "Odieresis", "Udieresis",
			"adieresis", "odieresis", "udieresis", "germandbls" },
	{"Denmark1",	"hash", "dollar", "at", "ae", "oslash", "aring",
			"AE", "Oslash", "Aring", "tilde" },
	{"France",	"hash", "dollar", "agrave", "ring", "ccedilla", "section",
			"eacute", "ugrave", "egrave", "dieresis" },
	{"Sweden1",	"hash", "currency", "Eacute", "adieresis", "odieresis", "aring",
			"Adieresis", "Odieresis", "Aring", "udieresis" },
	{"Italy",	"hash", "dollar", "at", "ring", "backslash", "eacute",
			"agrave", "ograve", "egrave", "igrave" },
	{"Spain",	"Pt", "dollar", "at", "exclamdown", "Ntilde", "questiondown",
			"dieresis", "ntilde", "cclose", "tilde" },
	{"Denmark2",	"hash", "dollar", "at", "AE", "Oslash", "Aring",
			"ae", "oslash", "aring", "tilde" },
	{"Sweden2",	"hash", "currency", "Eacute", "Adieresis", "Odieresis", "Aring",
			"adieresis", "odieresis", "aring", "udieresis" },
	{"Finland",	"hash", "dollar", "at", "Adieresis", "Odieresis", "Aring",
			"adieresis", "odieresis", "aring", "tilde" },
	{"Norway",	"hash", "currency", "Eacute", "AE", "Oslash", "Aring",
			"ae", "oslash", "aring", "udieresis" },
	{"UK-Ascii",	"sterling", "dollar", "at", "sqopen", "backslash", "sqclose",
			"copen", "bar", "cclose", "tilde" },
	{"Commodore-DIN",	"hash", "dollar", "section", "sqopen", "sterling", "sqclose",
			"copen", "bar", "cclose", "tilde" }
    };


#if 0
    char *highDINnames[] = {		/* E0 - FF */
	"space", "", "cbm-z", "cbm-e", "cbm-x", "cbm-q", "cbm-@", "cbm-w",
	"cbm-u","cbm-r", "cbm-", "cbm-", "cbm-", "cbm-", "cbm-", "cbm-",
	"at", "greekmu", "", "", "", "", "", "",
	"", "", "", "", "", "", "germandbls", "greekpi"
    };
#endif


    /*
     * Create PETSCII Encoding Vector
     */

    fprintf(out, "\
9 dict dup begin\n\
  /FontType 3 def\n\
  /FontMatrix [1 0 0 1 0 0] def\n\
  /FontBBox [1 1 -.2 0] def\n\
  /Encoding 256 array def\n\n\
  0 1 255 {Encoding exch /.notdef put} for\n\n");


    /*
     * First, do Ascii. Letters are the other way around in PETCII.
     */

    fprintf(out, "  Encoding\n    dup 32 [");

    for (i = 0; i <= 0x20; i++)		/* 20 - 40 */
	fprintf(out, "/%s", punctnames[i]);

    fprintf(out, " ] putinterval\n\
    dup %d [ /a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z ] putinterval\n\
    dup %d [ /A/B/C/D/E/F/G/H/I/J/K/L/M/N/O/P/Q/R/S/T/U/V/W/X/Y/Z ] putinterval\n\n",
	    (petcii ? 65 : 97),
	    (petcii ? 97 : 65));


    /*
     * ...then the rest of alphabetics section...
     * US-ASCII is the standard ASCII version.
     */

    fprintf(out, "\
    dup  91 [ /%s /%s /%s /uparrow /leftarrow ] putinterval\n\
    dup 123 [ /%s /%s /%s /%s /shuparr ] putinterval\n\n",
	    InterNames[lang][4],
	    InterNames[lang][5],
	    InterNames[lang][6],

	    InterNames[lang][7],
	    InterNames[lang][8],
	    InterNames[lang][9],
	    InterNames[lang][10]);
    /* ( petcii ? "/shuparr" : "/tilde" ) */


    if (csets) {		/* This makes sure they are created ... */
	if (petcii) {
	    for (i = 0; i < NUM_ICHARS; i++) {
		fprintf(out, "    dup %d /%s put\n", i + 0xc0, ICharNames[i]);
	    }
	    fprintf(out, "\n");
	}
	else {

	/*
	 * ISO Latin/1 Encoding
	 * Pt is translated to code 160, which is not it's real location.
	 * This allocates so much space that no continuous area for
	 * graphic codes is left.
	 */

	    fprintf(out, "\
    dup 152 /dieresis  put\n\
    dup 160 [ /Pt/exclamdown/.notdef/sterling/currency ] putinterval\n\
    dup 167 /section   put\n\
    dup 168 /dieresis  put\n\
    dup 191 /questiondown put\n\
    dup 196 [ /Adieresis/Aring/AE ] putinterval\n\
    dup 201 /Eacute    put\n\
    dup 209 /Ntilde    put\n\
    dup 214 /Odieresis put\n\
    dup 216 /Oslash    put\n\
    dup 220 /Udieresis put\n\
    dup 223 /germandbls put\n\
    dup 224 /agrave    put\n\
    dup 228 [ /adieresis/aring/ae/ccedilla/egrave/eacute ] putinterval\n\
    dup 236 /igrave    put\n\
    dup 241 /ntilde    put\n\
    dup 242 /ograve    put\n\
    dup 248 /oslash    put\n\
    dup 249 /ugrave    put\n\
    dup 252 /udieresis put\n\n");
	}
    }  /* csets */


    /*
     * ...and finally, the graphics characters.
     */

    if (petcii)
	for (i = 0x61; i <= 0x7a; i++)
	    fprintf(out, "    dup %d /ucs%c put\tdup %d /cbm%c put\n",
		    i + 32, i,  i + 64, i);

    fprintf(out, "    pop\n\n");



    /*
     * Font Building routine from the Blue Book Program 21, on page 223
     * Copyright (C) 1985 Adobe Systems Incorporated.
     */

    fprintf(out, "\
  /BuildChar\n\
    { 0 begin\n\
	/char exch def\n\
	/fontdict exch def\n\
	/charname fontdict /Encoding get char get def\n\
\n\
	/charinfo fontdict /MPSData get charname get def\n\
	/wx charinfo 0 get def			%% width\n\
	/charbbox charinfo 1 4 getinterval def	%% font \n\
	wx 0 charbbox aload pop setcachedevice	%% cache\n\
\n\
	charinfo 5 get charinfo 6 get true\n\
	fontdict /imagemaskmatrix get\n\
	  dup 4 charinfo 7 get put\n\
	  dup 5 charinfo 8 get put\n\
	charinfo 9 1 getinterval cvx\n\
	imagemask\n\
      end\n\
    } def\n\n");


    /* --------------------------------------------------------------------- */

    /*
     * Character Data.
     * Create the font images from Chargen provided.
     */

    fprintf(out, "\
  /BuildChar load 0 6 dict put\n\n\
  /imagemaskmatrix [%d 0 0 %d 0 0] def\n\n\
  /MPSData %d dict def			%% count\n\
  MPSData begin\n", width, height, count);

    for (i = 0; i <= 0x20; i++) {		/* 20 - 40 */
	fprintf(out, "    /%s", punctnames[i]);
	do_fontdata (out, i, width, height);
    }

    for (i = 0x41; i <= 0x5a; i++) {		/* uppercase */
	fprintf(out, "    /%c", i);
	do_fontdata (out, i, width, height);
    }

    for (i = 0x61; i <= 0x7a; i++) {		/* lowercase */
	fprintf(out, "    /%c", i);
	do_fontdata (out, i - 0x40, width, height);
    }


    /*
     * Special CBM character Images.
     */

    fprintf(out, "\n");

    for (i = 0; i < 5; i++) {
	fprintf(out, "    /%s", hipunctnames[i]);
	do_fontdata (out, i + 0x3b, width, height);
    }
    for (i = 0; i < 5; i++) {
	fprintf(out, "    /%s", hipunctnames[i+5]);
	do_fontdata (out, i + 0x5b, width, height);
    }

    fprintf(out, "\n");	


    /*
     * International Character Images.
     */

    if (csets) {
	for (i = 0; i < NUM_ICHARS; i++) {
	    fprintf(out, "    /%s", ICharNames[i]);
	    do_fontdata (out, i + 0xa0, width, height);
	}
	fprintf(out, "\n");
    }


    /*
     * Graphics Character Images.
     */

    for (i = 0x61; i <= 0x7a; i++) {	/* Graphics uppercase - nonstandard location*/
	fprintf(out, "    /ucs%c", i);
	do_fontdata (out, i, width, height);
    }

    for (i = 0x61; i <= 0x7a; i++) {		/* CBM-x Graphics */
	fprintf(out, "    /cbm%c", i);
	do_fontdata (out, i + 0x20, width, height);
    }

    fprintf(out, "\
    /.notdef [0  0 0 0 0  1 0  0 0  <> ] def\n\
  end\n\n");


    /* --------------------------------------------------------------------- */

    /*
     * International Char Sets
     */

    if (csets) {
	fprintf(out, "%%%%--\n\n");

    for (i = 0; i < NUM_LANGUAGES; i++) {
	fprintf(out, "/%s\t [", InterNames[i][0]);

	for (j = 1; j <= 10; j++)
	    fprintf(out, "/%s ", InterNames[i][j]);

	fprintf(out, "] def\n");
    }

	fprintf(out, "\n\
/ChangeCharset {\n\
  currentdict exch get	%%get array from dict\n\
  currentdict begin	%% avataan fonttikirjasto\n\
    dup 0 get Encoding exch 35 exch put\n\
    dup 1 get Encoding exch 36 exch put\n\
    dup 2 get Encoding exch 64 exch put\n\
    dup 3 get Encoding exch 91 exch put\n\
    dup 4 get Encoding exch 92 exch put\n\
    dup 5 get Encoding exch 93 exch put\n\
    dup 6 get Encoding exch 123 exch put\n\
    dup 7 get Encoding exch 124 exch put\n\
    dup 8 get Encoding exch 125 exch put\n\
        9 get Encoding exch 126 exch put\n\
  end\n\
  %% t{h{n viel{ pit{isi saada cachen invalidointi\n\
} bind def\n\n\
%%%%--\n\n");

    }  /* csets */


    fprintf(out, "\
  /UniqueID 2 def\n\
end\n\
/MPSfont exch definefont pop\n\
%%%%EndProlog\n\n");

    return (0);
}



 /*
luullakseni sit{ encoding-taulukkoa ei tarvita lainkaan, ellet v{ltt{m{tt{
halua ... tuonkin voit hoitaa lipulla jonka laitat fonttiin
melkein sanoisin ett{ fonttia on helpoin muuttaa.
Teet siit{ font-diktionaryst{ vain v{h{n isomman kuin mit{ siin{
esimerkiss{ on. sitten vaan sinne sis{lle jotain

/charset 1 def
/upperlower 1 def
 
.. ja sitten siell{ BuildChar?-funktiossa jotain

charset 1 eq { ... if-osuus } { else-osuus } ifelse

ja kun haluat muuttaa noita lippuja fontin ulkopuolelta, se k{y aika helposti,
 {h, jotenkin hankit sen fontin pinoon, esim:

/font-name findfont % jos sulla on nimetty fontti

%sitten

begin /charset 2 def end
*/


/* ------------------------------------------------------------------------- */

/*----------------------+--------------------*-
	PETCII		|  LATIN
------------------------+----------------------
 00			|
 10   ctrl1		|
------------------------+----------------------
 20		    Punctuation
 30		    Numbers
------------------------+----------------------
 40   a-z    | A-Z	|  A-Z
 50  +=======+=========	|  [ \ ] ^ _
 60   A-Z    | Graphics	|  a-z
 70  +-------+---------	|  { | } ~ Del
-------------+----------+----------------------
 80  Shifted Graphics	|
 90  remapped here	|
------------------------|  International
 A0  CBM-x Graphics	|  Characters
 B0			|
------------------------|  (Undefined in ASCII)
 C0  International	|
 D0  remapped here	|
------------------------|
 E0  DIN		|
 F0  (Same as A0 - BF)	|
-*----------------------+--------------------*/


/*
 * Special Coding to allow Graphics Characters
 */

static BYTE extens[][11] = {	/* graphics/uppercase -- uppercase/lowercase */
  { 0x23, 0x24, 0x40, 0x5B, 0x5C, 0x5D,  0xE2, 0xDF, 0xE3, 0xE4 },  /* CBM  */
  { 0x23, 0x24, 0x40, 0x5B, 0xDE, 0x5D,  0xE2, 0xDF, 0xE3, 0xE4 },  /* US   */
  { 0x23, 0x24, 0xDD, 0xC1, 0xC5, 0xC7,  0xC8, 0xD2, 0xD5, 0xCF },  /* GER  */
  { 0x23, 0x24, 0x40, 0xC9, 0xD4, 0xCB,  0xC0, 0xC6, 0xC2, 0xE4 },  /* DEN1 */
  { 0x23, 0x24, 0xCA, 0xDC, 0xCC, 0xDD,  0xCD, 0xD6, 0xCE, 0xD9 },  /* FRA  */
  { 0x23, 0xD8, 0xC3, 0xC8, 0xD2, 0xCB,  0xC1, 0xC5, 0xC2, 0xD5 },  /* SWE1 */
  { 0x23, 0x24, 0x40, 0xDC, 0xDE, 0xCD,  0xCA, 0xD3, 0xCE, 0xD0 },  /* ITA  */
  { 0xD7, 0x24, 0x40, 0xDA, 0xC4, 0xDB,  0xD9, 0xD1, 0xE3, 0xE4 },  /* SPA  */
  { 0x23, 0x24, 0x40, 0xC0, 0xC6, 0xC2,  0xC9, 0xD4, 0xCB, 0xE4 },  /* DEN2 */
  { 0x23, 0xD8, 0xC3, 0xC1, 0xC5, 0xC2,  0xC8, 0xD2, 0xCB, 0xD5 },  /* SWE2 */
  { 0x23, 0x24, 0x40, 0xC1, 0xC5, 0xC2,  0xC8, 0xD2, 0xCB, 0xE4 },  /* FIN  */
  { 0x23, 0xD8, 0xC3, 0xC0, 0xC6, 0xC2,  0xC9, 0xD4, 0xCB, 0xD5 },  /* NOR  */
  { 0x5C, 0x24, 0x40, 0x5B, 0xDE, 0x5D,  0xE2, 0xDF, 0xE3, 0xE4 },  /* UK   */
  { 0x23, 0x24, 0xDD, 0x5B, 0x5C, 0x5D,  0xE2, 0xDF, 0xE3, 0xE4 }   /* C-DIN */
};


/*
 * This is almost identical to the p_toichar() routine, but both graphics
 * and international characters are mapped differently. See the table above.
 */

int  to_psvect(int lang, int lwcase, BYTE c)
{

    /* Duplicate codes  C0 - FF */

    if (c >= 0xc0 && c <= 0xdf)	/* Same as 96 - 127 */
	c ^= 0xa0;

    else if (c == 255)
	c = 126;


    /* CBM-x Graphics Images  A0 - BF, E0 - FE */

    else if (c >= 0xE0) {		/* C0 -- FF is handled now */
	c &= 0xBF;
    }


    /* Numbers, etc.
     * Stray ctrl1 and ctrl2 codes will go here.
     */

    if (c < 0x40 || c >= 0x80)
	return c;


    /* @ / Eacute / Section */

    if (c == 0x40)
	return (extens[lang][2]);

    else if (c == 0x60) {
	/* Standard mode: vertical bar; all others: left quote (`) */

    }


    /* Alphabetics ... everything else has gone */

    if ((c >= 0x41) && (c <= 0x5a)) {
	if (lwcase)
	    c ^= 0x20;
    }
    else if ((c >= 0x5b) && (c <= 0x5d)) {
	if (!lwcase && (lang == CS_DEN1 || lang == CS_SWE1))
	    c = extens[lang][c - 0x5b + 6];
	else
	    c = extens[lang][c - 0x58];
    }
    else if ((c >= 0x61) && (c <= 0x7a)) {
	if (lwcase)
	    c ^= 0x20;
	else
	    c += 0x20;		/* graphics 1 images */
    }
    else if ((c >= 0x7b) && (c <= 0x7e)) {
	c = extens[lang][c - 0x7b + 6];
    }

    return (c);
}


/* ------------------------------------------------------------------------- */

/*
 * The string does not contain control or command characters,
 * but plain text only.
 *
 *   printf(), fprintf(), and  sprintf()  return  the  number  of
 *   characters  transmitted,  or  return  a negative value if an
 *   error was encountered.
 *
 */

int writeps(PRINTER *printer, int width, int height, BYTE *string)
{
    BYTE *p;
    int   col, n = 0;
    int   string_length;


    if (!string)
	return (0);

/*fprintf(stderr, "writeps %02x\n", printer->ps_state);*/

    string_length = (int)strlen((char *)string);
    if (string_length > 1
	&& string_length <= printer->mgn_right - printer->mgn_left
	&& printer->column + string_length > printer->mgn_right) {
	do_newline(printer, 1);
    }

    for (col = printer->column, p = string; *p && n >= 0; col++, p++) {
	if ((printer->page_h - printer->mgn_top
	     - (printer->line / 3)) <= 10) {
	    if (printer->ps_state & PS_SHOW)
		fprintf (printer->FileDs, ") show\n");

	    /*if (printer->ps_state & PS_DRAW)
		end_graphics();*/

	    fprintf (printer->FileDs, "showpage\n");
	    printer->ps_state &= ~(PS_SHOW | PS_DRAW | PS_PAGEHDR);
	}

	if (!(printer->ps_state & PS_SHOW)) {
	    if (!(printer->ps_state & PS_PAGEHDR))
		pr_init_page(printer);

	    printer->ps_state |= PS_SHOW;
	    fprintf (printer->FileDs, "%d %d moveto (",
		     col * 7,
		     printer->page_h - printer->mgn_top - (printer->line / 3));
	}

	if (*p == '(' || *p == ')' || *p == '{' || *p == '}' ||
	    *p == '[' || *p == ']' || *p == '\\')
	   fputc ('\\', printer->FileDs);	/* Escape special chars */

	if (*p != '\n') fputc (*p, printer->FileDs);


	/* do newline */

	if (*p == '\n' || col >= printer->mgn_right) {
	    do_newline(printer, 1);
	    col = printer->mgn_left;
	}  /* CR */
    }  /* for */

    printer->column = col;
    return (col);
}


/* ------------------------------------------------------------------------- */

/*
 * Graphics
 */

/*
90 asteen k{{nt| ja koon vaihto n{kyy menev{n jotenkin n{in:
/n 1920 def
/buf n string def %% t{m{ sitten voipi sy|d{ muistia jos n{it{ varaillaan
		  %% joka v{liss{

7 n 1 [0 n 7 0  0 0] {currentfile buf readhexstring pop} image
08 ff 55	  %% ja n{it{ on n kappaletta

Ja itse asiassa nuo 4 ekaa muodostaa sen rotaatiomatriisin ja loput
kaksi translaatiovektorin.
*/

/* ------------------------------------------------------------------------- */

int  drawps(FILE *out, int x, int y, int width, int height, int cpi, int a, BYTE *ptr)
{
    int   cnt;
    int scale = 1;

    fprintf(out, "%d %d 1 [%d 0 0 %d  %d %d] ",
	    height, width, width * scale, height, x, y);
    fprintf(out,
	    (PSLevel <2) ? "{currentfile chbuf readhexstring pop} image " : "{<");


    for (cnt = width; cnt >= 0; cnt--, ptr++)
	fprintf (out, "%02x", (*ptr & 0xFF));

    if (PSLevel >= 2)
	fprintf(out, ">} image");

    fputc ('\n', out);
    return 0;
}
		/* return bar length (in character widths) */


int drawbar(FILE *out, int width, int height, BYTE value)
{

    /* Draw a Graphics Bar */

    fprintf(out, "%d %d 1 [0 %d %d 0  0 0] ", height, width, width, height);
    fprintf(out,
	    (PSLevel <2) ? "{currentfile chbuf readhexstring pop} image %02x\n"
	    : "{<%02x>} image\n", value);

    return 0;
}


/* ------------------------------------------------------------------------- */

/*
 * Special Effects
 */

void pr_set_reverse(FILE *out, int t)
{
    fprintf (out, t ? "RVS\n" : "RVSOFF\n");
}


void pr_set_size(FILE *out, int t)
{
    /* 0 = normal size, 1 = double width, 2 = double height, 3 = both */

    fprintf (out, t ? "RVS\n" : "RVSOFF\n");
}


void pr_set_colour(FILE *out, int c)
{
    static float rgb[][3] = {
	/* BLACK RED  BLUE VIOLET YELLOW ORANGE GREEN */
	{ .1, .1, .1}, { 1, 0, 0}, { 0, 0, 1}, { 1, 0, 1},
	{ 1, 1, 0}, { 1, .6, 0}, { 0, .8, 0}
    };
 

    if (c >= 0 && c <= 7)   /* P_MAX_COLOUR */
	fprintf (out, "%f %f %f setrgbcolor\n", rgb[c][0], rgb[c][1], rgb[c][2]);
}


void pr_set_font(FILE *out, int t)
{

}


void pr_set_lf(FILE *out, int n)
{

}
