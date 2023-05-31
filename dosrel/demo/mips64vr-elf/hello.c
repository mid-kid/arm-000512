int iprintf (const char *fmt,...);
main()
{
  int i;
  for (i = 0; i < 10; i++)
    iprintf("Hello World %d\n", i);
  return 0;
}
