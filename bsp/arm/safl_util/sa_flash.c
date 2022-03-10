
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define FLASH_SZ (4 * 1024 * 1024)
#define FLASH_BLOCK_SZ (256 * 1024)

volatile void *flash_base;
int  driver_fd;

/* 
 * sync PCI transactions
 */
static void
pci_sync(void)
{
    volatile unsigned int x;

    x = *(unsigned int *)flash_base;
}

static void
flash_write_mode(void)
{
    pci_sync();
    *(volatile int *)flash_base = 0x40404040;
}


static void
flash_normal_mode(void)
{
    pci_sync();
    *(volatile int *)flash_base = 0xffffffff;
}


/*
 *  Check to see if there is some flash at the location specified.
 */
static int
flash_verify(void) 
{
    volatile unsigned int *fp = (volatile unsigned int *)flash_base; 
    unsigned int mfg, id ;

    flash_normal_mode();

    /* read the manufacturer's id. */
    pci_sync();
    *fp = 0x90909090;
    mfg = *fp;

    if (mfg != 0x89898989) {
	flash_normal_mode();
	return 0;
    } 

    id = *(fp + 1) ;

    flash_normal_mode();

    if (id < 0xA1A1A1A1)
	return 0;

    return 1;
}


#if 0 /* NOT USED */
static unsigned int
flash_read_dword(int offset)
{
    /* swap initial 32 byte blocks when accessing flash from PCI */
    if (offset < 32)
	offset += 32;
    else if (offset < 64)
	offset -= 32;

    offset &= ~3;  /* dword alignment */

    return *(volatile unsigned int *)(flash_base + offset);
}
#endif


static int
flash_write_dword(int offset, unsigned int data) 
{ 
    volatile unsigned int *fp; 
    int status;

    /* swap initial 32 byte blocks when accessing flash from PCI */
    if (offset < 32)
	offset += 32;
    else if (offset < 64)
	offset -= 32;

    offset &= ~3;  /* dword alignment */

    fp = (volatile unsigned int *)(flash_base + offset) ;

    flash_write_mode();
  
    /* write the data */
    pci_sync();
    *fp = data;

    /* wait till done */
    do {
	pci_sync();
	*fp = 0x70707070;
	status = *fp;
    } while ((status & 0x80808080) != 0x80808080);
  
    flash_normal_mode();

   /* check for error */
    if ((status & 0x10101010) == 0x10101010)
	return 0;

    return 1;
}


static void
flash_erase_block(int block) 
{
    volatile unsigned int *fp;

    fp = (volatile unsigned int *)(flash_base + (block * FLASH_BLOCK_SZ));

    /* write delete block command followed by confirm */
    pci_sync();
    *fp = 0x20202020;
    pci_sync();
    *fp = 0xd0d0d0d0;

    do {
	pci_sync() ;
	*fp = 0x70707070;
    } while ((*fp & 0x80808080) != 0x80808080) ;

    flash_normal_mode();
}


int
main(int argc, char *argv[])
{
    int in_fd, i, got, offset, extra;
    int buf[256];

    switch (argc) {
      case 1:
	in_fd = STDIN_FILENO;
	break;
      case 2:
	in_fd = open(argv[1], O_RDONLY);
	if (in_fd < 0) {
	    fprintf(stderr, "Can't open %s", argv[1]);
	    perror("");
	    exit(1);
	}
	break;
      default:
	fprintf(stderr, "Usage: sa_flash [filename]\n");
	exit(1);
    }

    driver_fd = open("/dev/safl", O_RDWR);
    if (driver_fd < 0) {
	perror("Can't open device");
	exit (1);
    }

    flash_base = mmap(NULL, FLASH_SZ, PROT_READ|PROT_WRITE,
		      MAP_SHARED, driver_fd, 0);

    if (flash_base == NULL) {
	perror("mmap failed");
	close(driver_fd);
	return 0;
    }

    if (!flash_verify()) {
	fprintf(stderr, "Couldn't find flash.\n");
	exit(1);
    }
    
    flash_erase_block(0);

    extra = offset = 0;
    while ((got = read(in_fd, ((char *)buf) + extra, sizeof(buf) - extra)) > 0) {
	got += extra;

	extra = got & 3;
	got /= 4;
	for (i = 0; i < got; ++i, offset += 4)
	    flash_write_dword(offset, buf[i]);

	if (extra)
	    buf[0] = buf[i];

	printf("*"); fflush(stdout);
    }
    printf("\n");

    if (extra)
	flash_write_dword(offset, buf[0]);

    munmap((void *)flash_base, FLASH_SZ);
    close(driver_fd);
    return 0;
}




/*
 * Local variables:
 *  compile-command: "cc -g -O2 -Wall sa_flash.c -o sa_flash"
 * End:
 */
