/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

/*
 * RMNET SDIO module.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/wakelock.h>

#include <mach/sdio_al.h>

#define SDIO_CH_LOCAL_OPEN       0x1
#define SDIO_CH_REMOTE_OPEN      0x2

#define SDIO_MUX_HDR_MAGIC_NO    0x33fc

#define SDIO_MUX_HDR_CMD_DATA    0
#define SDIO_MUX_HDR_CMD_OPEN    1
#define SDIO_MUX_HDR_CMD_CLOSE   2

#define DEBUG

static int msm_rmnet_sdio_debug_mask;
module_param_named(debug_mask, msm_rmnet_sdio_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(DEBUG)
static uint32_t msm_rmnet_sdio_read_cnt;
static uint32_t msm_rmnet_sdio_write_cnt;

#define DBG(x...) do {		                \
		if (msm_rmnet_sdio_debug_mask)	\
			printk(KERN_DEBUG x);	\
	} while (0)

#define DBG_INC_READ_CNT(x) do {	                       \
		msm_rmnet_sdio_read_cnt += (x);                \
		printk(KERN_DEBUG "%s: total read bytes %u\n", \
		       __func__, msm_rmnet_sdio_read_cnt);     \
	} while (0)

#define DBG_INC_WRITE_CNT(x)  do {	                          \
		msm_rmnet_sdio_write_cnt += (x);                  \
		printk(KERN_DEBUG "%s: total written bytes %u\n", \
		       __func__, msm_rmnet_sdio_write_cnt);	  \
	} while (0)
#else
#define DBG(x...) do { } while (0)
#define DBG_INC_READ_CNT(x...) do { } while (0)
#define DBG_INC_WRITE_CNT(x...) do { } while (0)
#endif

struct sdio_ch_info {
	uint32_t status;
	void (*receive_cb)(void *, struct sk_buff *);
	void (*write_done)(void *, struct sk_buff *);
	void *priv;
	spinlock_t lock;
	struct sk_buff *skb;
};

static struct sdio_channel *sdio_mux_ch;
static struct sdio_ch_info sdio_ch[8];
struct wake_lock sdio_mux_ch_wakelock;

struct sdio_mux_hdr {
	uint16_t magic_num;
	uint8_t reserved;
	uint8_t cmd;
	uint8_t pad_len;
	uint8_t ch_id;
	uint16_t pkt_len;
};

struct sdio_partial_pkt_info {
	uint32_t valid;
	struct sk_buff *skb;
	struct sdio_mux_hdr *hdr;
};

static void sdio_mux_read_data(struct work_struct *work);
static void sdio_mux_write_data(struct work_struct *work);

static DEFINE_MUTEX(sdio_mux_lock);
static DECLARE_WORK(work_sdio_mux_read, sdio_mux_read_data);
static DECLARE_WORK(work_sdio_mux_write, sdio_mux_write_data);

static struct workqueue_struct *sdio_mux_workqueue;
static struct sdio_partial_pkt_info sdio_partial_pkt;

#define sdio_ch_is_open(x)						\
	(sdio_ch[(x)].status == (SDIO_CH_LOCAL_OPEN | SDIO_CH_REMOTE_OPEN))

#define sdio_ch_is_local_open(x)			\
	(sdio_ch[(x)].status & SDIO_CH_LOCAL_OPEN)

static inline void skb_set_data(struct sk_buff *skb,
				unsigned char *data,
				unsigned int len)
{
	/* panic if tail > end */
	skb->data = data;
	skb->tail = skb->data + len;
	skb->len  = len;
}

static void sdio_mux_save_partial_pkt(struct sdio_mux_hdr *hdr,
				      struct sk_buff *skb_mux)
{
	struct sk_buff *skb;

	/* i think we can avoid cloning here */
	skb =  skb_clone(skb_mux, GFP_KERNEL);
	if (!skb) {
		pr_err("%s: cannot clone skb\n", __func__);
		return;
	}

