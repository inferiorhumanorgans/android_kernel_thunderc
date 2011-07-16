/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef _KGSL_G12_H
#define _KGSL_G12_H

struct kgsl_g12_device {
	struct kgsl_device dev;    /* Must be first field in this struct */
	int current_timestamp;
	int timestamp;
	wait_queue_head_t wait_timestamp_wq;
};

struct kgsl_g12_z1xx {
	unsigned int prevctx;
	unsigned int numcontext;
	struct kgsl_memdesc      cmdbufdesc;
};

extern struct kgsl_g12_z1xx g_z1xx;

irqreturn_t kgsl_g12_isr(int irq, void *data);
int kgsl_g12_setstate(struct kgsl_device *device, uint32_t flags);
struct kgsl_device *kgsl_get_g12_generic_device(void);
int kgsl_g12_regread(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int *value);
int kgsl_g12_regwrite(struct kgsl_device *device, unsigned int offsetwords,
			unsigned int value);
int __init kgsl_g12_config(struct kgsl_devconfig *,
				struct platform_device *pdev);
int kgsl_g12_getfunctable(struct kgsl_functable *ftbl);


#endif /* _KGSL_G12_H */
