#define DONTDEL 1
#define FAKE 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define UNIX 1

#include <sys/stat.h>

#ifndef UNIX
#include <direct.h>
#include <io.h>
#include <dos.h>
#include "dirent.h"
#define stat _stat
#define mymkdir(x) _mkdir(x)
#define SLASH '\\'
#else
#include <dirent.h>
#define _S_IFDIR S_IFDIR
#define mymkdir(x) mkdir(x, 0777)
#define SLASH '/'
#endif

#include <errno.h>

#define BUFLEN 400		/* twice the max filelen size */
char release_name[100];
char release_number[100];

typedef struct node_
  {
    int type;
    char *name;			/* just the filename */
    char *pathname;		/* all the name from the root upto
				   * and including the filename */
    long int size;
    long int natural_size;	/* Size before compression */
    int depth;
    struct node_ *next;
    struct node_ *child;
    int disknum;
    int diskallocated;
  }
node;




#define MAXDISKS 20
#define DISKSIZE 600000000L	/* CD size is 650M */
long int diskused[MAXDISKS];
int lastdisk;
int node_count;

long int total_size;		/* Uncompressed size of all files */
long int total_small_size;

node **node_vec;

fill_vec (tree, in)
     node *tree;
     int in;
{
  for (; tree; tree = tree->next)
    {
      if (tree->type == 'd')
	{
	  in = fill_vec (tree->child, in);
	}
      else
	{
	  node_vec[in++] = tree;
	}
    }
  return in;
}

int
comp (a, b)
     node **a;
     node **b;
{
     if ((*a)->size > (*b)->size)
          return 1;
     else if ((*a)->size < (*b)->size)
          return -1;

     return 0;
}

void
sort_vec ()
{

  qsort ((void *) node_vec, (size_t) node_count, sizeof (node *), comp);
}

void
dump_vec ()
{
  int i;
  for (i = 0; i < node_count; i++)
    {

      printf ("%d %ld %s\n", node_vec[i]->disknum, node_vec[i]->size, node_vec[i]->name);
    }
}


/* Roundup to the next 1k boundary */
int 
myroundup (x)
{
  return (x + 1023) & ~1023;
}
static void
allocate_to_disk (nd, disk)
     int nd;
     int disk;
{


  node_vec[nd]->disknum = disk;
  node_vec[nd]->diskallocated = 1;
  diskused[disk] += myroundup (node_vec[nd]->size);

  /*
   * printf("on disk %d size %ld %s\n", disk, diskused[disk],
   * node_vec[nd]->name);
   */

}
/*
 * Initializae the packing vector with a few things to make sure that the
 * readme and install scripts turn up on the first disk.
 *
 * All we do is make sure that all files in the toplevl of the release go onto
 * disk1.
 */

void
init_vec ()
{
  int i;
  for (i = 0; i < node_count; i++)
    {
      if (node_vec[i]->depth == 0)
	{
	  allocate_to_disk (i, 0);
	}
    }
  /* We keep the catalog on disk0 too, let's say that it's 20k */
  diskused[0] += 20 * 1024;


}

void
bin_vec ()
{
  int i;
  for (i = 0; i < node_count; i++)
    {
      int j;
      if (!node_vec[i]->diskallocated)
	{
	  for (j = 0; j < MAXDISKS; j++)
	    {
	      /*
	         * Stick this file on the disk if it needs to
	         * be allocated and it will fit.
	       */
	      if (node_vec[i]->size + diskused[j] < DISKSIZE)
		{
		  allocate_to_disk (i, j);

		  break;
		}
	    }
	  if (j > lastdisk)
	    lastdisk = j;
	}
    }
}
void
dumpdirs (tree, f)
     node *tree;
     FILE *f;
{
  for (; tree; tree = tree->next)
    {
      if (tree->type == 'd')
	{
	  fprintf (f, "%d %s\n", tree->depth, tree->pathname);
	  dumpdirs (tree->child, f);
	}
    }

}

