/* ------------------------------------------------------------------------- */
/* acq400_sysfs_utils.c  D-TACQ ACQ400 FMC  DRIVER                                   
 * Project: ACQ420_FMC
 * Created: 16 Nov 2016  			/ User: pgm
 * ------------------------------------------------------------------------- *
 *   Copyright (C) 2016 Peter Milne, D-TACQ Solutions Ltd         *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                *
 *
 * TODO 
 * TODO
 * ------------------------------------------------------------------------- */

#include <linux/ctype.h>

#include "acq400.h"
#include "acq400_sysfs.h"

int TIM_CTRL_LOCK;
module_param(TIM_CTRL_LOCK, int, 0644);
MODULE_PARM_DESC(TIM_CTRL_LOCK, "disable usr access when set");


ssize_t acq400_show_triplet(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned T1,
	unsigned T2,
	unsigned T3)
{
	u32 regval = acq400rd32(acq400_devices[dev->id], REG);

	return sprintf(buf, "%d,%d,%d\n",
		MASKSHR(regval, T1), MASKSHR(regval, T2), MASKSHR(regval, T3));
}


#define SHLMASK(x, f) (((x)<<getSHL(f))&(f))

u32 get_edge_sense(char edge[], u32 vv)
{
	int len = strlen(edge);
	if (len){
		int ii;
		for(ii = 0; ii < len; ++ii){
			edge[ii] = tolower(edge[ii]);
		}
		vv = strcmp(edge, "rising") == 0;
	}
	return vv;
}
ssize_t acq400_store_triplet(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned T1,
		unsigned T2,
		unsigned T3)
{
	u32 v1, v2, v3;
	char edge[21];
	edge[0] = '\0';

	if (sscanf(buf, "%d,d%d,%20s", &v1, &v2, edge) == 3 ||
	    sscanf(buf, "%d,%d,%d", &v1, &v2, &v3) == 3		){

		u32 regval = acq400rd32(acq400_devices[dev->id], REG);
		v3 = get_edge_sense(edge, v3);


		regval &= ~(T1|T2|T3);
		regval |= SHLMASK(v1, T1) | SHLMASK(v2, T2) | SHLMASK(v3, T3);

		acq400wr32(acq400_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}

/* generic show_bits, store_bits */

ssize_t acq400_show_bits(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK)
{
	u32 regval = acq400rd32(acq400_devices[dev->id], REG);
	u32 field = (regval>>SHL)&MASK;

	return sprintf(buf, "%x\n", field);
}

ssize_t acq400_store_bits(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK,
		unsigned show_warning)
{
	u32 field;
	if (sscanf(buf, "%x", &field) == 1){
		u32 regval = acq400rd32(acq400_devices[dev->id], REG);
		regval &= ~(MASK << SHL);
		regval |= (field&MASK) << SHL;
		if (show_warning){
			dev_warn(dev, "deprecated %04x = %08x", REG, regval);
		}
		acq400wr32(acq400_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}


ssize_t acq400_show_bitN(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK)
{
	u32 regval = acq400rd32(acq400_devices[dev->id], REG);
	u32 field = (regval>>SHL)&MASK;

	return sprintf(buf, "%x\n", !field);
}

ssize_t acq400_store_bitN(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK,
		unsigned show_warning)
{
	u32 field;
	if (sscanf(buf, "%x", &field) == 1){
		u32 regval = acq400rd32(acq400_devices[dev->id], REG);
		regval &= ~(MASK << SHL);
		regval |= ((!field)&MASK) << SHL;
		if (show_warning){
			dev_warn(dev, "deprecated %04x = %08x", REG, regval);
		}
		acq400wr32(acq400_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}


ssize_t acq400_show_dnum(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	unsigned SHL,
	unsigned MASK)
{
	u32 regval = acq400rd32(acq400_devices[dev->id], REG);
	u32 field = (regval>>SHL)&MASK;
	u32 sbit = (MASK+1) >> 1;
	if ((sbit&field) != 0){
		while(sbit <<= 1){
			field |= sbit;
		}
	}

	return sprintf(buf, "%d\n", (int)field);
}
ssize_t acq400_store_dnum(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		unsigned SHL,
		unsigned MASK)
{
	int field;
	if (sscanf(buf, "%d", &field) == 1){
		u32 regval = acq400rd32(acq400_devices[dev->id], REG);
		regval &= ~(MASK << SHL);
		regval |= (field&MASK) << SHL;
		acq400wr32(acq400_devices[dev->id], REG, regval);
		return count;
	}else{
		return -1;
	}
}

ssize_t show_signal(
	struct device * dev,
	struct device_attribute *attr,
	char * buf,
	unsigned REG,
	int shl, int mbit,
	const char*signame, const char* mbit_hi, const char* mbit_lo)
{
	u32 adc_ctrl = acq400rd32(acq400_devices[dev->id], REG);
	if (adc_ctrl&mbit){
		u32 sel = (adc_ctrl >> shl) & CTRL_SIG_MASK;
		unsigned dx = sel&CTRL_SIG_SEL;
		int rising = ((adc_ctrl >> shl) & CTRL_SIG_RISING) != 0;

		return sprintf(buf, "%s=%d,%d,%d %s d%u %s\n",
				signame, (adc_ctrl&mbit)>>getSHL(mbit), dx, rising,
				mbit_hi, dx, rising? "RISING": "FALLING");
	}else{
		return sprintf(buf, "%s=0,0,0 %s\n", signame, mbit_lo);
	}
}

int store_signal3(struct acq400_dev* adev,
		unsigned REG,
		int shl, int mbit,
		unsigned imode, unsigned dx, unsigned rising)
{
	u32 adc_ctrl = acq400rd32(adev, REG);

	adc_ctrl &= ~mbit;
	if (imode != 0){
		if (dx > 7){
			dev_warn(DEVP(adev), "rejecting \"%u\" dx > 7", dx);
			return -1;
		}
		adc_ctrl &= ~(CTRL_SIG_MASK << shl);
		adc_ctrl |=  (dx|(rising? CTRL_SIG_RISING:0))<<shl;
		adc_ctrl |= imode << getSHL(mbit);
	}
	acq400wr32(adev, REG, adc_ctrl);
	return 0;

}


int lock_action(struct device * dev, const char* buf)
{
	wait_queue_head_t _waitq;

	dev_warn(dev, "pid %d attempted to write to TIM_CTRL with LOCK ON: \"%s\"", current->pid, buf);

	init_waitqueue_head(&_waitq);
	if (TIM_CTRL_LOCK > 1){
		wait_event_interruptible_timeout(_waitq, 0, TIM_CTRL_LOCK);
		dev_warn(dev, "pid %d return", current->pid);
	}
	return -EBUSY;
}
ssize_t store_signal(
		struct device * dev,
		struct device_attribute *attr,
		const char * buf,
		size_t count,
		unsigned REG,
		int shl, int mbit, const char* mbit_hi, const char* mbit_lo,
		int not_while_busy)
{
	char sense;
	char mode[16];
	char* edge = mode;
	unsigned imode, dx, rising;
	struct acq400_dev* adev = acq400_devices[dev->id];

	if (not_while_busy && adev->busy){
		return -EBUSY;
	}

	if (REG == TIM_CTRL && shl == TIM_CTRL_TRIG_SHL && TIM_CTRL_LOCK){
		return lock_action(dev, buf);

	}
	/* first form: imode,dx,rising : easiest with auto eg StreamDevice */
	if (sscanf(buf, "%u,%u,%u", &imode, &dx, &rising) == 3){
		if (store_signal3(adev, REG, shl, mbit, imode, dx, rising)){
			return -1;
		}else{
			return count;
		}
	}

	/* second form: 1,dX,rising : sync_role uses this */
	if (sscanf(buf, "%u,d%u,%s", &imode, &dx, edge) == 3){
		rising = get_edge_sense(edge, 0);
		if (store_signal3(adev, REG, shl, mbit, imode, dx, rising)){
			return -1;
		}else{
			return count;
		}
	}

	/* third form: mode dDX sense : better for human scripting */
	switch(sscanf(buf, "%10s d%u %c", mode, &dx, &sense)){
	case 1:
		if (strcmp(mode, mbit_lo) == 0){
			return store_signal3(adev, REG, shl, mbit, 0, 0, 0);
		}
		dev_warn(dev, "single arg must be:\"%s\"", mbit_lo);
		return -1;
	case 3:
		if (strcmp(mode, mbit_hi) == 0){
			unsigned falling = strchr("Ff-Nn", sense) != NULL;
			rising 	= strchr("Rr+Pp", sense) != NULL;

			if (!rising && !falling){
				dev_warn(dev,
				 "rejecting \"%s\" sense must be R or F", buf);
				return -1;
			}else if (store_signal3(adev, REG, shl, mbit, 1, dx, rising)){
				return -1;
			}else{
				return count;
			}
		}
		/* else fall thru */
	default:
		dev_warn(dev, "%s|%s dX R|F", mbit_lo, mbit_hi);
		return -1;
	}
}
