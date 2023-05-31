char comm_char[100];
char char_init[] = "$RCSfile: testme.c,v $$Revision: 1.1 $$Date: 1996/11/02 05:34:08 $\nPatch level: ###\n";

int printf (const char *fmt,...);
char *strcpy (char *, const char *);

static char static_char[57];
static char static_char_init[] = "A file wide static char array that should live in the data segment of this program.\n";

int
fact (i)
     int i;
{
  if (i == 1)
    return 1;
  else
    return i * fact ( i - 1);
}

main()
{
  int num;
  static char func_static_char[72];
  static char func_static_char_init[] = "Yet another insanely long string that should live in the static space inside of this module\n";
  extern _end, _edata;

  strcpy (comm_char, "A global var in the BSS segment\n");

  strcpy (static_char, "A static var in the BSS segment\n");

  strcpy (func_static_char, "A function static var in the BSS segment\n");

  printf ("Hello world\n");

  num = fact(10);

  printf ("fact(10) = %d\n", num);

  printf ("rcsid = %s\n", char_init);
  printf ("edata = 0x%x, end = 0x%x\n", &_edata, &_end);
  _exit(0);
}

