/*
 * Test program for runtime overlay manager.
 */

#include "ovlymgr.h"

extern int foo();
extern int bar();
extern int baz();
extern int grbx();

int main()
{
  int a, b, c, d, e;

  if (!OverlayLoad(0))
    return -1;
  a = foo(1);
  if (!OverlayLoad(1))
    return -1;
  b = bar(1);
  if (!OverlayLoad(2))
    return -1;
  c = baz(1);
  if (!OverlayLoad(3))
    return -1;
  d = grbx(1);
  e = a + b + c + d;
  return e;
}

