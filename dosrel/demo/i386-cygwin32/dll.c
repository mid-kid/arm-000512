#include <windows.h>


char *in_a_dll(int a)
{
  switch (a)
    {
    case 0:
      return "Hi Steve\n";
      break;
    case 1:
      return "Hello World\n";
    }
}


WINAPI dll_entry(int a,int b,int c)
{
  return 1;
}