	/* protect? */
	skb_set_data(skb, (unsigned char *)hdr,
		     skb->tail - (unsigned char *)hdr);
	sdio_partial_pkt.skb = skb;
	sdio_partial_pkt.valid = 1;
	DBG("%s: head %p data %p tail %p end %p len %d\n", __func__,
	    skb->head, skb->data, skb->tail, skb->end, skb->len);
	return;
}

static void *handle_sdio_mux_data(struct sdio_mux_hdr *hdr,
				  struct sk_buff *skb_mux)
{
	struct sk_buff *skb;
	void *rp = (void *)hdr;
	unsigned long flags;

	/* protect? */
	rp += sizeof(*hdr);
	if (rp < (void *)skb_mux->tail)
		rp += (hdr->pkt_len + hdr->pad_len);

	if (rp > (void *)skb_mux->tail) {
		/* partial packet */
		sdio_mux_save_partial_pkt(hdr, skb_mux);
		goto packet_done;
	}

	DBG("%s: hdr %p next %p tail %p pkt_size %d\n",
	    __func__, hdr, rp, skb_mux->tail, hdr->pkt_len + hdr->pad_len);

	skb =  skb_clone(skb_mux, GFP_KERNEL);
	if (!skb) {
		pr_err("%s: cannot clone skb\n", __func__);
		goto packet_done;
	}

	skb_set_data(skb, (unsigned char *)(hdr + 1), hdr->pkt_len);
	DBG("%s: head %p data %p tail %p end %p len %d\n",
	    __func__, skb->head, skb->data, skb->tail, skb->end, skb->len);

	/* probably we should check channel status */
	/* discard packet early if local side not open */
	spin_lock_irqsave(&sdio_ch[hdr->ch_id].lock, flags);
	if (sdio_ch[hdr->ch_id].receive_cb)
		sdio_ch[hdr->ch_id].receive_cb(sdio_ch[hdr->ch_id].priv, skb);
	else
		dev_kfree_skb_any(skb);
	spin_unlock_irqrestore(&sdio_ch[hdr->ch_id].lock, flags);

packet_done:
	return rp;
}

static void *handle_sdio_mux_command(struct sdio_mux_hdr *hdr,
				     struct sk_buff *skb_mux)
{
	void *rp;
	unsigned long flags;

	DBG("%s: cmd %d ch %d\n", __func__, hdr->cmd, hdr->ch_id);
	switch (hdr->cmd) {
	case SDIO_MUX_HDR_CMD_DATA:
		rp = handle_sdio_mux_data(hdr, skb_mux);
		break;
	case SDIO_MUX_HDR_CMD_OPEN:
		spin_lock_irqsave(&sdio_ch[hdr->ch_id].lock, flags);
		sdio_ch[hdr->ch_id].status |= SDIO_CH_REMOTE_OPEN;
		spin_unlock_irqrestore(&sdio_ch[hdr->ch_id].lock, flags);
		rp = hdr + 1;
		break;
	case SDIO_MUX_HDR_CMD_CLOSE:
		/* probably should drop pending write */
		spin_lock_irqsave(&sdio_ch[hdr->ch_id].lock, flags);
		sdio_ch[hdr->ch_id].status &= ~SDIO_CH_REMOTE_OPEN;
		spin_unlock_irqrestore(&sdio_ch[hdr->ch_id].lock, flags);
		rp = hdr + 1;
		break;
	default:
		rp = hdr + 1;
	}

	return rp;
}

static void *handle_sdio_partial_pkt(struct sk_buff *skb_mux)
{
	struct sk_buff *p_skb;
	struct sdio_mux_hdr *p_hdr;
	void *ptr, *rp = skb_mux->data;

	/* protoect? */
	if (sdio_partial_pkt.valid) {
		p_skb = sdio_partial_pkt.skb;

		ptr = skb_push(skb_mux, p_skb->len);
		memcpy(ptr, p_skb->data, p_skb->len);
		sdio_partial_pkt.skb = NULL;
		sdio_partial_pkt.valid = 0;
		dev_kfree_skb_any(p_skb);

		DBG("%s: head %p data %p tail %p end %p len %d\n", __func__,
		    skb_mux->head, skb_mux->data, skb_mux->tail,
		    skb_mux->end, skb_mux->len);

		p_hdr = (struct sdio_mux_hdr *)skb_mux->data;
		rp = handle_sdio_mux_command(p_hdr, skb_mux);
	}
	return rp;
}

