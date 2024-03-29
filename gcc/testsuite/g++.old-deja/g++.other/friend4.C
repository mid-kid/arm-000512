// Build don't link:

// by Alexandre Oliva <oliva@dcc.unicamp.br>

// I'm not 100% sure this program is correct, but g++ shouldn't just
// crash.

// The idea is to give privileged access to bar<A> only to
// specializations foo<A,B>, for all B.

template <class A, class B> void foo();
template <class C> class bar {
  int i;
  template <class B> friend void foo<C,B>(); // ERROR - bogus declaration
};
template <class A, class B> void foo() {
  bar<A> baz; baz.i = 1;   // ERROR - foo cannot access bar<int>::i
  bar<int> buz; buz.i = 1; // ERROR - foo cannot access bar<int>::i
}
int main() {
  foo<void,void>();
  foo<int,void>();
}