void
add_catalog (tree, diskpath)
     node *tree;
     char *diskpath;
{

  char buf[BUFLEN];
  FILE *f;
  int i;
  int disknum;
  sprintf (buf, "%s%cdisk0%cmanifest", diskpath, SLASH, SLASH);
  f = fopen (buf, "w");

  fprintf (f, "** RELEASE MANIFEST **\n");
  fprintf (f, "%s", release_name);
  fprintf (f, "%s", release_number);
  fprintf (f, "Total uncompressed file size %10ld\n", total_size);
  fprintf (f, "Total compressed file size   %10ld\n", total_small_size);
  fprintf (f, "Number of floppies           %10d\n", lastdisk + 1);
  fprintf (f, "\n");
  fprintf (f, "Directory structure:\n");
  dumpdirs (tree, f);
  fprintf (f, "\nFile structure:\n");
  for (disknum = 0; disknum <= lastdisk; disknum++)
    {
      for (i = 0; i < node_count; i++)
	{
	  if (node_vec[i]->disknum == disknum)
	    {
	      char *p;
	      sprintf (buf, "%d %d %s\n", disknum, node_vec[i]->depth, node_vec[i]->pathname);
	      for (p = buf; *p; p++)
		{
		  if (*p == '/')
		    *p = '\\';
		}

	      fprintf (f, buf);
	    }
	}
    }
  fprintf (f, "\n");
  fclose (f);
}


void
pack (tree)
     node *tree;
{
  /* First build a vector of pointers to all the nodes */
  node_vec = (node **) (calloc (node_count, sizeof (node *)));
  node_count = fill_vec (tree, 0);	/* Drop the directory nodes */
  /* Then sort the nodes by size */
  sort_vec ();

  init_vec ();

  /* Now stick the things into bins */
  bin_vec ();


  /* dump_vec(); */
}

node *
build_tree (root, partialroot, depth, size)
     char *root;
     char *partialroot;
     int depth;
     long int *size;
{
  /* Passed a direcotry, build node list */
  DIR *dirp = opendir (root);
  struct dirent *p;
  node *tail = 0;
  node *head = 0;
  for (p = readdir (dirp); p; p = readdir (dirp))
    {
      struct stat buf;
      char *f = malloc (BUFLEN);
      char *f1 = malloc (BUFLEN);


      sprintf (f, "%s%c%s", root, SLASH, p->d_name);
      sprintf (f1, "%s%c%s", partialroot, SLASH, p->d_name);
      printf ("%s\n", f);
      stat (f, &buf);

      if (p->d_name[0] != '.')
	{
	  node *n = (node *) (malloc (sizeof (node)));
	  node_count++;
	  n->name = strdup (p->d_name);
	  n->pathname = strdup (f1);
	  n->next = 0;
	  n->depth = depth;
	  n->diskallocated = 0;
	  n->disknum = -1;
	  if (buf.st_mode & _S_IFDIR)
	    {
	      n->type = 'd';
	      n->size = 999;
	      n->child = build_tree (f, f1, depth + 1, size);
	    }
	  else
	    {
	      n->type = 'f';
	      n->size = buf.st_size;
	      n->child = 0;
	    }
	  *size += n->size;
	  if (tail)
	    tail->next = n;
	  else
	    head = n;
	  tail = n;
	  free (f);
	  free (f1);
	}
    }
  closedir (dirp);
  return head;
}

