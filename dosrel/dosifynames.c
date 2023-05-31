#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
/* Turn provided name into dos equivalent */

int
main(ac, av)
int ac;
char **av;
{
  char *p;
  char *copy;
  char *filename;
  int opt_move = 0;
  int i;
  char *name;

  for (i =1;  i < ac; i++) {
    if (av[i][0] == '-'
	&& av[i][1] == 'm')
      opt_move = 1;
    else
      name = av[i];
  }

  if (name == 0) {
    puts("!badname!");
  }
  else {
    copy = malloc (strlen (name) + 1);
    strcpy (copy, name);
    /* Go to the end and work outwhere the 8.3 bit starts */
    filename = strrchr (copy, '/');
    if (!filename)
      filename = copy;
    else
      filename ++;

    /* If the first char of the filename is uppercase, insert
       an underscore at the front */

    if (isupper(filename[0])) {
      char *c = malloc (strlen (name) + 2);
      char *s = copy;
      char *d = c;
      while (s != filename) {
	*d++ = *s;
      }
      *d++ = '_';
      while (*s)
	{
	  *d++ = *s++;
	}
      free(copy);
      copy = c;
    }

    /* Turn any + signs into x es */

    for (p = filename; *p ; p++)
      if (*p == '+')
	*p = 'x';

    /* Make sure it's only an 8.3 name */
    {
      char *dst;
      char *src;
      int before;
      int after;
      src = filename;
      dst = filename;
      before = 0;
      while (*src && *src != '.' && before < 8) 
	{
	  before++;
	  *dst++  = tolower(*src++);
	}
      while (*src && *src != '.')
	src++;

      if (*src == '.')
	{ *dst++ = *src++; }

      after = 0;
      while (*src && *src != '.' && after <3) 
	{
	  after++;
	  *dst++  = tolower(*src++);
	}
      *dst++ = 0;
    }

    if (opt_move) {
      int total = strlen (name) + strlen (copy) + 4;
      char *c = malloc(total);
      if (strcmp(name, copy)) {
	sprintf(c,"mv %s %s\n", name, copy);
	system(c);

      }
    }
    else {
      puts(copy);
    }
  }
  return 0;
}
