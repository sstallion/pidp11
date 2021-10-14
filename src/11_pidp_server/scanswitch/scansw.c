/*
 * Scan switches for PiDP-11 front panel
 * 
 * www.obsolescenceguaranteed.blogspot.com
 * 
 * v20181230
*/

#include <time.h>
#include <stdint.h>
#include "gpio.h"

void short_wait(void);								// used as pause between clocked GPIO changes
unsigned bcm_host_get_peripheral_address(void);		// find Pi 2 or Pi's gpio base address
static unsigned get_dt_ranges(const char *filename, unsigned offset); // Pi 2 detect

struct bcm2835_peripheral gpio;
long intervl = 1000000;		// pause


// PART 1 - GPIO and RT process stuff ----------------------------------

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x)
#define INP_GPIO(g)   *(gpio.addr + ((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g)   *(gpio.addr + ((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio.addr + (((g)/10))) |= (((a)<=3?(a) + 4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET  *(gpio.addr + 7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR  *(gpio.addr + 10) // clears bits which are 1 ignores bits which are 0
 
#define GPIO_READ(g)  *(gpio.addr + 13) &= (1<<(g))

#define GPIO_PULL *(gpio.addr + 37) // pull up/pull down
#define GPIO_PULLCLK0 *(gpio.addr + 38) // pull up/pull down clock