void
dump (d, x)
     int d;
     node *x;
{
  while (x)
    {
      int i;
      for (i = 0; i < d * 4; i++)
	printf ("*");
      printf ("%6ld %s %s\n", x->size, x->pathname, x->name);
      dump (d + 1, x->child);
      x = x->next;
    }


}
void
zip (src, dst, tree)
     char *dst;
     char *src;
     node *tree;
{
  char *dstbuf = malloc (BUFLEN);
  for (; tree; tree = tree->next)
    {

      if (tree->type == 'd')
	{
	  sprintf (dstbuf, "%s%s", dst, tree->pathname);
	  if (mymkdir (dstbuf) != 0 && errno != EEXIST)
	    {
	      fprintf (stderr, "Can't make directory %s\n", dstbuf);
	    }
	  zip (src, dst, tree->child);
	}
      else
	{
	  if (tree->depth == 0 || FAKE)
	    {
	      sprintf (dstbuf, "cp %s%s %s%s", src, tree->pathname, dst, tree->pathname);
	      system (dstbuf);
	    }
	  else
	    {
	      sprintf (dstbuf, "compress -c -f -b 14 %s%s >%s%s", src, tree->pathname, dst, tree->pathname);
	      system (dstbuf);
	    }

	}

    }
  free (dstbuf);
}
/* Return 1 if there's a child in the disk */

child_in (disk, tree)
     int disk;
     node *tree;
{
  while (tree)
    {
      if (tree->disknum == disk)
	return 1;
      if (child_in (disk, tree->child))
	return 1;
      tree = tree->next;
    }
  return 0;
}
void
movein (dst, src, tree, disk)
     char *dst;
     char *src;
     node *tree;
     int disk;
{
  char *buf = malloc (BUFLEN);
  for (; tree; tree = tree->next)
    {
      if (tree->type == 'd')
	{
	  if (1 || child_in (disk, tree->child))
	    {
	      sprintf (buf, "%s%cdisk%d%s", dst, SLASH, disk, tree->pathname);
	      mymkdir (buf);
	      movein (dst, src, tree->child, disk);
	    }
	}
      else
	{
	  if (tree->disknum == disk)
	    {
	      sprintf (buf, "cp %s%s %s%cdisk%d%s", src, tree->pathname, dst, SLASH, disk, tree->pathname);
	      if (system (buf) != 0)
		{
		  fprintf (stderr, "Can't %s\n", buf);
		  perror (0);
		  exit (99);
		}
	      sprintf (buf, "rm %s%s", src, tree->pathname);
	      if (!DONTDEL)
		{
		  if (system (buf) != 0)
		    {
		      fprintf (stderr, "Can't %s\n", buf);
		      perror (0);
		      exit (99);
		    }
		}
	    }
	}
    }
  free (buf);
}

void
makediskareas (dst, src, tree)
     char *dst;
     char *src;
     node *tree;
{
  char *buf = malloc (BUFLEN);
  int i;
  mymkdir (dst);
  for (i = 0; i <= lastdisk; i++)
    {
      sprintf (buf, "%s%cdisk%d", dst, SLASH, i);
      if (mymkdir (buf) != 0 && errno != EEXIST)
	{
	  fprintf (stderr, "Can't make directory %s\n", buf);

	}
      movein (dst, src, tree, i);
    }
  free (buf);
}

static char buf[BUFLEN];
main (ac, av)
     int ac;
     char **av;
{
  node *tree;
  FILE *f;

#if 0
  char *base = av[1];

  int len = strlen (base) + 10;
  char *root = malloc (len);
  char *comp = malloc (len);
  char *disk = malloc (len);

  sprintf (root, "%s%corig", base, SLASH);
  sprintf (comp, "%s%ccomp", base, SLASH);
  sprintf (disk, "%s%cdisk", base, SLASH);

#else

  char *root = av[1];
  char *comp = av[2];
  char *disk = av[3];



#endif

  sprintf (buf, "%s%crel", root, SLASH);
  f = fopen (buf, "r");

  if (!f)
    {
      fprintf (stderr, "Can't open top level directory rel <%s>\n", buf);
      exit (9);
    }
  fgets (release_name, sizeof (release_name), f);
  fgets (release_number, sizeof (release_number), f);

  fclose (f);
  printf ("RELIT %s %s %s\n", root, release_name, release_number);

  tree = build_tree (root, "", 0, &total_size);

  /* dump(1, tree); */
  mymkdir (comp);

  zip (root, comp, tree);

  tree = build_tree (comp, "", 0, &total_small_size);
  pack (tree);

  makediskareas (disk, comp, tree);
  add_catalog (tree, disk);
  return 0;
}
