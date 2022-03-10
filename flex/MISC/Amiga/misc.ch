Changes for MISC.C by Andreas Scherer, January 19, 1995.

@x l.166
Char clower( c )
register int c;
@y
Char clower(
register int c)
@z

@x l.175
char *copy_string( str )
register char *str;
@y
char *copy_string(
register char *str)
@z

@x l.201
Char *copy_unsigned_string( str )
register Char *str;
@y
Char *copy_unsigned_string(
register Char *str)
@z

@x l.351
int htoi( str )
Char str[];
@y
int htoi(
Char str[])
@z

@x l.358
	return result;
@y
	return (int)result;
@z

@x l.366
int is_hex_digit( ch )
int ch;
@y
int is_hex_digit(
int ch)
@z

@x l.500
Char myesc( array )
Char array[];
@y
Char myesc(
Char array[])
@z

@x l.588
	return result;
@y
	return (int)result;
@z