static void sdio_mux_read_data(struct work_struct *work)
{
	struct sk_buff *skb_mux;
	void *ptr = 0;
	int sz, rc, len = 0;
	struct sdio_mux_hdr *hdr;

	DBG("%s: reading\n", __func__);
	/* should probably have a separate read lock */
	mutex_lock(&sdio_mux_lock);
	sz = sdio_read_avail(sdio_mux_ch);
	DBG("%s: read avail %d\n", __func__, sz);
	if (sz <= 0) {
		if (sz)
			pr_err("%s: read avail failed %d\n", __func__, sz);
		mutex_unlock(&sdio_mux_lock);
		return;
	}

	/* net_ip_aling is probably not required */
	if (sdio_partial_pkt.valid)
		len = sdio_partial_pkt.skb->len;
	skb_mux = dev_alloc_skb(sz + NET_IP_ALIGN + len);
	if (skb_mux == NULL) {
		pr_err("%s: cannot allocate skb\n", __func__);
		mutex_unlock(&sdio_mux_lock);
		return;
	}

	skb_reserve(skb_mux, NET_IP_ALIGN + len);
	ptr = skb_put(skb_mux, sz);

	/* half second wakelock is fine? */
	wake_lock_timeout(&sdio_mux_ch_wakelock, HZ / 2);
	rc = sdio_read(sdio_mux_ch, ptr, sz);
	DBG("%s: read %d\n", __func__, rc);
	if (rc) {
		pr_err("%s: sdio read failed %d\n", __func__, rc);
		dev_kfree_skb_any(skb_mux);
		mutex_unlock(&sdio_mux_lock);
		queue_work(sdio_mux_workqueue, &work_sdio_mux_read);
		return;
	}
	mutex_unlock(&sdio_mux_lock);

	DBG_INC_READ_CNT(sz);
	DBG("%s: head %p data %p tail %p end %p len %d\n", __func__,
	    skb_mux->head, skb_mux->data, skb_mux->tail,
	    skb_mux->end, skb_mux->len);

	/* move to a separate function */
	/* probably do skb_pull instead of pointer adjustment */
	hdr = handle_sdio_partial_pkt(skb_mux);
	while ((void *)hdr < (void *)skb_mux->tail) {

		if (((void *)hdr + sizeof(*hdr)) > (void *)skb_mux->tail) {
			/* handle partial header */
			sdio_mux_save_partial_pkt(hdr, skb_mux);
			break;
		}

		if (hdr->magic_num != SDIO_MUX_HDR_MAGIC_NO) {
			pr_err("%s: packet error\n", __func__);
			break;
		}

		hdr = handle_sdio_mux_command(hdr, skb_mux);
	}
	dev_kfree_skb_any(skb_mux);

	DBG("%s: read done\n", __func__);
	queue_work(sdio_mux_workqueue, &work_sdio_mux_read);
}

static int sdio_mux_write(struct sk_buff *skb)
{
	int rc, sz;

	mutex_lock(&sdio_mux_lock);
	sz = sdio_write_avail(sdio_mux_ch);
	DBG("%s: avail %d len %d\n", __func__, sz, skb->len);
	if (skb->len <= sz) {
		rc = sdio_write(sdio_mux_ch, skb->data, skb->len);
		DBG("%s: write returned %d\n", __func__, rc);
		if (rc)
			rc = -EAGAIN;
		else
			DBG_INC_WRITE_CNT(skb->len);
	} else
		rc = -ENOMEM;

	mutex_unlock(&sdio_mux_lock);
	return rc;
}

static int sdio_mux_write_cmd(void *data, uint32_t len)
{
	int avail, rc;
	for (;;) {
		mutex_lock(&sdio_mux_lock);
		avail = sdio_write_avail(sdio_mux_ch);
		DBG("%s: avail %d len %d\n", __func__, avail, len);
		if (avail >= len) {
			rc = sdio_write(sdio_mux_ch, data, len);
			DBG("%s: write returned %d\n", __func__, rc);
			if (!rc) {
				DBG_INC_WRITE_CNT(len);
				break;
			}
		}
		mutex_unlock(&sdio_mux_lock);
		msleep(250);
	}
	mutex_unlock(&sdio_mux_lock);
	return 0;
}

