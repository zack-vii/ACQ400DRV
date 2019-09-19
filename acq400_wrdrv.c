/* ------------------------------------------------------------------------- */
/* acq400_wrdrv.c  D-TACQ ACQ400 White Rabbit  DRIVER	                     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2019 Peter Milne, D-TACQ Solutions Ltd                    *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of Version 2 of the GNU General Public License        *
 *  as published by the Free Software Foundation;                            *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/*
 * acq400_wrdrv.c
 *
 *  Created on: 19 Sep 2019
 *      Author: pgm
 */


#include "acq400.h"
#include "bolo.h"
#include "hbm.h"
#include "acq400_debugfs.h"
#include "acq400_lists.h"
#include "acq400_ui.h"



int wr_ts_inten = 1;
module_param(wr_ts_inten, int, 0444);
MODULE_PARM_DESC(nbuffers, "1: enable Time Stamp interrupts");

int wr_pps_inten = 1;
module_param(wr_pps_inten, int, 0444);
MODULE_PARM_DESC(nbuffers, "1: enable PPS interrupts");



static inline u32 wr_ctrl_set(struct acq400_dev *adev, unsigned bits){
	u32 int_sta = acq400rd32(adev, WR_CTRL);
	int_sta |= bits;
	acq400wr32(adev, WR_CTRL, int_sta);
	return int_sta;
}
static inline u32 wr_ctrl_clr(struct acq400_dev *adev, unsigned bits){
	u32 int_sta = acq400rd32(adev, WR_CTRL);
	int_sta &= ~bits;
	acq400wr32(adev, WR_CTRL, int_sta);
	return int_sta;
}


static irqreturn_t wr_ts_isr(int irq, void *dev_id)
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	u32 int_sta = acq400rd32(adev, WR_CTRL);

	sc_dev->ts_client.wc_ts =acq400rd32(adev, WR_TAI_STAMP);

	acq400wr32(adev, WR_CTRL, int_sta);
	wake_up_interruptible(&sc_dev->ts_client.wc_waitq);
	return IRQ_HANDLED;	/* canned */
}
#if 0
static irqreturn_t wr_ts_kthread(int irq, void *dev_id)
/* keep the AO420 FIFO full. Recycle buffer only */
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	return IRQ_HANDLED;
}
#endif

static irqreturn_t wr_pps_isr(int irq, void *dev_id)
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	u32 int_sta = acq400rd32(adev, WR_CTRL);

	sc_dev->pps_client.wc_ts =acq400rd32(adev, WR_TAI_CUR_L);

	acq400wr32(adev, WR_CTRL, int_sta);
	wake_up_interruptible(&sc_dev->pps_client.wc_waitq);
	return IRQ_HANDLED;	/* canned */
}
#if 0
static irqreturn_t wr_pps_kthread(int irq, void *dev_id)
/* keep the AO420 FIFO full. Recycle buffer only */
{
	struct acq400_dev *adev = (struct acq400_dev *)dev_id;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);

	return IRQ_HANDLED;
}
#endif


void init_scdev(struct acq400_dev* adev)
{
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	init_waitqueue_head(&sc_dev->pps_client.wc_waitq);
	init_waitqueue_head(&sc_dev->ts_client.wc_waitq);
}

