// Build don't link:
// excess errors test - XFAIL *-linux-gnu

char *a="a�";

class A
{
public:
	A()
	{
		char *b="a�";
	}
};

char *c="a�";
