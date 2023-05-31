Changes for SYM.C by Andreas Scherer, January 19, 1995.

@x l.43
struct hash_entry *findsym();
@y
struct hash_entry *findsym(register char[], hash_table, int);
char *copy_string(register char *);
Char *copy_unsigned_string(register Char *);
@z

@x l.51
int addsym( sym, str_def, int_def, table, table_size )
register char sym[];
char *str_def;
int int_def;
hash_table table;
int table_size;
@y
int addsym(
register char sym[],
char *str_def,
int int_def,
hash_table table,
int table_size)
@z

@x l.101
void cclinstal( ccltxt, cclnum )
Char ccltxt[];
int cclnum;
@y
void cclinstal(
Char ccltxt[],
int cclnum)
@z

@x l.108
	Char *copy_unsigned_string();
@y
@z

@x l.121
int ccllookup( ccltxt )
Char ccltxt[];
@y
int ccllookup(
Char ccltxt[])
@z

@x l.130
struct hash_entry *findsym( sym, table, table_size )
register char sym[];
hash_table table;
int table_size;
@y
struct hash_entry *findsym(
register char sym[],
hash_table table,
int table_size)
@z

@x l.156
int hashfunct( str, hash_size )
register char str[];
int hash_size;
@y
int hashfunct(
register char str[],
int hash_size)
@z

@x l.178
void ndinstal( name, definition )
char name[];
Char definition[];
@y
void ndinstal(
char name[],
Char definition[])
@z

@x l.182
	char *copy_string();
	Char *copy_unsigned_string();
@y
@z

@x l.197
Char *ndlookup( nd )
char nd[];
@y
Char *ndlookup(
char nd[])
@z

@x l.206
void scextend()
@y
void scextend(void)
@z

@x l.227
void scinstal( str, xcluflg )
char str[];
int xcluflg;
@y
void scinstal(
char str[],
int xcluflg)
@z

@x l.231
	char *copy_string();
@y
@z

@x l.258
int sclookup( str )
char str[];
@y
int sclookup(
char str[])
@z
