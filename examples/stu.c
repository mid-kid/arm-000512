char comm_char[100];
char char_init[] = "$RCSfile: stu.c,v $$Revision: 1.1 $$Date: 1994/08/05 17:55:41 $\nPatch level: ###\n";

#define printf nothing

nothing () {}

/* __main() {} */

static char static_char[57];

static
func()
{
  static char func2_static_char[72];
  static char func2_static_char_2[72];
  static char func2_static_char_init[] = "Yet another insanely long string that should live in the static space inside of this module\n";
  static char func2_static_char_init_2[] = "Yet another insanely long string that should live in the static space inside of this module 2\n";

  strcpy (comm_char, "A global var in the BSS segment\n");

  strcpy (static_char, "A static var in the BSS segment\n");

  strcpy (func2_static_char, "A function static var in the BSS segment\n");

  printf ("Hello world\n");

  printf ("rcsid = %s\n", char_init);
}

static char static_char_init[] = "A file wide static char array that should live in the data segment of this program.\n";

main()
{
  static char func_static_char[72];
  static char func_static_char_2[72];
  static char func_static_char_init[] = "Yet another insanely long string that should live in the static space inside of this module\n";
  static char func_static_char_init_2[] = "Yet another insanely long string that should live in the static space inside of this module 2\n";

  {
    int foo;

    strcpy (comm_char, "A global var in the BSS segment\n");

    subr ();

    strcpy (static_char, "A static var in the BSS segment\n");
  }

  printf ("%d %d %d %d %d\n",1,2,3,4,5);

  strcpy (func_static_char, "A function static var in the BSS segment\n");

  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5, 6, 7);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);  printf ("%d %d %d %d %d\n", 1, 2, 3, 4, 5);

  printf ("Hello world\n");

  printf ("rcsid = %s\n", char_init);
}

subr()
{
  int x, y = 45;

  x *= x;

  return x + y;
}
