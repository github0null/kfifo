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

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "kfifo.h"

#if defined(__CC_ARM)
    #define __STATIC_INLINE static __inline
    #define smp_mb()        __memory_changed()
    #define smp_rmb()       __memory_changed()
    #define smp_wmb()       __memory_changed()
#elif defined(__GNUC__)
    #define __STATIC_INLINE static inline
    #define smp_mb()        asm volatile("" : : : "memory")
    #define smp_rmb()       asm volatile("" : : : "memory")
    #define smp_wmb()       asm volatile("" : : : "memory")
#else
    #define __STATIC_INLINE
    #define smp_mb()
    #define smp_rmb()
    #define smp_wmb()
#endif

/* ref: https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2 */
__STATIC_INLINE bool is_power_of_2(unsigned int v) {
    return v && !(v & (v - 1));
}

/* ref: https://graphics.stanford.edu/~seander/bithacks.html#IntegerMinOrMax */
__STATIC_INLINE int min(int x, int y) {
    return y ^ ((x ^ y) & -(x < y)); // min(x, y)
}

static void         __kfifo_reset(struct kfifo *fifo);
static unsigned int __kfifo_len(struct kfifo *fifo);
static unsigned int __kfifo_put(struct kfifo *fifo, unsigned char *buffer, unsigned int len);
static unsigned int __kfifo_get(struct kfifo *fifo, unsigned char *buffer, unsigned int len);

/**
 * kfifo_init - allocates a new FIFO using a preallocated buffer
 * @buffer: the preallocated buffer to be used.
 * @size: the size of the internal buffer, this have to be a power of 2.
 *
 * Do NOT pass the kfifo to kfifo_free() after use! Simply free the
 * &struct kfifo with kfree().
 */
int kfifo_init(struct kfifo *fifo, unsigned char *buffer, unsigned int size)
{
	/* size must be a power of 2 */
	if (!is_power_of_2(size))
        return -1;
    if (fifo == NULL)
        return -1;

	fifo->buffer = buffer;
	fifo->size   = size;
	fifo->in     = fifo->out = 0;

	return 0;
}

/**
 * kfifo_reset - removes the entire FIFO contents
 * @fifo: the fifo to be emptied.
 */
void kfifo_reset(struct kfifo *fifo)
{
	__kfifo_reset(fifo);
}

/**
 * kfifo_len - returns the number of bytes available in the FIFO
 * @fifo: the fifo to be used.
 */
unsigned int kfifo_len(struct kfifo *fifo)
{
	return __kfifo_len(fifo);
}

/**
 * kfifo_put - puts some data into the FIFO
 * @fifo: the fifo to be used.
 * @buffer: the data to be added.
 * @len: the length of the data to be added.
 *
 * This function copies at most @len bytes from the @buffer into
 * the FIFO depending on the free space, and returns the number of
 * bytes copied.
 */
unsigned int kfifo_put(struct kfifo *fifo,
                       unsigned char *buffer, unsigned int len)
{
	return __kfifo_put(fifo, buffer, len);
}

/**
 * kfifo_get - gets some data from the FIFO
 * @fifo: the fifo to be used.
 * @buffer: where the data must be copied.
 * @len: the size of the destination buffer.
 *
 * This function copies at most @len bytes from the FIFO into the
 * @buffer and returns the number of copied bytes.
 */
unsigned int kfifo_get(struct kfifo *fifo,
                       unsigned char *buffer, unsigned int len)
{
	unsigned int ret;

	ret = __kfifo_get(fifo, buffer, len);

	/*
	 * optimization: if the FIFO is empty, set the indices to 0
	 * so we don't wrap the next time
	 */
	if (fifo->in == fifo->out)
		fifo->in = fifo->out = 0;

	return ret;
}

/*-------------------------------------------------------------*/
/* internal functions */
/*-------------------------------------------------------------*/

/**
 * __kfifo_reset - removes the entire FIFO contents, no locking version
 * @fifo: the fifo to be emptied.
 */
static void __kfifo_reset(struct kfifo *fifo)
{
	fifo->in = fifo->out = 0;
}

/**
 * __kfifo_len - returns the number of bytes available in the FIFO, no locking version
 * @fifo: the fifo to be used.
 */
static unsigned int __kfifo_len(struct kfifo *fifo)
{
	return fifo->in - fifo->out;
}

/**
 * __kfifo_put - puts some data into the FIFO, no locking version
 * @fifo: the fifo to be used.
 * @buffer: the data to be added.
 * @len: the length of the data to be added.
 *
 * This function copies at most @len bytes from the @buffer into
 * the FIFO depending on the free space, and returns the number of
 * bytes copied.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these functions.
 */
static unsigned int __kfifo_put(struct kfifo *fifo,
                                unsigned char *buffer, unsigned int len)
{
	unsigned int l;

	len = min(len, fifo->size - fifo->in + fifo->out);

	/*
	 * Ensure that we sample the fifo->out index -before- we
	 * start putting bytes into the kfifo.
	 */

	smp_mb();

	/* first put the data starting from fifo->in to buffer end */
	l = min(len, fifo->size - (fifo->in & (fifo->size - 1)));
	memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buffer, l);

	/* then put the rest (if any) at the beginning of the buffer */
	memcpy(fifo->buffer, buffer + l, len - l);

	/*
	 * Ensure that we add the bytes to the kfifo -before-
	 * we update the fifo->in index.
	 */

	smp_wmb();

	fifo->in += len;

	return len;
}

/**
 * __kfifo_get - gets some data from the FIFO, no locking version
 * @fifo: the fifo to be used.
 * @buffer: where the data must be copied.
 * @len: the size of the destination buffer.
 *
 * This function copies at most @len bytes from the FIFO into the
 * @buffer and returns the number of copied bytes.
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these functions.
 */
static unsigned int __kfifo_get(struct kfifo *fifo,
                                unsigned char *buffer, unsigned int len)
{
	unsigned int l;

	len = min(len, fifo->in - fifo->out);

	/*
	 * Ensure that we sample the fifo->in index -before- we
	 * start removing bytes from the kfifo.
	 */

	smp_rmb();

	/* first get the data from fifo->out until the end of the buffer */
	l = min(len, fifo->size - (fifo->out & (fifo->size - 1)));
	memcpy(buffer, fifo->buffer + (fifo->out & (fifo->size - 1)), l);

	/* then get the rest (if any) from the beginning of the buffer */
	memcpy(buffer + l, fifo->buffer, len - l);

	/*
	 * Ensure that we remove the bytes from the kfifo -before-
	 * we update the fifo->out index.
	 */

	smp_mb();

	fifo->out += len;

	return len;
}