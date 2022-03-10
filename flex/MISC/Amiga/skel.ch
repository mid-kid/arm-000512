Changes for SKEL.C by Andreas Scherer, January 19, 1995.

@x l.1074
  "	b->yy_is_interactive = file ? isatty( fileno(file) ) : 0;",
@y
  "#ifdef _AMIGA",
  "	b->yy_is_interactive = 0;",
  "#else",
  "	b->yy_is_interactive = file ? isatty( fileno(file) ) : 0;",
  "#endif",
@z