int acq400_wr_init_irq(struct acq400_dev* adev)
{
	int rc;
	int irq = platform_get_irq(adev->pdev, IRQ_REQUEST_OFFSET);
	if (irq <= 0){
		return 0;
	}
	dev_info(DEVP(adev), "acq400_wr_init_irq %d", irq);

/*
	rc = devm_request_threaded_irq(
			DEVP(adev), irq, wr_ts_isr, wr_ts_kthread, IRQF_NO_THREAD,
			"wr_ts",	adev);
*/
	rc = devm_request_irq(DEVP(adev), irq, wr_ts_isr, IRQF_NO_THREAD, "wr_ts", adev);

	if (rc){
		dev_err(DEVP(adev),"unable to get IRQ %d K414 KLUDGE IGNORE\n", irq);
		return 0;
	}

	irq = platform_get_irq(adev->pdev, IRQ_REQUEST_OFFSET+1);
	if (irq <= 0){
		return 0;
	}
	if (wr_ts_inten){
		wr_ctrl_set(adev, WR_CTRL_TS_INTEN);
	}
	dev_info(DEVP(adev), "acq400_wr_init_irq %d", irq);
/*
	rc = devm_request_threaded_irq(
			DEVP(adev), irq, wr_pps_isr, wr_pps_kthread, IRQF_NO_THREAD,
			"wr_pps",	adev);
*/
	rc = devm_request_irq(DEVP(adev), irq, wr_pps_isr, IRQF_NO_THREAD, "wr_pps", adev);
	if (rc){
		dev_err(DEVP(adev),"unable to get IRQ %d K414 KLUDGE IGNORE\n", irq);
		return 0;
	}
	if (wr_pps_inten){
		wr_ctrl_set(adev, WR_CTRL_PPS_INTEN);
	}

	init_scdev(adev);
	return rc;
}

int _acq400_wr_open(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	int is_ts = PD(file)->minor==ACQ400_MINOR_WR_TS;
	struct WrClient *wc = is_ts? &sc_dev->ts_client: &sc_dev->pps_client;

	if (!is_ts && (file->f_flags & O_WRONLY)) {
		return -EACCES;
	}else if (wc->wc_pid != 0 && wc->wc_pid != current->pid){
		return -EBUSY;
	}else{
		wc->wc_pid = current->pid;
		return 0;
	}
}

int acq400_wr_release(struct inode *inode, struct file *file)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	int is_ts = PD(file)->minor==ACQ400_MINOR_WR_TS;
	struct WrClient *wc = is_ts? &sc_dev->ts_client: &sc_dev->pps_client;

	if (wc->wc_pid != 0 && wc->wc_pid == current->pid){
		wc->wc_pid = 0;
	}
	return 0;
}

ssize_t acq400_wr_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
	struct acq400_path_descriptor* pdesc = PD(file);
	struct acq400_dev* adev = pdesc->dev;
	struct acq400_sc_dev* sc_dev = container_of(adev, struct acq400_sc_dev, adev);
	struct WrClient *wc = PD(file)->minor==ACQ400_MINOR_WR_TS? &sc_dev->ts_client: &sc_dev->pps_client;
	u32 tmp;
	int rc;

	if (count < sizeof(u32)){
		return -EINVAL;
	}
	dev_dbg(DEVP(adev), "acq400_wr_read");
	if (wait_event_interruptible(wc->wc_waitq, wc->wc_ts)){
		return -EINTR;
	}
	tmp = wc->wc_ts;
	wc->wc_ts = 0;

	rc = copy_to_user(buf, &tmp, sizeof(u32));
	f_pos += sizeof(u32);

	if (rc){
		return -rc;
	}else{
		return sizeof(u32);
	}
}

ssize_t acq400_wr_write(
	struct file *file, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct acq400_dev* adev = ACQ400_DEV(file);
	u32 tmp;
	int rc;


	if (count < sizeof(u32)){
		return -EINVAL;
	}
	dev_dbg(DEVP(adev), "acq400_wr_write() ");


	rc = copy_from_user(&tmp, buf, sizeof(u32));

	if (rc){
		return -1;
	}else{
		acq400wr32(adev, WR_TAI_TRG, tmp);
	}

	*f_pos += sizeof(u32);
	dev_dbg(DEVP(adev), "acq400_wr_write() return count:%u", sizeof(u32));

	return sizeof(u32);
}

int acq400_wr_open(struct inode *inode, struct file *file)
{
	static struct file_operations acq400_fops_wr = {
			.open = _acq400_wr_open,
			.read = acq400_wr_read,
			.write = acq400_wr_write,
			.release = acq400_wr_release
	};
	file->f_op = &acq400_fops_wr;
	return file->f_op->open(inode, file);
}