static void sdio_mux_write_data(struct work_struct *work)
{
	int i, rc, reschedule = 0;
	struct sk_buff *skb;
	unsigned long flags;

	for (i = 0; i < 8; i++) {
		spin_lock_irqsave(&sdio_ch[i].lock, flags);
		if (sdio_ch_is_local_open(i) && sdio_ch[i].skb) {
			skb = sdio_ch[i].skb;
			spin_unlock_irqrestore(&sdio_ch[i].lock, flags);
			DBG("%s: writing for ch %d\n", __func__, i);
			rc = sdio_mux_write(skb);
			if (rc == -EAGAIN) {
				reschedule = 1;
			} else if (!rc) {
				spin_lock_irqsave(&sdio_ch[i].lock, flags);
				sdio_ch[i].skb = NULL;
				sdio_ch[i].write_done(sdio_ch[i].priv, skb);
				spin_unlock_irqrestore(&sdio_ch[i].lock, flags);
			}
		} else
			spin_unlock_irqrestore(&sdio_ch[i].lock, flags);
	}

	/* probably should use delayed work */
	if (reschedule)
		queue_work(sdio_mux_workqueue, &work_sdio_mux_write);
}

int msm_rmnet_sdio_write(uint32_t id, struct sk_buff *skb)
{
	int rc = 0;
	struct sdio_mux_hdr *hdr;
	unsigned long flags;

	if (!skb)
		return -EINVAL;

	DBG("%s: writing to ch %d len %d\n", __func__, id, skb->len);
	spin_lock_irqsave(&sdio_ch[id].lock, flags);
	if (!sdio_ch_is_local_open(id)) {
		pr_err("%s: port not open: %d\n", __func__, sdio_ch[id].status);
		rc = -ENODEV;
		goto write_done;
	}

	if (sdio_ch[id].skb) {
		pr_err("%s: packet pending ch: %d\n", __func__, id);
		rc = -EPERM;
		goto write_done;
	}

	hdr = (struct sdio_mux_hdr *)skb_push(skb, sizeof(struct sdio_mux_hdr));

	/* caller should allocate for hdr and padding
	   hdr is fine, padding is tricky */
	hdr->magic_num = SDIO_MUX_HDR_MAGIC_NO;
	hdr->cmd = SDIO_MUX_HDR_CMD_DATA;
	hdr->reserved = 0;
	hdr->ch_id = id;
	hdr->pkt_len = skb->len - sizeof(struct sdio_mux_hdr);
	if (skb->len & 0x3)
		skb_put(skb, 4 - (skb->len & 0x3));

	hdr->pad_len = skb->len - (sizeof(struct sdio_mux_hdr) + hdr->pkt_len);

	DBG("%s: data %p, tail %p skb len %d pkt len %d pad len %d\n",
	    __func__, skb->data, skb->tail, skb->len,
	    hdr->pkt_len, hdr->pad_len);
	sdio_ch[id].skb = skb;
	queue_work(sdio_mux_workqueue, &work_sdio_mux_write);

write_done:
	spin_unlock_irqrestore(&sdio_ch[id].lock, flags);
	return rc;
}

