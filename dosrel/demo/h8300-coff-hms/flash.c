/* This program will flags the LEDs on an H8/300 eval board */


/* Port 6 on the eval card has LEDS wired to it */
#define P6DDR  (*((volatile unsigned char *)0xffb9))  /* Direction register */
#define P6DR   (*((volatile unsigned char *)0xffbb))  /* Data register */
static int speed=20000;

delay()
{
  int j;
  for (j = 0; j < speed; j++)
   asm("nop");
}
main()
{
  char v;
  P6DDR = 0xff;                 /* Set port 6 to output */

  v = 1;

  while (1) 
  {
    P6DR = v;
    v <<= 1;
    if (v == 0) 
    {
      /* Shifted off the end, stick back at start */
      v = 1;
    }
    delay();
  }
}

