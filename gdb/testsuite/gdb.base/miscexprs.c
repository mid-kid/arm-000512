void marker1 ()
{
    
}
/*
static struct {
    char c[100];
} cbig;


static struct {
    int i[800];
} ibig;


static struct {
    long l[900];
} lbig;

static struct {
    float f[200];
} fbig;

static struct {
    double d[300];
} dbig;

static struct {
    short s[400];
} sbig;*/

/*struct {
    long l[900];
} lbig;*/

int main()
{

struct {
    char c[100];
} cbig;


struct {
    int i[800];
} ibig;


struct {
    long l[900];
} lbig;

struct {
    float f[200];
} fbig;

struct {
    double d[300];
} dbig;

struct {
    short s[400];
} sbig; 
    ibig.i[100] = 5;
    cbig.c[100] = 'A';
    fbig.f[100] = 11.99999;
    dbig.d[202] = 9.99999999;
    sbig.s[90] = 255;
    lbig.l[333] = 999999999;
    
    #ifdef usestubs
       set_debug_traps();
       breakpoint();
    #endif
    marker1();
    return 0;

}























































































