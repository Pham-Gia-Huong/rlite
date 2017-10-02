/*
 * Packet buffers for the rlite stack.
 *
 * Copyright (C) 2015-2016 Nextworks
 * Author: Vincenzo Maffione <v.maffione@gmail.com>
 *
 * This file is part of rlite.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include "rlite-kernel.h"


struct rl_buf *
rl_buf_alloc(size_t size, size_t hdroom, gfp_t gfp)
{
    struct rl_buf *rb;
    size_t real_size = size + hdroom;
    uint8_t *kbuf;

    rb = rl_alloc(sizeof(*rb), gfp, RL_MT_BUFHDR);
    if (unlikely(!rb)) {
        PE("Out of memory\n");
        return NULL;
    }

    kbuf = rl_alloc(sizeof(*rb->raw) + real_size, gfp, RL_MT_BUFDATA);
    if (unlikely(!kbuf)) {
        rl_free(rb, RL_MT_BUFHDR);
        PE("Out of memory\n");
        return NULL;
    }

    rb->raw = (struct rl_rawbuf *)kbuf;
    rb->raw->size = real_size;
    atomic_set(&rb->raw->refcnt, 1);
    rb->pci = (struct rina_pci *)(rb->raw->buf + hdroom);
    rb->len = size;
    rb->u.rmt.compl_flow = NULL;
    INIT_LIST_HEAD(&rb->node);

    return rb;
}
EXPORT_SYMBOL(rl_buf_alloc);

struct rl_buf *
rl_buf_clone(struct rl_buf *rb, gfp_t gfp)
{
    struct rl_buf *crb;

    crb = rl_alloc(sizeof(*crb), gfp, RL_MT_BUFHDR);
    if (unlikely(!crb)) {
        return NULL;
    }

    BUG_ON(rb == NULL);
    /* Increment the raw buffer reference counter. */
    atomic_inc(&rb->raw->refcnt);

    /* Normal copy - includes pointer copy. */
    memcpy(crb, rb, sizeof(*rb));

    /* Reset some fields. */
    crb->u.rmt.compl_flow = NULL;
    INIT_LIST_HEAD(&crb->node);

    return crb;
}
EXPORT_SYMBOL(rl_buf_clone);

void
__rl_buf_free(struct rl_buf *rb)
{
    if (atomic_dec_and_test(&rb->raw->refcnt)) {
        rl_free(rb->raw, RL_MT_BUFDATA);
    }

    rl_free(rb, RL_MT_BUFHDR);
}
EXPORT_SYMBOL(__rl_buf_free);
