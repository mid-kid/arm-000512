
/* Trivial program to flash an led on PORT B bit 15,
   with some places to insert breakpoints and watchpoints
   to demonstrate GDB
*/

#define PBIOR (*(volatile short int *)(0x5ffffc6))
#define PBCR1 (*(volatile short int *)(0x5ffffcc))
#define PBCR2 (*(volatile short int *)(0x5ffffce))
#define PBDR  (*(volatile short int *)(0x5ffffc2))

int delay = 10;
int count = 0;

main()
{
  int i;
  int j = 0;
  PBIOR |= 0x8000;
  PBCR1 &= ~0xc000;

  while (1) 
    {
      somewhere (j++);
      for (i = 0; i < 10; i++) 
	{
	  /* Toggle the light */
	  PBDR ^= 0x8000;
	  sleep (delay);
	}
    }
}

dummy()
{
}


somewhere (x)
{
  count += x;
}

sleep (delay)
{
  int i;
  int j;
  for (j = 0; j < 1000; j++)
    for (i = 0; i < delay; i++)
      dummy();
}

