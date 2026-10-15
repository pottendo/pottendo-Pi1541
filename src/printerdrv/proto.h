
extern int  p_topetcii ( int c );
extern int  p_toichar ( int lang, int lwcase, BYTE c );
extern int  set_locale ( char *localename );
extern int  str_to_lang ( char *name );
extern char *ctrl_to_str ( int clean, BYTE c );
extern char *cbm_to_str ( BYTE c );
extern int  p_toascii ( int c, int cs );
extern void init_ps(FILE *out, int petcii, int csets, int lang, int dinmode);