int msm_rmnet_sdio_open(uint32_t id, void *priv,
			void (*receive_cb)(void *, struct sk_buff *),
			void (*write_done)(void *, struct sk_buff *))
{
	struct sdio_mux_hdr hdr;
	unsigned long flags;

	DBG("%s: opening ch %d\n", __func__, id);
	if (id >= 8)
		return -EINVAL;

	spin_lock_irqsave(&sdio_ch[id].lock, flags);
	if (sdio_ch_is_local_open(id)) {
		pr_info("%s: Already opened %d\n", __func__, id);
		spin_unlock_irqrestore(&sdio_ch[id].lock, flags);
		goto open_done;
	}

	sdio_ch[id].receive_cb = receive_cb;
	sdio_ch[id].write_done = write_done;
	sdio_ch[id].priv = priv;
	sdio_ch[id].status |= SDIO_CH_LOCAL_OPEN;
	spin_unlock_irqrestore(&sdio_ch[id].lock, flags);

	hdr.magic_num = SDIO_MUX_HDR_MAGIC_NO;
	hdr.cmd = SDIO_MUX_HDR_CMD_OPEN;
	hdr.reserved = 0;
	hdr.ch_id = id;
	hdr.pkt_len = 0;
	hdr.pad_len = 0;

	sdio_mux_write_cmd((void *)&hdr, sizeof(hdr));

open_done:
	pr_info("%s: opened ch %d\n", __func__, id);
	return 0;
}

int msm_rmnet_sdio_close(uint32_t id)
{
	struct sdio_mux_hdr hdr;
	unsigned long flags;

	DBG("%s: closing ch %d\n", __func__, id);
	spin_lock_irqsave(&sdio_ch[id].lock, flags);

	if (sdio_ch[id].skb) {
		spin_unlock_irqrestore(&sdio_ch[id].lock, flags);
		return -EINVAL;
	}

	sdio_ch[id].receive_cb = NULL;
	sdio_ch[id].priv = NULL;
	spin_unlock_irqrestore(&sdio_ch[id].lock, flags);

	hdr.magic_num = SDIO_MUX_HDR_MAGIC_NO;
	hdr.cmd = SDIO_MUX_HDR_CMD_CLOSE;
	hdr.reserved = 0;
	hdr.ch_id = id;
	hdr.pkt_len = 0;
	hdr.pad_len = 0;

	sdio_mux_write_cmd((void *)&hdr, sizeof(hdr));

	pr_info("%s: closed ch %d\n", __func__, id);
	return 0;
}

static void sdio_mux_notify(void *_dev, unsigned event)
{
	DBG("%s: event %d notified\n", __func__, event);

	/* write avail may not be enouogh for a packet, but should be fine */
	if ((event == SDIO_EVENT_DATA_WRITE_AVAIL) &&
	    sdio_write_avail(sdio_mux_ch))
		queue_work(sdio_mux_workqueue, &work_sdio_mux_write);

	if ((event == SDIO_EVENT_DATA_READ_AVAIL) &&
	    sdio_read_avail(sdio_mux_ch))
		queue_work(sdio_mux_workqueue, &work_sdio_mux_read);
}

static int msm_rmnet_sdio_probe(struct platform_device *pdev)
{
	int rc;
	static int sdio_mux_initialized;

	DBG("%s probe called\n", __func__);
	if (sdio_mux_initialized)
		return 0;

	/* is one thread gud enough for read and write? */
	sdio_mux_workqueue = create_singlethread_workqueue("msm_rmnet_sdio");
	if (!sdio_mux_workqueue)
		return -ENOMEM;

	rc = sdio_open("SDIO_RMNET_DATA", &sdio_mux_ch, NULL, sdio_mux_notify);
	if (rc < 0) {
		pr_err("%s: sido open failed %d\n", __func__, rc);
		destroy_workqueue(sdio_mux_workqueue);
		return rc;
	}

	wake_lock_init(&sdio_mux_ch_wakelock, WAKE_LOCK_SUSPEND,
		       "rmnet_sdio_mux");
	for (rc = 0; rc < 8; rc++)
		spin_lock_init(&sdio_ch[rc].lock);

	sdio_mux_initialized = 1;
	return 0;
}

static struct platform_driver msm_rmnet_sdio_driver = {
	.probe		= msm_rmnet_sdio_probe,
	.driver		= {
		.name	= "SDIO_RMNET_DATA",
		.owner	= THIS_MODULE,
	},
};

static int __init msm_rmnet_sdio_init(void)
{
	return platform_driver_register(&msm_rmnet_sdio_driver);
}

module_init(msm_rmnet_sdio_init);
MODULE_DESCRIPTION("MSM RMNET SDIO MUX");
MODULE_LICENSE("GPL v2");
