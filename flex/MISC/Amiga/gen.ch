Changes for GEN.C by Andreas Scherer, January 19, 1995.

@x l.39
void indent_puts PROTO((char []));

@y
void indent_puts PROTO((char []));
Char clower PROTO((register int));
@z

@x l.62
void do_indent()
@y
void do_indent(void)
@z

@x l.82
void gen_backing_up()
@y
void gen_backing_up(void)
@z

@x l.103
void gen_bu_action()
@y
void gen_bu_action(void)
@z

@x l.132
void genctbl()
@y
void genctbl(void)
@z

@x l.231
	Char clower();
@y
@z

@x l.272
void gen_find_action()
@y
void gen_find_action(void)
@z

@x l.394
void genftbl()
@y
void genftbl(void)
@z

@x l.428
void gen_next_compressed_state( char_map )
char *char_map;
@y
void gen_next_compressed_state(
char *char_map)
@z

@x l.474
void gen_next_match()
@y
void gen_next_match(void)
@z

@x l.649
void gen_NUL_trans()
@y
void gen_NUL_trans(void)
@z

@x l.723
void gen_start_state()
@y
void gen_start_state(void)
@z

@x l.754
void gentabs()
@y
void gentabs(void)
@z

@x l.762
	static char C_char_decl[] =
		"static const YY_CHAR %s[%d] =\n    {   0,\n";	/* } for vi */
@y
@z
