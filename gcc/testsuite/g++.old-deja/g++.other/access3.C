// The standard sez that a use of a name gets the most access it can through
// the various paths that can reach it.  Here, the access decl in B gives
// us access.

struct A
{
  void f ();			// gets bogus error - ref below XFAIL *-*-*
};

struct B: private virtual A
{
  A::f;
};

struct C: private virtual A, public B
{
};

int
main ()
{
  C c;

  c.f ();			// gets bogus error - private XFAIL *-*-*
}