// Pi 4 update
/* https://github.com/RPi-Distro/raspi-gpio/blob/master/raspi-gpio.c */	
/* 2711 has a different mechanism for pin pull-up/down/enable  */
#define GPPUPPDN0                57        /* Pin pull-up/down for pins 15:0  */
#define GPPUPPDN1                58        /* Pin pull-up/down for pins 31:16 */
#define GPPUPPDN2                59        /* Pin pull-up/down for pins 47:32 */
#define GPPUPPDN3                60        /* Pin pull-up/down for pins 57:48 */

 
// Exposes the physical address defined in the passed structure using mmap on /dev/mem
int map_peripheral(struct bcm2835_peripheral *p)
{
   if ((p->mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("Failed to open /dev/mem, use sudo.\n");
      return -1;
   }
   p->map = mmap(
      NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,
      p->mem_fd,      	// File descriptor to physical memory virtual file '/dev/mem'
      p->addr_p);       // Address in physical map that we want this memory block to expose
   if (p->map == MAP_FAILED) {
        perror("mmap");
        return -1;
   }
   p->addr = (volatile unsigned int *)p->map;
   return 0;
}
 
void unmap_peripheral(struct bcm2835_peripheral *p) 
{	munmap(p->map, BLOCK_SIZE);
	close(p->mem_fd);
}

 
// PART 2 - the multiplexing logic driving the front panel -------------

uint8_t row; 
uint8_t ledrows[] = {20, 21, 22, 23, 24, 25};
uint8_t rows[] = {16, 17, 18};
uint8_t cols[] = {26,27,4, 5,6,7, 8,9,10, 11,12,13};

int main()
{
	int i,j,k,switchscan[3], tmp;
	uint8_t exVal = 0;
	
	// ------------ Find gpio address (different for Pi 2) -------------

	gpio.addr_p = bcm_host_get_peripheral_address() +  + 0x200000;

//	if (gpio.addr_p== 0x20200000) printf("scanswitch - RPi Plus\n");
//	else printf("scanswitch - RPi 2 or later\n");
//printf("gpio %lx\r\n", gpio.addr_p);

	if(map_peripheral(&gpio) == -1) 
	{	printf("Failed to map the physical GPIO registers into the virtual memory space.\n");
		return -1;
	}

	// initialise GPIO (all pins used as inputs, with pull-ups enabled on cols)
	for (i=0;i<6;i++)			// Define ledrows as input
		INP_GPIO(ledrows[i]);
	for (i=0;i<12;i++)			// Define cols as input
		INP_GPIO(cols[i]);
	for (i=0;i<3;i++)			// Define rows as input
		INP_GPIO(rows[i]);

if (gpio.addr_p==0xfe200000)
{
	//printf("Configuring pullups for Pi 4\r\n");
	/* https://github.com/RPi-Distro/raspi-gpio/blob/master/raspi-gpio.c */	
	/* 2711 has a different mechanism for pin pull-up/down/enable  */
	int gpiox;
	int pullreg;
	int pullshift;
        unsigned int pullbits;
        unsigned int pull;

	// GPIO column pins
	for (i=0;i<12;i++)
	{
		gpiox = cols[i];
		pullreg = GPPUPPDN0 + (gpiox>>4);
		pullshift = (gpiox & 0xf) << 1;
		pull = 1;	// pullup

		pullbits = *(gpio.addr + pullreg);
		//printf("col %d pullreg %d pullshift %x pull %d -- pullbits %x --> ", gpiox, pullreg, pullshift, pull, pullbits);
		pullbits &= ~(3 << pullshift);
		pullbits |= (pull << pullshift);
		*(gpio.addr + pullreg) = pullbits;
		//printf("%x == %x --- %xl\r\n", pullbits, *(&gpio.addr_p + pullreg), gpio.addr_p + pullreg);
	}
	// GPIO row pins
	for (i=0;i<3;i++)
	{
		gpiox = rows[i];
		pullreg = GPPUPPDN0 + (gpiox>>4);
		pullshift = (gpiox & 0xf) << 1;
		pull = 0;	// pullup

		pullbits = *(gpio.addr + pullreg);
		pullbits &= ~(3 << pullshift);
		pullbits |= (pull << pullshift);
		*(gpio.addr + pullreg) = pullbits;
	}
	// GPIO ledrow pins
	for (i=0;i<6;i++)
	{
		gpiox = ledrows[i];
		pullreg = GPPUPPDN0 + (gpiox>>4);
		pullshift = (gpiox & 0xf) << 1;
		pull = 0;	// pullup

		pullbits = *(gpio.addr + pullreg);
		pullbits &= ~(3 << pullshift);
		pullbits |= (pull << pullshift);
		*(gpio.addr + pullreg) = pullbits;
	}
}
else 	// configure pullups for older Pis
{
	//printf("Configuring pullups for Pi 3 or older\r\n");
	// BCM2835 ARM Peripherals PDF p 101 & elinux.org/RPi_Low-level_peripherals#Internal_Pull-Ups_.26_Pull-Downs
	GPIO_PULL = 2;				// pull-up
	short_wait();				// must wait 150 cycles
	GPIO_PULLCLK0 = 0x0C003ff0; 		// selects GPIO pins 4..15+26+27 (assumes we avoid pins 2 and 3!)
	short_wait();
	GPIO_PULL = 0;  			// reset GPPUD register
	short_wait();
	GPIO_PULLCLK0 = 0; 			// remove clock
	short_wait(); 				// probably unnecessary

	// BCM2835 ARM Peripherals PDF p 101 & elinux.org/RPi_Low-level_peripherals#Internal_Pull-Ups_.26_Pull-Downs
	GPIO_PULL = 0;				// no pull-up no pull-down just float
	short_wait();				// must wait 150 cycles
	GPIO_PULLCLK0 = 0x03f00000; 		// selects GPIO pins 20..25 on pidp11
	short_wait();
	GPIO_PULL = 0; 				// reset GPPUD register
	short_wait();
	GPIO_PULLCLK0 = 0; 			// remove clock
	short_wait(); 				// probably unnecessary

	// BCM2835 ARM Peripherals PDF p 101 & elinux.org/RPi_Low-level_peripherals#Internal_Pull-Ups_.26_Pull-Downs
	GPIO_PULL = 0;				// no pull-up no pull down just float
	short_wait();				// must wait 150 cycles
	GPIO_PULLCLK0 = 0x070000; 		// selects GPIO pins 16..18
	short_wait();
	GPIO_PULL = 0; 				// reset GPPUD register
	short_wait();
	GPIO_PULLCLK0 = 0; 			// remove clock
	short_wait(); 				// probably unnecessary
}
	// --------------------------------------------------


	// prepare for reading switches		

	for(row = 0; row < 2; row++) {
		INP_GPIO(rows[row]);
		OUT_GPIO(rows[row]);			// turn on one switch row
		GPIO_CLR = 1 << rows[row];		// and output 0V to overrule built-in pull-up from column input pin
	
		nanosleep ((struct timespec[]){{0, intervl/100}}, NULL); // probably unnecessary long wait
		switchscan[row]=0;

		for(j=0;j<12;j++) {		// 12 switches in each row
			tmp = GPIO_READ(cols[j]);
			if(tmp==0)
				switchscan[row] += 1<<j;// only allows for one switch (last one) to be detected in each row
		}
		INP_GPIO(rows[row]);			// stop sinking current from this row of switches
	}

	unmap_peripheral(&gpio);

	printf("%d\n", switchscan[1]<<12 | switchscan[0]);

	return switchscan[0];
}


void short_wait(void)				// creates pause required in between clocked GPIO settings changes
{
	fflush(stdout); //
	usleep(100000); // suggested as alternative for asm which c99 does not accept
}



unsigned bcm_host_get_peripheral_address(void)		// find Pi 2 or Pi's gpio base address
{
//   unsigned address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
//   return address == ~0 ? 0x20000000 : address;
// Pi 4 fix: https://github.com/raspberrypi/userland/blob/master/host_applications/linux/libs/bcm_host/bcm_host.c
   unsigned address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
   if (address == 0)
      address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
   return address == ~0 ? 0x20000000 : address;
}
static unsigned get_dt_ranges(const char *filename, unsigned offset)
{
   unsigned address = ~0;
   FILE *fp = fopen(filename, "rb");
   if (fp)
   {
      unsigned char buf[4];
      fseek(fp, offset, SEEK_SET);
      if (fread(buf, 1, sizeof buf, fp) == sizeof buf)
      address = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0;
      fclose(fp);
   }
   return address;
}
