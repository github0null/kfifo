/*
 * A simple kernel FIFO implementation.
 *
 * Copyright (C) 2004 Stelian Pop <stelian@popies.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef _MY_KFIFO_H
#define _MY_KFIFO_H

#ifdef __cplusplus
extern "C" {
#endif

struct kfifo {
	unsigned char *buffer;	/* the buffer holding the data */
	unsigned int size;	/* the size of the allocated buffer */
	unsigned int in;	/* data is added at offset (in % size) */
	unsigned int out;	/* data is extracted from off. (out % size) */
};

int          kfifo_init(struct kfifo *fifo, unsigned char *buffer, unsigned int size);
unsigned int kfifo_put(struct kfifo *fifo, unsigned char *buffer, unsigned int len);
unsigned int kfifo_get(struct kfifo *fifo, unsigned char *buffer, unsigned int len);
void         kfifo_reset(struct kfifo *fifo);
unsigned int kfifo_len(struct kfifo *fifo);

#ifdef __cplusplus
}
#endif

#endif
