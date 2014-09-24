/*                                                                           *
 * dio432_drv.c                                                              *
 *                                                                           *
 *  Created on: 21 Sep 2014                                                  *
 *      Author: pgm                                                          *
 *   Copyright (C) 2014 Peter Milne, D-TACQ Solutions Ltd                    *
 *                      <peter dot milne at D hyphen TACQ dot com>           *
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

#include "acq400.h"
#include "acq400_lists.h"

#include "dmaengine.h"

int dio432_immediate_jiffies = 1;
module_param(dio432_immediate_jiffies, int, 0644);
MODULE_PARM_DESC(ndevices, "poll interval, immediate mode");

static void _acq400wr32(struct acq400_dev *adev, int offset, u32 value)
{
	if (adev->RW32_debug){
		dev_info(DEVP(adev), "acq400wr32 %p [0x%02x] = %08x\n",
				adev->dev_virtaddr + offset, offset, value);
	}else{
		dev_dbg(DEVP(adev), "acq400wr32 %p [0x%02x] = %08x\n",
				adev->dev_virtaddr + offset, offset, value);
	}

	iowrite32(value, adev->dev_virtaddr + offset);
}

static u32 _acq400rd32(struct acq400_dev *adev, int offset)
{
	u32 rc = ioread32(adev->dev_virtaddr + offset);
	if (adev->RW32_debug){
		dev_info(DEVP(adev), "acq400rd32 %p [0x%02x] = %08x\n",
			adev->dev_virtaddr + offset, offset, rc);
	}else{
		dev_dbg(DEVP(adev), "acq400rd32 %p [0x%02x] = %08x\n",
			adev->dev_virtaddr + offset, offset, rc);
	}
	return rc;
}

void dio432_set_direction(struct acq400_dev *adev, unsigned byte_is_output)
{
	_acq400wr32(adev, DIO432_DIO_CPLD_CTRL, byte_is_output);
	_acq400wr32(adev, DIO432_DIO_CPLD_CTRL,
				DIO432_CPLD_CTRL_COMMAND_WRITE|byte_is_output);

	/* @@todo doxygen indicates poll for complete, but SR example sets it
	 * following the SR example
	 */
	_acq400wr32(adev, DIO432_DIO_CPLD_CTRL,
			DIO432_CPLD_CTRL_COMMAND_COMPLETE|
			DIO432_CPLD_CTRL_COMMAND_WRITE|byte_is_output);
	_acq400wr32(adev, DIO432_DIO_CPLD_CTRL, byte_is_output);
}

void dio32_init(struct acq400_dev *adev, int immediate)
{
	unsigned syscon = immediate? DIO432_CTRL_LL: 0;

#ifdef FAST_IS_GOOD
	syscon |= IS_DIO432PMOD(adev)?
		DIO432_CTRL_SHIFT_DIV_PMOD: DIO432_CTRL_SHIFT_DIV_FMC;
#else
	syscon |= DIO432_CTRL_SHIFT_DIV_PMOD;
#endif
	_acq400wr32(adev, DIO432_DIO_CTRL, syscon);
	_acq400wr32(adev, DIO432_DIO_CTRL, syscon |= DIO432_CTRL_MODULE_EN);
	_acq400wr32(adev, DIO432_DIO_CTRL, syscon | DIO432_CTRL_ADC_RST|DIO432_CTRL_FIFO_RST);
	_acq400wr32(adev, DIO432_DIO_CTRL, syscon);
	_acq400wr32(adev, DIO432_DIO_CTRL, syscon |= DIO432_CTRL_FIFO_EN);
	dio432_set_direction(adev, adev->dio432_immediate.byte_is_output);

	_acq400wr32(adev, DIO432_DI_FIFO_STATUS, DIO432_FIFSTA_CLR);
	_acq400wr32(adev, DIO432_DO_FIFO_STATUS, DIO432_FIFSTA_CLR);
	_acq400wr32(adev, DIO432_DIO_ICR, 1);
	_acq400wr32(adev, DIO432_DIO_CTRL, syscon |= DIO432_CTRL_ADC_EN);
}

int dio32_immediate_loop(void *data)
{
	struct acq400_dev *adev = (struct acq400_dev *)data;
	unsigned syscon = DIO432_CTRL_LL;
	int nloop = 0;
	int wake_clients;

	dio32_init(adev, 1);

	for(; !kthread_should_stop(); ++nloop){
		unsigned do32_cache = adev->dio432_immediate.DO32;
		_acq400wr32(adev, DIO432_FIFO, adev->dio432_immediate.DO32);
		adev->dio432_immediate.DI32 = _acq400rd32(adev, DIO432_FIFO);
		if (wake_clients){
			wake_up_interruptible(&adev->DMA_READY);
		}
		wake_clients = wait_event_interruptible_timeout(
			adev->w_waitq,
			do32_cache != adev->dio432_immediate.DO32 || kthread_should_stop(),
			dio432_immediate_jiffies);
	}
}

void dio432_init_immediate(struct acq400_dev* adev)
{
	adev->w_task = kthread_run(dio32_immediate_loop, adev,
					"%s.dio32i", devname(adev));
}

void dio432_init_clocked(struct acq400_dev* adev)
{
	if (adev->w_task != 0){
		kthread_stop(adev->w_task);
	}
	dio32_init(adev, 0);
}
void dio432_set_immediate(struct acq400_dev* adev, int enable)
{
	if (enable){
		dio432_init_immediate(adev);
	}else{
		dio432_init_clocked(adev);
	}
}
