// Build don't link: 
// GROUPS passed old-abort
extern "C" void printf (char *, ...);

class A {
	int	i;
	int	j;
    public:
	int	h;
	A() { i=10; j=20; }
	virtual void f1() { printf("i=%d j=%d\n",i,j); }
	friend virtual void f2() { printf("i=%d j=%d\n",i,j); }// ERROR -  virtual.*
};

class B : public A {
    public:
	virtual void f1() { printf("i=%d j=%d\n",i,j); }// ERROR -  member.*// ERROR -  member.*
	friend virtual void f2() { printf("i=%d j=%d\n",i,j); }// ERROR -  virtual.*// ERROR -  member.*// ERROR -  member.*
};

int
main() {
	A * a = new A;
}
