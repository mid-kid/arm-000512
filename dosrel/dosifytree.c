#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <dirent.h>
#define _S_IFDIR S_IFDIR
#define mymkdir(x) mkdir(x,666)
#define SLASH '/'


#include <errno.h>

#define MAXDEPTH 20
#define MAXPATH 1000

typedef struct node_
  {
    int type;
    char *name;			/* just the filename */
    char *pathname;		/* all the name from the root upto
					 * and including the filename */
    int depth;
    struct node_ *next;
    struct node_ *child;
    struct node_ *parent;	/* The one above */
    int disknum;
    int diskallocated;
  }
node;


char *root;

char
tlate (x)
     char x;
{
  x = tolower (x);
  if (x == '+')
    x = 'x';
  return x;
}

char *
mangle (buf, in)
     char *buf;
     char *in;
{
  char *src;
  int before;
  int after;
  char *dst = buf;
  /*
	 * These are done manually in the install script, because we may need
	 * to add some info to header.gcc if (isupper (in[0])) { dst++ = '_';
	 * }
	 */

  src = in;

  before = 0;
  while (*src && *src != '.' && before < 8)
    {
      before++;
      *dst++ = tlate (*src++);
    }
  while (*src && *src != '.')
    src++;

  if (*src == '.')
    {
      *dst++ = *src++;
    }
  after = 0;
  while (*src && *src != '.' && after < 3)
    {
      after++;
      *dst++ = tlate (*src++);
    }
  *dst++ = 0;

  if (strcmp (buf, in))
    return buf;
  return 0;

}

node *
build_tree (root, partialroot, depth, parent)
     char *root;
     char *partialroot;
     int depth;
     node *parent;
{
  /* Passed a direcotry, build node list */
  DIR *dirp = opendir (root);
  struct dirent *p;
  node *tail = 0;
  node *head = 0;
  for (p = readdir (dirp); p; p = readdir (dirp))
    {
      struct stat buf;
      char f[MAXPATH];
      char f1[200];


      sprintf (f, "%s%c%s", root, SLASH, p->d_name);
      sprintf (f1, "%s%c%s", partialroot, SLASH, p->d_name);

      stat (f, &buf);

      if (p->d_name[0] != '.')
	{
	  node *n = (node *) (malloc (sizeof (node)));
	  n->name = strdup (p->d_name);
	  n->pathname = strdup (f1);
	  n->next = 0;
	  n->depth = depth;
	  n->diskallocated = 0;
	  n->disknum = 0;
	  n->parent = parent;
	  if (buf.st_mode & _S_IFDIR)
	    {
	      n->type = 'd';
	      n->child = build_tree (f, f1, depth + 1, n);
	    }
	  else
	    {
	      n->type = 'f';
	      n->child = 0;
	    }
	  if (tail)
	    tail->next = n;
	  else
	    head = n;
	  tail = n;
	}
    }
  closedir (dirp);
  return head;
}

/*
 * Turn a node into a name by walking up the tree and working it out - the
 * pathname can become obsolete if renaming has happende
 */

char *
getname (path, x)
     char *path;
     node *x;
{
  int len = 0;
  node *parents[MAXDEPTH];
  int depth = 0;
  node *ptr;
  int i;
  char *dst;
  for (ptr = x; ptr; ptr = ptr->parent)
    {
      parents[depth++] = ptr;
    }

  /* Walk down again and fill in the name */

  dst = path;
  for (i = depth - 1; i >= 0; i--)
    {
      int len = strlen (parents[i]->name);
      *dst++ = '/';
      memcpy (dst, parents[i]->name, len);
      dst += len;
    }
  *dst++ = 0;
  return path;
}

void
dump (d, x)
     int d;
     node *x;
{
  char tmp[MAXPATH];
  while (x)
    {
      int i;

      for (i = 0; i < d * 4; i++)
	printf ("*");
      printf ("%s %s %s\n", x->pathname, x->name, getname (tmp, x));


      dump (d + 1, x->child);
      x = x->next;
    }
}

massage_tree (x)
     node *x;
{
  while (x)
    {
      char t3[MAXPATH];
      char *newname = mangle (t3, x->name);
      if (newname)
	{
	  char t1[MAXPATH];
	  char t2[MAXPATH];
	  getname (t1, x);
	  x->name = strdup (newname);
	  getname (t2, x);
	  printf ("mv %s%s %s%s\n", root, t1, root, t2);
	}
      massage_tree (x->child);
      x = x->next;
    }
}

main (ac, av)
     int ac;
     char **av;
{
  node *tree;
  root = av[1];
  tree = build_tree (av[1], "", 0, 0);
  massage_tree (tree);
}
