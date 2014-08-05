/* soft-sccb.c
 * Implement the SCCB master side protocol.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <linux/semaphore.h>
#include <mach/regs-gpio.h>

#include "sccb.h"

static DECLARE_MUTEX(bus_lock);

static void __inline__ sccb_start(void)
{
	CFG_WRITE(SIO_D);

	Low(SIO_D);
	WAIT_STABLE();
}

static void __inline__ sccb_write_byte(u8 data)
{
	int i;

	CFG_WRITE(SIO_D);
	WAIT_STABLE();

	/* write 8-bits octet. */
	for (i=0;i<8;i++)
	{
		Low(SIO_C);
		WAIT_STABLE();

		if (data & 0x80)
		{
			High(SIO_D);
		}
		else
		{
			Low(SIO_D);
		}
		data = data<<1;
		WAIT_CYCLE();
		
		High(SIO_C);
		WAIT_CYCLE();
	}
	
	/* write byte done, wait the Don't care bit now. */
	{
		Low(SIO_C);
		High(SIO_D);
		CFG_READ(SIO_D);
		WAIT_CYCLE();
		
		High(SIO_C);
		WAIT_CYCLE();
	}
}

static u8 __inline__ sccb_read_byte(void)
{
	int i;
	u8 data;

	CFG_READ(SIO_D);
	WAIT_STABLE();
	
	Low(SIO_C);
	WAIT_CYCLE();

	data = 0;
	for (i=0;i<8;i++)
	{
		High(SIO_C);
		WAIT_STABLE();
		
		data = data<<1;
		data |= State(SIO_D)?1:0;
		WAIT_CYCLE();
		
		Low(SIO_C);
		WAIT_CYCLE();
	}
	
	/* read byte down, write the NA bit now.*/
	{
		CFG_WRITE(SIO_D);
		High(SIO_D);
		WAIT_CYCLE();
		
		High(SIO_C);
		WAIT_CYCLE();
	}
	
	return data;
}

static void __inline__ sccb_stop(void)
{
	Low(SIO_C);
	WAIT_STABLE();
	
	CFG_WRITE(SIO_D);
	Low(SIO_D);
	WAIT_CYCLE();
	
	High(SIO_C);
	WAIT_STABLE();
	
	High(SIO_D);
	WAIT_CYCLE();
	
	CFG_READ(SIO_D);
}

void sccb_write(u8 IdAddr, u8 SubAddr, u8 data)
{
	down(&bus_lock);
	sccb_start();
	sccb_write_byte(IdAddr);
	sccb_write_byte(SubAddr);
	sccb_write_byte(data);
	sccb_stop();
	up (&bus_lock);
}

u8 sccb_read(u8 IdAddr, u8 SubAddr)
{
	u8 data;

	down(&bus_lock);
	sccb_start();
	sccb_write_byte(IdAddr);
	sccb_write_byte(SubAddr);                
	sccb_stop();

	sccb_start();
	sccb_write_byte(IdAddr|0x01);
	data = sccb_read_byte();
	sccb_stop();
	up(&bus_lock);
	
	return data;
}

int sccb_init(void)
{
	CFG_WRITE(SIO_C);
	CFG_WRITE(SIO_D);

	High(SIO_C);
	High(SIO_D);
	WAIT_STABLE();

	return 0;
}

void sccb_cleanup(void)
{
	CFG_READ(SIO_C);
	CFG_READ(SIO_D);
}
