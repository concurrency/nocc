/*
 *	support.c - support functions
 *	Copyright (C) 2000-2004 Fred Barnes <frmb@kent.ac.uk>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*{{{  includes, etc.*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <errno.h>

#include "support.h"
#include "nocc.h"
/*}}}*/
/*{{{  local configuration*/


/*
 *	NOTE: valgrind (for debugging memory) much prefers the sys-allocator.. :)
 *	but for performance, use the slab-allocator.
 */

#undef SLAB_ALLOCATOR
#define SYS_ALLOCATOR


/*}}}*/
/*{{{  memory pools*/
/*
 *	this uses a Brinch-Hansen style half-power-of-two, address mapped allocator..
 */

#define POOL_COUNTERS

#define FIRST_POOL 4
#define N_POOLS 23

#if defined (SLAB_ALLOCATOR)
#define MAX_SLAB_SIZE 65536
#define MAX_SLAB_SHIFT 16
#define SLAB_SHIFT 18
#define CHUNK_SIZE (32 << SLAB_SHIFT)
#endif	/* defined (SLAB_ALLOCATOR) */

#ifdef POOL_COUNTERS

static unsigned int dmempools[N_POOLS << 2];
#define DMAddrSlot(X) dmempools[(X) << 2]
#define DMSizeSlot(X) dmempools[((X) << 2) | 0x1]
#define DMAvailSlot(X) dmempools[((X) << 2) | 0x2]
#define DMCountSlot(X) dmempools[((X) << 2) | 0x3]

#else	/* !POOL_COUNTERS */

static unsigned int dmempools[N_POOLS];
#define DMAddrSlot(X) dmempools[(X)]
#define DMSizeSlot(X) (-1)
#define DMAvailSlot(X) (-1)
#define DMCountSlot(X) (-1)

#endif	/* !POOL_COUNTERS */

#define SlotToSize(N)	((2 << ((((N) + FIRST_POOL) >> 1) + 2)) + (((N) & 1) ? (2 << ((((N) + FIRST_POOL) >> 1) + 1)) : 0))
#define SizeToSlot(X)	((((X) <= 32) ? 4 : (((bsr((X) - 1) - 2) << 1)) - (((X) < (3 << (bsr((X) - 1) - 1))) ? 1 : 0)) - FIRST_POOL)

#if defined (SLAB_ALLOCATOR)
static int pooladdrmap[32] = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 10, 11, 12, 13, 14, 15,
	8, 8, 10, 10, 16, 17, 18, 19,
	20, 20, 21, 21, 21, 22, 22, 22
};

/* things in the spareslabs are mapped, but not touched */
static void **spareslabs[N_POOLS];

#define SlotOfAddr(A) pooladdrmap[(((unsigned int)(A)) >> SLAB_SHIFT) & 0x1f]
#define SizeOfAddr(A) SlotToSize(SlotOfAddr(A))

/* the pool can go up to 1 gig.. */
static void *pool_baseaddr = (void *)0x80000000;
static void *pool_limit = (void *)0xc0000000;

static void *pool_nextchunkaddr;
static int pool_mapfd = -1;		/* descriptor for /dev/zero */

#endif	/* defined (SLAB_ALLOCATOR) */

static int pool_totalloc = 0;
static void *zero_block;		/* return this for 0 sized allocations */
/*}}}*/


#if defined (SLAB_ALLOCATOR)
/*{{{  static void *chunk_getnextchunk (int fd)*/
/*
 *	allocates another chunk for the pool allocator
 */
static void *chunk_getnextchunk (int fd)
{
	void *addr;

	if (pool_nextchunkaddr >= pool_limit) {
		nocc_fatal ("out of memory in chunk_getnextchunk()");
		exit (EXIT_FAILURE);
	}

	addr = mmap (pool_nextchunkaddr, CHUNK_SIZE, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, fd, 0);

	if (addr == ((void *)-1)) {
		nocc_fatal ("failed to map chunk: %s", strerror (errno));
		exit (EXIT_FAILURE);
	} else if (addr != pool_nextchunkaddr) {
		nocc_fatal ("memory mapped in the wrong place! at: %8.8x\n", (unsigned int)addr);
		exit (EXIT_FAILURE);
	}

	/* we've got a chunk.. */
	pool_nextchunkaddr += CHUNK_SIZE;

	return addr;
}
/*}}}*/
/*{{{  static void slab_addspare (int p, void *slab)*/
/*
 *	adds a spare slab to the spare-slabs list
 */
static void slab_addspare (int p, void *slab)
{
	int mspares = (SizeOfAddr (spareslabs[p]) >> 2);
	int nspares = (int)(spareslabs[p][0]);

	if (nspares == mspares - 1) {
		/* need to reallocate the list of spare slabs */
		void **newspare = (void **)dmem_new (SizeToSlot (mspares << 3));
		int j;

		memcpy (newspare, spareslabs[p], mspares << 2);
		dmem_release (spareslabs[p]);
		spareslabs[p] = newspare;
		mspares <<= 1;
		for (j=nspares+1; j<mspares; j++) {
			spareslabs[p][j] = NULL;
		}
	}
	spareslabs[p][nspares + 1] = slab;
	spareslabs[p][0] = (void *)(nspares + 1);

	return;
}
/*}}}*/
/*{{{  static void *slab_getspare (int p)*/
/*
 *	grabs a spare slab for the specified pool
 */
static void *slab_getspare (int p)
{
	/* int mspares = (SizeOfAddr (spareslabs[p]) >> 2); */
	int nspares = (int)(spareslabs[p][0]);
	void *slab;

	if (!nspares) {
		/* need a new chunk */
		void *addr = chunk_getnextchunk (pool_mapfd);
		int i;

#ifdef DEBUG
		nocc_message ("DEBUG: slab_getspare(%d) got %d bytes at 0x%8.8x -> 0x%8.8x.\n", p, CHUNK_SIZE, (unsigned int)addr, (unsigned int)(addr + CHUNK_SIZE - 1));
#endif

		for (i=0; i<32; i++) {
			int slotnum = SlotOfAddr (i << SLAB_SHIFT);
			int slotsize = SlotToSize (slotnum);
			void *qaddr = addr + (i << SLAB_SHIFT);
			void *qlim = addr + ((i + 1) << SLAB_SHIFT) - (slotsize - 7);

			for (; qaddr < qlim; qaddr += MAX_SLAB_SIZE) {
				slab_addspare (slotnum, qaddr);
			}
		}
		nspares = (int)(spareslabs[p][0]);
	}
	slab = spareslabs[p][nspares];
	spareslabs[p][0] = (void *)(nspares - 1);

	return slab;
}
/*}}}*/
/*{{{  static void pool_smashslab (int p, void *slab)*/
/*
 *	breaks a fresh slab up and places on free-lists
 */
static void pool_smashslab (int p, void *slab)
{
	void *qaddr, *qlim;
	int slotsize = SlotToSize (p);
	int nblocks;

	qaddr = slab;
	qlim = qaddr + (MAX_SLAB_SIZE - (slotsize - 7));

	for (nblocks = 0; qaddr < qlim; qaddr += slotsize, nblocks++) {
		unsigned int *tblk = (unsigned int *)qaddr;

		tblk[0] = (unsigned int)(qaddr + slotsize);
		tblk[1] = (unsigned int)0xdeadbeef;
	}

	/* fixup last block addr */
	if (!DMAddrSlot (p)) {
		((unsigned int *)(qaddr - slotsize))[0] = (unsigned int)NULL;
		((unsigned int *)(qaddr - slotsize))[1] = (unsigned int)0xfeedbeef;
	} else {
		((unsigned int *)(qaddr - slotsize))[0] = DMAddrSlot (p);
		((unsigned int *)(qaddr - slotsize))[1] = (unsigned int)0xdeadbeef;
	}

	/* put this slab on the block list */
	DMAddrSlot (p) = (unsigned int)slab;

#ifdef POOL_COUNTERS
	DMCountSlot (p) = DMCountSlot (p) + nblocks;
	DMAvailSlot (p) = DMAvailSlot (p) + nblocks;
#endif	/* POOL_COUNTERS */

	return;
}
/*}}}*/
#endif	/* defined (SLAB_ALLOCATOR) */

/*{{{  void *dmem_new (int p)*/
/*
 *	allocates a block of memory from the specified pool
 */
void *dmem_new (int p)
{
	void *thisblk = NULL;
	void *nextblk;

	if ((p < 0) || (p >= N_POOLS)) {
		return NULL;
	}
	if (!DMAddrSlot (p)) {
#if defined (SLAB_ALLOCATOR)
		/* need some more for this slot */
		void *slab = slab_getspare (p);

		pool_smashslab (p, slab);
#elif defined (SYS_ALLOCATOR)
		thisblk = malloc (SlotToSize (p));
		if (!thisblk) {
			nocc_fatal ("dmem_new(): unable to allocate %d bytes (system)\n", SlotToSize (p));
			exit (EXIT_FAILURE);
		}
		DMAddrSlot (p) = (unsigned int)thisblk;
		*(void **)thisblk = NULL;
		((unsigned int *)thisblk)[1] = 0xdeadbeef;
#ifdef POOL_COUNTERS
		DMAvailSlot (p) = DMAvailSlot (p) + 1;
#endif	/* POOL_COUNTERS */
#endif	/* !defined (SLAB_ALLOCATOR) && defined (SYS_ALLOCATOR) */
	}

	thisblk = (void *)(DMAddrSlot (p));
	nextblk = *(void **)thisblk;

	DMAddrSlot (p) = (unsigned int)nextblk;

	if (nextblk && ((unsigned int *)thisblk)[1] != 0xdeadbeef) {
		nocc_fatal ("dmem_new(): bad block magic 0x%8.8x for block at 0x%8.8x in pool %d", ((unsigned int *)thisblk)[1], (unsigned int)thisblk, p);
		exit (EXIT_FAILURE);
	}

#ifdef POOL_COUNTERS
	DMAvailSlot (p) = DMAvailSlot (p) - 1;
#endif	/* POOL_COUNTERS */

	pool_totalloc++;

	return thisblk;
}
/*}}}*/
/*{{{  void *dmem_alloc (int bytes)*/
/*
 *	allocates a specified amount of memory
 */
void *dmem_alloc (int bytes)
{
	void *ptr;

#if defined (SLAB_ALLOCATOR)
	if (bytes > MAX_SLAB_SIZE) {
		ptr = malloc (bytes);
		if (!ptr) {
			nocc_internal ("dmem_alloc(): out of memory! (wanted %d bytes)", bytes);
			exit (EXIT_FAILURE);
		}
	} else {
		ptr = dmem_new (SizeToSlot (bytes));
	}
#elif defined (SYS_ALLOCATOR)
	ptr = malloc (bytes);
	if (!ptr) {
		nocc_internal ("dmem_alloc(): out of memory! (wanted %d bytes)", bytes);
		exit (EXIT_FAILURE);
	}
#endif	/* !defined (SLAB_ALLOCATOR) && defined (SYS_ALLOCATOR) */
	return ptr;	
}
/*}}}*/
/*{{{  void *dmem_realloc (void *ptr, int obytes, int nbytes)*/
/*
 *	reallocates a block of memory
 */
void *dmem_realloc (void *ptr, int obytes, int nbytes)
{
	void *nptr;

#if defined (SLAB_ALLOCATOR)
	if ((obytes > MAX_SLAB_SIZE) || (nbytes > MAX_SLAB_SIZE)) {
		nocc_internal ("dmem_realloc(): will not reallocate outside the pool (%d -> %d)", obytes, nbytes);
	}
	if (SizeToSlot (obytes) == SizeToSlot (nbytes)) {
		/* same pool, easy :) */
		nptr = ptr;
	} else {
		nptr = dmem_new (SizeToSlot (nbytes));
		/* copy data */
		memcpy (nptr, ptr, (nbytes < obytes) ? nbytes : obytes);
		dmem_release (ptr);
	}
#elif defined (SYS_ALLOCATOR)
	nptr = dmem_alloc (nbytes);
	/* copy data */
	memcpy (nptr, ptr, (nbytes < obytes) ? nbytes : obytes);
	dmem_release (ptr);
#endif	/* !defined (SLAB_ALLOCATOR) && defined (SYS_ALLOCATOR) */

#if 0
fprintf (stderr, "dmem_realloc (0x%8.8x, %d) -> (0x%8.8x, %d)\n", (unsigned int)ptr, obytes, (unsigned int)nptr, nbytes);
#endif
	return nptr;
}
/*}}}*/
/*{{{  void dmem_release (void *ptr)*/
/*
 *	releases a block of memory
 */
void dmem_release (void *ptr)
{
#if defined (SLAB_ALLOCATOR)
	int slot;

	if ((ptr < (void *)pool_baseaddr) || (ptr >= (void *)pool_nextchunkaddr)) {
		/* not memory from the pool allocator */
		free (ptr);
		return;
	}
	slot = SlotOfAddr (ptr);

	((unsigned int *)ptr)[0] = DMAddrSlot (slot);
	((unsigned int *)ptr)[1] = 0xdeadbeef;
	DMAddrSlot (slot) = (unsigned int)ptr;
#ifdef POOL_COUNTERS
	DMAvailSlot (slot) = DMAvailSlot (slot) + 1;
#endif	/* POOL_COUNTERS */
#elif defined (SYS_ALLOCATOR)
	free (ptr);
#endif	/* !defined (SLAB_ALLOCATOR) && defined (SYS_ALLOCATOR) */

	return;
}
/*}}}*/
/*{{{  void dmem_usagedump (void)*/
/*
 *	shows a dump of the pool allocator
 */
void dmem_usagedump (void)
{
#if defined (SLAB_ALLOCATOR)
	fprintf (stderr, "memory pool at 0x%8.8x -> 0x%8.8x (%d bytes, >= %d M), limit 0x%8.8x\n", (unsigned int)pool_baseaddr, (unsigned int)pool_nextchunkaddr - 1, ((unsigned int)pool_nextchunkaddr - (unsigned int)pool_baseaddr), ((unsigned int)pool_nextchunkaddr - (unsigned int)pool_baseaddr) >> 20, (unsigned int)pool_limit);
#endif
	fprintf (stderr, "pool contents (%d allocations):\n", pool_totalloc);
	{
		int i;
#ifdef SHOW_SPARESLABS
		int j, nspares;
#endif

		for (i=0; i<N_POOLS; i++) {
			fprintf (stderr, "\t%d\t%-10d\t%-10p\t%d\t%d\t", i, DMSizeSlot(i),
					(void *)(DMAddrSlot (i)), DMAvailSlot(i), DMCountSlot(i));
#if defined (SHOW_SPARESLABS) && defined (SLAB_ALLOCATOR)
			fprintf (stderr, "[");
			nspares = (int)(spareslabs[i][0]);
			for (j=0; j < nspares; j++) {
				fprintf (stderr, "%s0x%4.4x", ((!j) ? "" : ", "), (unsigned int)(spareslabs[i][j+1]) >> MAX_SLAB_SHIFT);
			}
			fprintf (stderr, "]\n");
#elif defined (SLAB_ALLOCATOR)
			fprintf (stderr, "%d\n", (int)(spareslabs[i][0]));
#else
			fprintf (stderr, "\n");
#endif
		}
	}

	return;
}
/*}}}*/
/*{{{  void dmem_init (void)*/
/*
 *	initialises the allocator
 */
void dmem_init (void)
{
	int i;
#if defined (SLAB_ALLOCATOR)
	void *qaddr, *qlim;
	void *addr;
	char *envstr;
#endif

	zero_block = (void *)&zero_block;
#if defined (SLAB_ALLOCATOR)
	/* environment might have a base-address set */
	envstr = getenv ("NOCC_POOLBASE");
	if (envstr) {
		if (!strncmp (envstr, "0x", 2)) {
			if (sscanf (envstr + 2, "%x", (unsigned int *)&pool_baseaddr) != 1) {
				nocc_warning ("dmem_init(): mangled NOCC_POOLBASE environment ?: %s", envstr);
				pool_baseaddr = (void *)0x80000000;
			}
			pool_limit = pool_baseaddr + 0x40000000;
		}
	}
	envstr = getenv ("NOCC_POOLLIMIT");
	if (envstr) {
		if (!strncmp (envstr, "0x", 2)) {
			if (sscanf (envstr + 2, "%x", (unsigned int *)&pool_limit) != 1) {
				nocc_warning ("dmem_init(): mangled NOCC_POOLLIMIT environment ?: %s", envstr);
				pool_limit = pool_baseaddr + 0x40000000;
			}
		}
	}
	if (pool_baseaddr >= pool_limit) {
		nocc_fatal ("dmem_init(): memory pool base >= limit! (0x%8.8x, 0x%8.8x)", (unsigned int)pool_baseaddr, (unsigned int)pool_limit);
		exit (EXIT_FAILURE);
	}

	pool_nextchunkaddr = pool_baseaddr;

	pool_mapfd = open ("/dev/zero", O_RDWR);
	if (pool_mapfd < 0) {
		nocc_fatal ("dmem_init(): failed to open /dev/zero: %s", strerror (errno));
		exit (1);
	}

#ifdef DEBUG
	nocc_message ("DEBUG: dmem_init() allocating %d bytes at 0x%8.8x  (break at 0x%8.8x)", CHUNK_SIZE, pool_baseaddr, (unsigned int)sbrk (0));
#endif
	addr = chunk_getnextchunk (pool_mapfd);

#ifdef DEBUG
	nocc_message ("DEBUG: dmem_init() got %d bytes at 0x%8.8x -> 0x%8.8x", CHUNK_SIZE, (unsigned int)addr, (unsigned int)(addr + CHUNK_SIZE - 1));
	nocc_message ("initialising pool allocator..");
#endif
#endif	/* defined (SLAB_ALLOCATOR) */

	for (i=0; i<N_POOLS; i++) {
		DMAddrSlot(i) = (unsigned int)NULL;
#ifdef POOL_COUNTERS
		DMSizeSlot(i) = SlotToSize (i);
		DMAvailSlot(i) = 0;
		DMCountSlot(i) = 0;
#endif	/* POOL_COUNTERS */
#if defined (SLAB_ALLOCATOR)
		spareslabs[i] = NULL;
#endif	/* defined (SLAB_ALLOCATOR) */
	}

#if defined (SLAB_ALLOCATOR)
	for (i=0; i<32; i++) {
		int slotnum = SlotOfAddr (i << SLAB_SHIFT);
		int slotsize = SlotToSize (slotnum);
		int j;

		/* pick up a single slab first, then queue the others on the spare slab-list */
		qaddr = addr + (i << SLAB_SHIFT);
		qlim = qaddr + (MAX_SLAB_SIZE - (slotsize - 7));

		pool_smashslab (slotnum, qaddr);

		/* if this is the first round, setup spareslabs */
		if (!i) {
			for (j=0; j<N_POOLS; j++) {
				int k;

				spareslabs[j] = (void **)dmem_new (0);
				for (k=0; k<8; k++) {
					spareslabs[j][k] = NULL;
				}
			}
		}

		/* add remaining slabs to spare slabs list */
		qaddr = addr + ((i << SLAB_SHIFT) + MAX_SLAB_SIZE);
		qlim = addr + (((i + 1) << SLAB_SHIFT) - (slotsize - 7));
		for (; qaddr < qlim; qaddr += MAX_SLAB_SIZE) {
			slab_addspare (slotnum, qaddr);
		}
	}
#endif	/* defined (SLAB_ALLOCATOR) */

	return;
}
/*}}}*/
/*{{{  void dmem_shutdown (void)*/
/*
 *	shuts down the pool allocator
 */
void dmem_shutdown (void)
{
#if defined (SLAB_ALLOCATOR)
	void *qaddr;
#endif	/* defined (SLAB_ALLOCATOR) */
	int i;

#if defined (SLAB_ALLOCATOR)
	if (pool_mapfd < 0) {
		nocc_internal ("dmem_shutdown() already shut-down ?");
	}
	for (qaddr = (void *)pool_baseaddr; qaddr < pool_nextchunkaddr; qaddr += CHUNK_SIZE) {
		munmap (qaddr, CHUNK_SIZE);
	}
	close (pool_mapfd);
	pool_mapfd = -1;
#endif	/* defined (SLAB_ALLOCATOR) */

	for (i=0; i<N_POOLS; i++) {
		DMAddrSlot(i) = (unsigned int)NULL;
#ifdef POOL_COUNTERS
		DMSizeSlot(i) = SlotToSize (i);
		DMAvailSlot(i) = 0;
		DMCountSlot(i) = 0;
#endif	/* POOL_COUNTERS */
#if defined (SLAB_ALLOCATOR)
		spareslabs[i] = NULL;
#endif	/* defined (SLAB_ALLOCATOR) */
	}
	return;
}
/*}}}*/


#ifdef TRACE_MEMORY
/*{{{  memory-trace related stuff*/

#define SS_FILE_SIZE 64

typedef struct TAG_ss_memblock {
	struct TAG_ss_memblock *next, *prev;
	void *ptr;
	unsigned int vptr;
	size_t size;
	int line;
	char file[SS_FILE_SIZE];
} ss_memblock;

static ss_memblock *ss_head = NULL;
static ss_memblock *ss_tail = NULL;
static int ss_numalloc = 0;
static int ss_numfree = 0;
static int ss_numrealloc = 0;
static int ss_numhardrealloc = 0;


/*{{{  void ss_cleanup (void)*/
/*
 *	called on program exit to note what didn't get cleaned up
 */
void ss_cleanup (void)
{
	ss_memblock *tmpblk;

	fprintf (stderr, "%d blocks allocated\n", ss_numalloc);
	fprintf (stderr, "%d blocks freed\n", ss_numfree);
	fprintf (stderr, "%d blocks re-allocated\n", ss_numrealloc);
	fprintf (stderr, "%d blocks copied for re-allocation\n", ss_numhardrealloc);
	fprintf (stderr, "left-over memory blocks:\n");
	for (tmpblk = ss_head; tmpblk; tmpblk = tmpblk->next) {
		fprintf (stderr, "0x%-8x  %-8d  %s:%d\n", tmpblk->vptr, tmpblk->size, tmpblk->file, tmpblk->line);
	}
	return;
}
/*}}}*/
/*{{{  void ss_insert_blk (ss_memblock *blk)*/
/*
 *	inserts a memory block in the list
 */
void ss_insert_blk (ss_memblock *blk)
{
	ss_memblock *tb;

	if (!ss_head && !ss_tail) {
		ss_head = ss_tail = blk;
	} else if (!ss_head || !ss_tail) {
		nocc_fatal ("ss_head = %p, ss_tail = %p", ss_head, ss_tail);
		_exit (1);
	} else {
		for (tb=ss_head; tb && (tb->vptr < blk->vptr); tb = tb->next);
		if (!tb) {
			/* insert at end */
			blk->prev = ss_tail;
			ss_tail->next = blk;
			ss_tail = blk;
		} else if (!tb->prev) {
			/* insert at start */
			blk->next = ss_head;
			ss_head->prev = blk;
			ss_head = blk;
		} else {
			/* insert before `tb' */
			blk->prev = tb->prev;
			blk->next = tb;
			tb->prev->next = blk;
			tb->prev = blk;
		}
	}
	return;
}
/*}}}*/
/*{{{  void ss_remove_blk (ss_memblock *blk)*/
/*
 *	removes (disconnects) a memory block from the list
 */
void ss_remove_blk (ss_memblock *blk)
{
	if (blk->prev && blk->next) {
		blk->prev->next = blk->next;
		blk->next->prev = blk->prev;
	} else if (!blk->prev) {
		ss_head = blk->next;
		ss_head->prev = NULL;
	} else if (!blk->next) {
		ss_tail = blk->prev;
		ss_tail->next = NULL;
	} else if (!blk->prev && !blk->next) {
		ss_head = ss_tail = NULL;
	}
	return;
}
/*}}}*/
/*}}}*/
#endif	/* TRACE_MEMORY */

/*{{{  void *smalloc (size_t length)*/
/*
 *	allocates some memory
 */
#ifdef TRACE_MEMORY
void *ss_malloc (const char *file, const int line, size_t length)
#else
void *smalloc (size_t length)
#endif
{
	void *tmp;

	tmp = dmem_alloc (length);
	memset (tmp, 0, length);
	#ifdef TRACE_MEMORY
	{
		ss_memblock *tmpblk;

		ss_numalloc++;
		tmpblk = (ss_memblock *)dmem_alloc (sizeof (ss_memblock));
		tmpblk->prev = tmpblk->next = NULL;
		tmpblk->ptr = tmp;
		tmpblk->vptr = (unsigned int)tmp;
		tmpblk->size = length;
		strncpy (tmpblk->file, file, SS_FILE_SIZE);
		tmpblk->line = line;
		ss_insert_blk (tmpblk);
	}
	#endif	/* TRACE_MEMORY */
	return tmp;
}
/*}}}*/
/*{{{  void *srealloc (void *ptr, size_t old_size, size_t new_size)*/
/*
 *	re-allocates a memory block, moving it entirely if necessary
 */
#ifdef TRACE_MEMORY
void *ss_realloc (const char *file, const int line, void *ptr, size_t old_size, size_t new_size)
#else
void *srealloc (void *ptr, size_t old_size, size_t new_size)
#endif
{
	void *tmp;

	if (!ptr || !old_size) {
#ifdef TRACE_MEMORY
		tmp = ss_malloc (file, line, new_size);
#else
		tmp = dmem_alloc (new_size);
#endif
	} else {
#if defined (SLAB_ALLOCATOR)
		/* need to be slightly more smart with the pool allocator */

		if ((old_size <= MAX_SLAB_SIZE) && (new_size <= MAX_SLAB_SIZE)) {
			/* pool-pool reallocation */
#ifdef TRACE_MEMORY
			ss_memblock *tmpblk;
			for (tmpblk = ss_head; tmpblk; tmpblk = tmpblk->next) {
				if (tmpblk->ptr == ptr) {
					break;
				}
			}
#endif
			tmp = dmem_realloc (ptr, old_size, new_size);
#ifdef TRACE_MEMORY
			ss_numrealloc++;
			if (!tmpblk) {
				fprintf (stderr, "%s: serious: attempt to srealloc() non-allocated memory in %s:%d\n", progname, file, line);
			} else {
				tmpblk->ptr = tmp;
				tmpblk->vptr = (unsigned int)tmp;
			}
#endif
		} else if (old_size <= MAX_SLAB_SIZE) {
			/* pool-malloc reallocation (growing) */
#ifdef TRACE_MEMORY
			tmp = ss_malloc (file, line, new_size);
			memcpy (tmp, ptr, old_size);
			ss_free (file, line, ptr);
#else
			tmp = dmem_alloc (new_size);
			memcpy (tmp, ptr, old_size);
			sfree (ptr);
#endif
		} else if (new_size <= MAX_SLAB_SIZE) {
			/* malloc-pool reallocation (shrinking) */
#ifdef TRACE_MEMORY
			tmp = ss_malloc (file, line, new_size);
			memcpy (tmp, ptr, new_size);
			ss_free (file, line, ptr);
#else
			tmp = dmem_alloc (new_size);
			memcpy (tmp, ptr, new_size);
			sfree (ptr);
#endif
		} else
#endif /* SLAB_ALLOCATOR */
		{
			/* malloc-malloc reallocation */
#ifdef TRACE_MEMORY
			ss_memblock *tmpblk;
			for (tmpblk = ss_head; tmpblk; tmpblk = tmpblk->next) {
				if (tmpblk->ptr == ptr) {
					break;
				}
			}
#endif
			tmp = realloc (ptr, new_size);
			if (!tmp) {
#ifdef TRACE_MEMORY
				tmp = ss_malloc (file, line, new_size);
#else
				tmp = smalloc (new_size);
#endif
				if (new_size > old_size) {
					memcpy (tmp, ptr, old_size);
				} else {
					memcpy (tmp, ptr, new_size);
				}
#ifdef TRACE_MEMORY
				ss_free (file, line, ptr);
#else
				sfree (ptr);
#endif
			} else 
#ifdef TRACE_MEMORY
			{	
				ss_numrealloc++;
				if (!tmpblk) {
					fprintf (stderr, "%s: serious: attempt to srealloc() non-allocated memory in %s:%d\n", progname, file, line);
				} else {
					tmpblk->ptr = tmp;
					tmpblk->vptr = (unsigned int)tmp;
				}
#endif
				if (new_size > old_size) {
				memset (tmp + old_size, 0, new_size - old_size);
#ifdef TRACE_MEMORY
				}
#endif
			}
		}
	}
	return tmp;
}
/*}}}*/
/*{{{  void sfree (void *ptr)*/
/*
 *	frees previously allocated memory
 */
#ifdef TRACE_MEMORY
void ss_free (const char *file, const int line, void *ptr)
#else
void sfree (void *ptr)
#endif
{
	if (ptr) {
		#ifdef TRACE_MEMORY
			ss_memblock *tmpblk;

			ss_numfree++;
			for (tmpblk = ss_head; tmpblk; tmpblk = tmpblk->next) {
				if (tmpblk->ptr == ptr) {
					break;
				}
			}
			if (!tmpblk) {
				fprintf (stderr, "%s: serious: attempt to sfree() non-allocated memory (%p) in %s:%d\n", progname, ptr, file, line);
			} else {
				ss_remove_blk (tmpblk);
				free (tmpblk);
			}
		#endif
		dmem_release (ptr);
	}
	return;
}
/*}}}*/


/*{{{  char *string_ndup (const char *str, int length)*/
/*
 *	duplicates a chunk of string
 */
#ifdef TRACE_MEMORY
char *ss_string_ndup (const char *file, const int line, const char *str, int length)
#else
char *string_ndup (const char *str, int length)
#endif
{
	char *tmp;

#ifdef TRACE_MEMORY
	tmp = (char *)ss_malloc (file, line, length + 1);
#else
	tmp = (char *)smalloc (length + 1);
#endif
	memcpy (tmp, str, length);
	tmp[length] = '\0';
	return tmp;
}
/*}}}*/
/*{{{  char *string_dup (const char *str)*/
/*
 *	duplicates a string
 */
#ifdef TRACE_MEMORY
char *ss_string_dup (const char *file, const int line, const char *str)
#else
char *string_dup (const char *str)
#endif
{
#ifdef TRACE_MEMORY
	return ss_string_ndup (file, line, str, strlen (str));
#else
	return string_ndup (str, strlen (str));
#endif
}
/*}}}*/
/*{{{  void *mem_ndup (const void *ptr, int length)*/
/*
 *	duplicates a bit of memory
 */
#ifdef TRACE_MEMORY
void *ss_mem_ndup (const char *file, const int line, const void *ptr, int length)
#else
void *mem_ndup (const void *ptr, int length)
#endif
{
	void *tmp;

#ifdef TRACE_MEMORY
	tmp = ss_malloc (file, line, length);
#else
	tmp = smalloc (length);
#endif
	memcpy (tmp, ptr, length);
	return tmp;
}
/*}}}*/

/*{{{  void da_init (int *cur, int *max, void ***array)*/
/*
 *	initialises a dynamic array
 */
void da_init (int *cur, int *max, void ***array)
{
	*cur = 0;
	*max = 0;
	*array = NULL;
	return;
}
/*}}}*/
/*{{{  void da_additem (int *cur, int *max, void ***array, void *item)*/
/*
 *	adds an item to a dynamic array (at the end of it)
 */
#ifdef TRACE_MEMORY
void ss_da_additem (const char *file, const int line, int *cur, int *max, void ***array, void *item)
#else
void da_additem (int *cur, int *max, void ***array, void *item)
#endif
{
	if (*max == 0) {
#ifdef TRACE_MEMORY
		*array = (void **)ss_malloc (file, line, 8 * sizeof (void *));
#else
		*array = (void **)smalloc (8 * sizeof (void *));
#endif
		*max = 8;
	} else if (*cur == *max) {
#ifdef TRACE_MEMORY
		*array = (void **)ss_realloc (file, line, (void *)(*array), *max * sizeof(void *), (*max + 8) * sizeof(void *));
#else
		*array = (void **)srealloc ((void *)(*array), *max * sizeof(void *), (*max + 8) * sizeof(void *));
#endif
		*max = *max + 8;
	}
	(*array)[*cur] = item;
	*cur = *cur + 1;
	return;
}
/*}}}*/
/*{{{  void da_insertitem (int *cur, int *max, void ***array, void *item, int idx)*/
/*
 *	inserts an item in a dynamic array (first position is 0)
 */
#ifdef TRACE_MEMORY
void ss_da_insertitem (const char *file, const int line, int *cur, int *max, void ***array, void *item, int idx)
#else
void da_insertitem (int *cur, int *max, void ***array, void *item, int idx)
#endif
{
	int i;

	if (*max == 0) {
#ifdef TRACE_MEMORY
		*array = (void **)ss_malloc (file, line, 8 * sizeof (void *));
#else
		*array = (void **)smalloc (8 * sizeof (void *));
#endif
		*max = 8;
	} else if (*cur == *max) {
#ifdef TRACE_MEMORY
		*array = (void **)ss_realloc (file, line, (void *)(*array), *max * sizeof(void *), (*max + 8) * sizeof(void *));
#else
		*array = (void **)srealloc ((void *)(*array), *max * sizeof(void *), (*max + 8) * sizeof(void *));
#endif
		*max = *max + 8;
	}
	if (idx > *cur) {
		idx = *cur;
	}
	for (i = (*cur - 1); i >= idx; i--) {
		(*array)[i+1] = (*array)[i];
	}
	(*array)[idx] = item;
	*cur = *cur + 1;
	return;
}
/*}}}*/
/*{{{  int da_maybeadditem (int *cur, int *max, void ***array, void *item)*/
/*
 *	adds an item to a dynamic array, but only if it's not there already
 */
#ifdef TRACE_MEMORY
int ss_da_maybeadditem (const char *file, const int line, int *cur, int *max, void ***array, void *item)
#else
int da_maybeadditem (int *cur, int *max, void ***array, void *item)
#endif
{
	int idx;

	for (idx = 0; idx < *cur; idx++) {
		if ((*array)[idx] == item) {
			return 0;
		}
	}
#ifdef TRACE_MEMORY
	ss_da_additem (file, line, cur, max, array, item);
#else
	da_additem (cur, max, array, item);
#endif
	return 1;
}
/*}}}*/
/*{{{  void da_delitem (int *cur, int *max, void ***array, int idx)*/
/*
 *	removes an item from a dynamic array, at a specified index
 */
void da_delitem (int *cur, int *max, void ***array, int idx)
{
	if (idx >= *cur) {
		nocc_fatal ("item at index %d in array at %p (%d,%d) does not exist!", idx, *array, *cur, *max);
		exit (EXIT_FAILURE);
	}
	if (idx == (*cur - 1)) {
		*cur = *cur - 1;
	} else {
		void **walk;
		int i = (*cur - idx) - 1;

		for (walk = (*array) + idx; i > 0; walk++, i--) {
			walk[0] = walk[1];
		}
		*cur = *cur - 1;
	}
	return;
}
/*}}}*/
/*{{{  void da_rmitem (int *cur, int *max, void ***array, void *item)*/
/*
 *	removes an item from a dynamic array, based on its value
 *	(scans array and calls da_delitem)
 */
void da_rmitem (int *cur, int *max, void ***array, void *item)
{
	int idx;

	for (idx = 0; idx < *cur; idx++) {
		if ((*array)[idx] == item) {
			da_delitem (cur, max, array, idx);
			idx--;
		}
	}
	return;
}
/*}}}*/
/*{{{  void da_trash (int *cur, int *max, void ***array)*/
/*
 *	trashes a dynamic array and resets it to zero.  doesn't do anything with any contents
 */
#ifdef TRACE_MEMORY
void ss_da_trash (const char *file, const int line, int *cur, int *max, void ***array)
#else
void da_trash (int *cur, int *max, void ***array)
#endif
{
	if (*max && *array) {
#ifdef TRACE_MEMORY
		ss_free (file, line, *array);
#else
		sfree (*array);
#endif
	}
	*array = NULL;
	*cur = 0;
	*max = 0;
	return;
}
/*}}}*/
/*{{{  void da_qsort (void **array, int first, int last, int (*cfcn)(void *, void *))*/
/*
 *	does a quick-sort on a dynamic array using the compare function given
 */
void da_qsort (void **array, int first, int last, int (*cfcn)(void *, void *))
{
	int i, j;
	void *pivot;

#if 0
fprintf (stderr, "da_qsort(): array=0x%8.8x, first=%d, last=%d\n", (unsigned int)array, first, last);
#endif
	pivot = array[(first + last) >> 1];
	i = first;
	j = last;
	while (i <= j) {
		void *tmp;

#if 0
fprintf (stderr, "da_qsort(): i=%d, j=%d, pivot=(0x%8.8x), array[i]=(0x%8.8x), array[j]=(0x%8.8x)\n", i, j, (unsigned int)pivot, (unsigned int)array[i], (unsigned int)array[j]);
#endif
		while (cfcn (array[i], pivot) < 0) {
			i++;
		}
		while (cfcn (array[j], pivot) > 0) {
			j--;
		}
		if (i <= j) {
			tmp = array[i];
			array[i] = array[j];
			array[j] = tmp;
			i++;
			j--;
		}
	}
	if (j > first) {
		da_qsort (array, first, j, cfcn);
	}
	if (i < last) {
		da_qsort (array, i, last, cfcn);
	}
	return;
}
/*}}}*/
/*{{{  void da_setsize (int *cur, int *max, void ***array, int size)*/
/*
 *	sets the size of a dynamic array
 */
#ifdef TRACE_MEMORY
void ss_da_setsize (const char *file, const int line, int *cur, int *max, void ***array, int size)
#else
void da_setsize (int *cur, int *max, void ***array, int size)
#endif
{
	if (size < 0) {
		nocc_internal ("dynarray_setsize to %d", size);
	} else if (!size) {
#ifdef TRACE_MEMORY
		ss_da_trash (file, line, cur, max, array);
#else
		da_trash (cur, max, array);
#endif
	} else {
		if (*max == 0) {
#ifdef TRACE_MEMORY
			*array = (void **)ss_malloc (file, line, size * sizeof (void *));
#else
			*array = (void **)smalloc (size * sizeof (void *));
#endif
			*max = size;
			*cur = size;
		} else if (size > *max) {
#ifdef TRACE_MEMORY
			*array = (void **)ss_realloc (file, line, (void *)(*array),
					*max * sizeof(void  *), size * sizeof(void *));
#else
			*array = (void **)srealloc ((void *)(*array), *max * sizeof(void *),
					size * sizeof(void *));
#endif
			*max = size;
			*cur = size;
		} else {
			*cur = size;
		}
	}
}
/*}}}*/
/*{{{  void da_setmax (int *cur, int *max, void ***array, int size)*/
/*
 *	sets the maximum size of a dynamic array
 */
#ifdef TRACE_MEMORY
void ss_da_setmax (const char *file, const int line, int *cur, int *max, void ***array, int size)
#else
void da_setmax (int *cur, int *max, void ***array, int size)
#endif
{
	if (size < 0) {
		nocc_internal ("dynarray_setmax to %d", size);
	} else if (!size) {
#ifdef TRACE_MEMORY
		ss_da_trash (file, line, cur, max, array);
#else
		da_trash (cur, max, array);
#endif
	} else {
		if (*max == 0) {
#ifdef TRACE_MEMORY
			*array = (void **)ss_malloc (file, line, size * sizeof (void *));
#else
			*array = (void **)smalloc (size * sizeof (void *));
#endif
			*max = size;
			*cur = 0;
		} else if (size > *max) {
#ifdef TRACE_MEMORY
			*array = (void **)ss_realloc (file, line, (void *)(*array),
					*max * sizeof(void *), size * sizeof(void *));
#else
			*array = (void **)srealloc ((void *)(*array), *max * sizeof(void *),
					size * sizeof(void *));
#endif
			*max = size;
		} else if (size < *max) {
			/* making the array smaller */
#ifdef TRACE_MEMORY
			*array = (void **)ss_realloc (file, line, (void *)(*array),
					*max * sizeof(void *), size * sizeof(void *));
#else
			*array = (void **)srealloc ((void *)(*array), *max * sizeof(void *),
					size * sizeof(void *));
#endif
			*max = size;
			if (*cur > *max) {
				*cur = *max;
			}
		}
	}
}
/*}}}*/
/*{{{  void da_copy (int srccur, int srcmax, void **srcarray, int *dstcur, int *dstmax, void ***dstarray)*/
/*
 *	copies a dynamic array (and its contents)
 */
#ifdef TRACE_MEMORY
void ss_da_copy (const char *file, const int line, int srccur, int srcmax, void **srcarray, int *dstcur, int *dstmax, void ***dstarray)
#else
void da_copy (int srccur, int srcmax, void **srcarray, int *dstcur, int *dstmax, void ***dstarray)
#endif
{
	if (!srccur) {
		/* nothing to copy! */
		return;
	}
	if (!(*dstmax)) {
		/* empty destination, allocate it */
#ifdef TRACE_MEMORY
		*dstarray = (void **)ss_malloc (file, line, srcmax * sizeof (void *));
#else
		*dstarray = (void **)smalloc (srcmax * sizeof (void *));
#endif
		*dstmax = srcmax;
		*dstcur = 0;
	} else if ((*dstcur + srccur) >= *dstmax) {
		/* destination (maybe) needs reallocating */
#ifdef TRACE_MEMORY
		*dstarray = (void **)ss_realloc (file, line, (void *)(*dstarray), (*dstmax) * sizeof(void *),
				(*dstcur + srccur + 1) * sizeof(void *));
#else
		*dstarray = (void **)srealloc ((void *)(*dstarray), (*dstmax) * sizeof(void *),
				(*dstcur + srccur + 1) * sizeof(void *));
#endif
		*dstmax = (*dstcur + srccur + 1);
	}
	/* stick elements from srcarray[0..srccur-1] into dstarray[dstcur..] */
	memcpy (&((*dstarray)[*dstcur]), &((srcarray)[0]), srccur * sizeof (void *));
	*dstcur = *dstcur + srccur;

	return;
}
/*}}}*/


/*{{{  void sh_init (int *bsizes, void ***table, char ***keys, int size)*/
/*
 *	initialises a string-hash
 */
void sh_init (int *bsizes, void ***table, char ***keys, int size)
{
	int i;

	for (i=0; i<size; i++) {
		bsizes[i] = 0;
		table[i] = NULL;
		keys[i] = NULL;
	}
	return;
}
/*}}}*/
/*{{{  static unsigned int sh_hashcode (char *str, int bitsize)*/
/*
 *	returns a hash-code for some string
 */
static unsigned int sh_hashcode (char *str, int bitsize)
{
	unsigned int hc = (0x55a55a55 << bitsize);

	for (; *str != '\0'; str++) {
		unsigned int chunk = (unsigned int)*str;
		unsigned int top;

		hc ^= chunk;
		top = (hc >> ((sizeof(unsigned int) * 8) - bitsize));
		hc <<= (bitsize >> 1);
		hc ^= top;
	}
	return hc;
}
/*}}}*/
/*{{{  void sh_insert (int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key)*/
/*
 *	inserts an item into a string-hash
 */
#ifdef TRACE_MEMORY
void ss_sh_insert (const char *file, const int line, int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key)
#else
void sh_insert (int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key)
#endif
{
	unsigned int hcode = sh_hashcode (key, bitsize);
	int bucket = hcode & ((1 << bitsize) - 1);

#if 0 && defined(DEBUG)
fprintf (stderr, "sh_insert: adding item [%s] (0x%8.8x)\n", key, (unsigned int)item);
#endif
	if (!bsizes[bucket] || !table[bucket] || !keys[bucket]) {
#ifdef TRACE_MEMORY
		table[bucket] = (void **)ss_malloc (file, line, sizeof (void *));
#else
		table[bucket] = (void **)smalloc (sizeof (void *));
#endif
		table[bucket][0] = item;

#ifdef TRACE_MEMORY
		keys[bucket] = (char **)ss_malloc (file, line, sizeof (char *));
#else
		keys[bucket] = (char **)smalloc (sizeof (char *));
#endif
		keys[bucket][0] = key;

		bsizes[bucket] = 1;
	} else {
#ifdef TRACE_MEMORY
		table[bucket] = (void **)ss_realloc (file, line, (void *)table[bucket],
					bsizes[bucket] * sizeof(void *),
					(bsizes[bucket] + 1) * sizeof(void *));
#else
		table[bucket] = (void **)srealloc ((void *)table[bucket],
					bsizes[bucket] * sizeof(void *),
					(bsizes[bucket] + 1) * sizeof(void *));
#endif
		table[bucket][bsizes[bucket]] = item;
#ifdef TRACE_MEMORY
		keys[bucket] = (char **)ss_realloc (file, line, (void *)keys[bucket],
					bsizes[bucket] * sizeof(char *),
					(bsizes[bucket] + 1) * sizeof(char *));
#else
		keys[bucket] = (char **)srealloc ((void *)keys[bucket],
					bsizes[bucket] * sizeof(char *),
					(bsizes[bucket] + 1) * sizeof(char *));
#endif
		keys[bucket][bsizes[bucket]] = key;

		bsizes[bucket]++;
	}
	return;
}
/*}}}*/
/*{{{  void sh_remove (int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key)*/
/*
 *	removes an item from a string-hash
 */
#ifdef TRACE_MEMORY
void ss_sh_remove (const char *file, const int line, int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key)
#else
void sh_remove (int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key)
#endif
{
	unsigned int hcode = sh_hashcode (key, bitsize);
	int bucket = hcode & ((1 << bitsize) - 1);

	if (!bsizes[bucket] || !table[bucket] || !keys[bucket]) {
		return;
	} else {
		int i;

		for (i=0; i<bsizes[bucket]; i++) {
			if ((sh_hashcode (keys[bucket][i], bitsize) == hcode) && (item == table[bucket][i]) && (!strcmp (keys[bucket][i], key))) {
				/* shuffle up others */
				for (; i<(bsizes[bucket] - 1); i++) {
					table[bucket][i] = table[bucket][i+1];
					keys[bucket][i] = keys[bucket][i+1];
				}

				/* nullify the last entries */
				table[bucket][i] = NULL;
				keys[bucket][i] = NULL;

				/* now we have to make smaller */
				if (bsizes[bucket] == 1) {
#ifdef TRACE_MEMORY
					ss_free (file, line, table[bucket]);
					ss_free (file, line, keys[bucket]);
#else
					sfree (table[bucket]);
					sfree (keys[bucket]);
#endif
					table[bucket] = NULL;
					keys[bucket] = NULL;
				} else {
#ifdef TRACE_MEMORY
					table[bucket] = (void **)ss_realloc (file, line, (void *)table[bucket], bsizes[bucket] * sizeof (void *),
							(bsizes[bucket] - 1) * sizeof (void *));
					keys[bucket] = (char **)ss_realloc (file, line, (void *)keys[bucket], bsizes[bucket] * sizeof (void *),
							(bsizes[bucket] - 1) * sizeof (void *));
#else
					table[bucket] = (void **)srealloc ((void *)table[bucket], bsizes[bucket] * sizeof (void *),
							(bsizes[bucket] - 1) * sizeof (void *));
					keys[bucket] = (char **)srealloc ((void *)keys[bucket], bsizes[bucket] * sizeof (void *),
							(bsizes[bucket] - 1) * sizeof (void *));
#endif
				}
				bsizes[bucket] = bsizes[bucket] - 1;

				return;
			}
		}
	}
	nocc_warning ("sh_remove(): item [0x%8.8x:%s] not in stringhash", (unsigned int)item, key);
	return;
}
/*}}}*/
/*{{{  void *sh_lookup (int *bsizes, void ***table, char ***keys, int bitsize, char *match)*/
/*
 *	looks up an item in a string-hash
 */
void *sh_lookup (int *bsizes, void ***table, char ***keys, int bitsize, char *match)
{
	unsigned int hcode = sh_hashcode (match, bitsize);
	int bucket = hcode & ((1 << bitsize) - 1);

	if (!bsizes[bucket] || !table[bucket] || !keys[bucket]) {
		return NULL;
	} else {
		int i;

		for (i=0; i<bsizes[bucket]; i++) {
			if ((sh_hashcode (keys[bucket][i], bitsize) == hcode) && (!strcmp (keys[bucket][i], match))) {
#if 0 && defined(DEBUG)
fprintf (stderr, "sh_lookup: match for [%s] found item 0x%8.8x\n", match, (unsigned int)(table[bucket][i]));
#endif
				return table[bucket][i];
			}
		}
	}
	return NULL;
}
/*}}}*/
/*{{{  void sh_dump (FILE *stream, int *bsizes, void ***table, char ***keys, int size)*/
/*
 *	dumps the contents of a string-hash (debugging)
 */
void sh_dump (FILE *stream, int *bsizes, void ***table, char ***keys, int size)
{
	int i;

	fprintf (stream, "hash-table size: %d buckets\n", size);
	for (i=0; i < size; i++) {
		fprintf (stream, "bucket %d:\tsize %d:\t", i, bsizes[i]);
		if (bsizes[i]) {
			int j;

			for (j=0; j<bsizes[i]; j++) {
				fprintf (stream, "%s%s (0x%8.8x)", (!j ? "" : ", "), keys[i][j], (unsigned int)(table[i][j]));
			}
		}
		fprintf (stream, "\n");
	}
}
/*}}}*/
/*{{{  void sh_walk (int *bsizes, void ***table, char ***keys, int size, void (*func)(void *, char *, void *), void *p)*/
/*
 *	walks the contents of a string-hash
 */
void sh_walk (int *bsizes, void ***table, char ***keys, int size, void (*func)(void *, char *, void *), void *p)
{
	int i, j;

	for (i=0; i<size; i++) {
		if (bsizes[i]) {
			for (j=0; j<bsizes[i]; j++) {
				func (table[i][j], keys[i][j], p);
			}
		}
	}
	return;
}
/*}}}*/
/*{{{  void sh_trash (int *bsizes, void ***table, char ***keys, int size)*/
/*
 *	destroys a string-hash
 */
#ifdef TRACE_MEMORY
void ss_sh_trash (const char *file, const int line, int *bsizes, void ***table, char ***keys, int size)
#else
void sh_trash (int *bsizes, void ***table, char ***keys, int size)
#endif
{
	int i;

	for (i=0; i<size; i++) {
		if (table[i]) {
#ifdef TRACE_MEMORY
			ss_free (file, line, table[i]);
#else
			sfree (table[i]);
#endif
		}
		if (keys[i]) {
#ifdef TRACE_MEMORY
			ss_free (file, line, keys[i]);
#else
			sfree (keys[i]);
#endif
		}
	}

	return;
}
/*}}}*/


/*{{{  void ph_init (int *bsizes, void ***table, void ***keys, int *szptr, int *bszptr, void **fnptr, int bitsize)*/
/*
 *	initialises a pointer-hash
 */
void ph_init (int *bsizes, void ***table, void ***keys, int *szptr, int *bszptr, void **fnptr, int bitsize)
{
	int i;
	int size = (1 << bitsize);

	*szptr = size;
	*bszptr = bitsize;
	*fnptr = (void *)ph_lookup;
	for (i=0; i<size; i++) {
		bsizes[i] = 0;
		table[i] = NULL;
		keys[i] = NULL;
	}
	return;
}
/*}}}*/
/*{{{  static unsigned int ph_hashcode (void *ptr, int bitsize)*/
/*
 *	returns a pointer-hash for some pointer
 */
static unsigned int ph_hashcode (void *ptr, int bitsize)
{
	int i;
	unsigned int hc = (0x56756789 << bitsize);
	char *data = (char *)ptr;

	for (i=0; i<sizeof(int); i++) {
		unsigned int chunk = (unsigned int)data[i];
		unsigned int top;

		hc ^= chunk;
		top = (hc >> ((sizeof (unsigned int) * 8) - bitsize));
		hc <<= (bitsize >> 1);
		hc ^= top;
	}
	return hc;
}
/*}}}*/
/*{{{  void ph_insert (int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key)*/
/*
 *	inserts an item into a pointer-hash
 */
#ifdef TRACE_MEMORY
void ss_ph_insert (const char *file, const int line, int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key)
#else
void ph_insert (int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key)
#endif
{
	unsigned int hcode = ph_hashcode (key, bitsize);
	int bucket = hcode & ((1 << bitsize) - 1);

	if (!bsizes[bucket] || !table[bucket] || !keys[bucket]) {
#ifdef TRACE_MEMORY
		table[bucket] = (void **)ss_malloc (file, line, sizeof (void *));
#else
		table[bucket] = (void **)smalloc (sizeof (void *));
#endif
		table[bucket][0] = item;

#ifdef TRACE_MEMORY
		keys[bucket] = (void **)ss_malloc (file, line, sizeof (void *));
#else
		keys[bucket] = (void **)smalloc (sizeof (void *));
#endif
		keys[bucket][0] = key;

		bsizes[bucket] = 1;
	} else {
#ifdef TRACE_MEMORY
		table[bucket] = (void **)ss_realloc (file, line, (void *)table[bucket],
					bsizes[bucket] * sizeof(void *),
					(bsizes[bucket] + 1) * sizeof(void *));
#else
		table[bucket] = (void **)srealloc ((void *)table[bucket],
					bsizes[bucket] * sizeof(void *),
					(bsizes[bucket] + 1) * sizeof(void *));
#endif
		table[bucket][bsizes[bucket]] = item;
#ifdef TRACE_MEMORY
		keys[bucket] = (void **)ss_realloc (file, line, (void *)keys[bucket],
					bsizes[bucket] * sizeof(void *),
					(bsizes[bucket] + 1) * sizeof(void *));
#else
		keys[bucket] = (void **)srealloc ((void *)keys[bucket],
					bsizes[bucket] * sizeof(void *),
					(bsizes[bucket] + 1) * sizeof(void *));
#endif
		keys[bucket][bsizes[bucket]] = key;

		bsizes[bucket]++;
	}
	return;
}
/*}}}*/
/*{{{  void ph_remove (int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key)*/
/*
 *	removes an item from a pointer-hash
 */
#ifdef TRACE_MEMORY
void ss_ph_remove (const char *file, const int line, int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key)
#else
void ph_remove (int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key)
#endif
{
	unsigned int hcode = ph_hashcode (key, bitsize);
	int bucket = hcode & ((1 << bitsize) - 1);

	if (!bsizes[bucket] || !table[bucket] || !keys[bucket]) {
		return;
	} else {
		int i;

		for (i=0; i<bsizes[bucket]; i++) {
			if ((ph_hashcode (keys[bucket][i], bitsize) == hcode) && (item == table[bucket][i]) && (keys[bucket][i] == key)) {
				/* shuffle up others */
				for (; i<(bsizes[bucket] - 1); i++) {
					table[bucket][i] = table[bucket][i+1];
					keys[bucket][i] = keys[bucket][i+1];
				}

				/* nullify the last entries */
				table[bucket][i] = NULL;
				keys[bucket][i] = NULL;

				/* now we have to make smaller */
				if (bsizes[bucket] == 1) {
#ifdef TRACE_MEMORY
					ss_free (file, line, table[bucket]);
					ss_free (file, line, keys[bucket]);
#else
					sfree (table[bucket]);
					sfree (keys[bucket]);
#endif
					table[bucket] = NULL;
					keys[bucket] = NULL;
				} else {
#ifdef TRACE_MEMORY
					table[bucket] = (void **)ss_realloc (file, line, (void *)table[bucket], bsizes[bucket] * sizeof (void *),
							(bsizes[bucket] - 1) * sizeof (void *));
					keys[bucket] = (void **)ss_realloc (file, line, (void *)keys[bucket], bsizes[bucket] * sizeof (void *),
							(bsizes[bucket] - 1) * sizeof (void *));
#else
					table[bucket] = (void **)srealloc ((void *)table[bucket], bsizes[bucket] * sizeof (void *),
							(bsizes[bucket] - 1) * sizeof (void *));
					keys[bucket] = (void **)srealloc ((void *)keys[bucket], bsizes[bucket] * sizeof (void *),
							(bsizes[bucket] - 1) * sizeof (void *));
#endif
				}
				bsizes[bucket] = bsizes[bucket] - 1;

				return;
			}
		}
	}
	nocc_warning ("ph_remove(): item [0x%8.8x:0x%8.8x] not in pointerhash", (unsigned int)item, (unsigned int)key);
	return;
}
/*}}}*/
/*{{{  void *ph_lookup (int *bsizes, void ***table, void ***keys, int bitsize, void *match)*/
/*
 *	looks up an item in a pointer-hash
 */
void *ph_lookup (int *bsizes, void ***table, void ***keys, int bitsize, void *match)
{
	unsigned int hcode = ph_hashcode (match, bitsize);
	int bucket = hcode & ((1 << bitsize) - 1);

	if (!bsizes[bucket] || !table[bucket] || !keys[bucket]) {
		return NULL;
	} else {
		int i;

		for (i=0; i<bsizes[bucket]; i++) {
			if ((ph_hashcode (keys[bucket][i], bitsize) == hcode) && (keys[bucket][i] == match)) {
#if 0 && defined(DEBUG)
fprintf (stderr, "ph_lookup: match for [%s] found item 0x%8.8x\n", match, (unsigned int)(table[bucket][i]));
#endif
				return table[bucket][i];
			}
		}
	}
	return NULL;
}
/*}}}*/
/*{{{  void ph_dump (FILE *stream, int *bsizes, void ***table, void ***keys, int size)*/
/*
 *	dumps the contents of a pointer-hash (debugging)
 */
void ph_dump (FILE *stream, int *bsizes, void ***table, void ***keys, int size)
{
	int i;

	fprintf (stream, "hash-table size: %d buckets\n", size);
	for (i=0; i < size; i++) {
		fprintf (stream, "bucket %d:\tsize %d:\t", i, bsizes[i]);
		if (bsizes[i]) {
			int j;

			for (j=0; j<bsizes[i]; j++) {
				fprintf (stream, "%s0x%8.8x (0x%8.8x)", (!j ? "" : ", "), (unsigned int)(keys[i][j]), (unsigned int)(table[i][j]));
			}
		}
		fprintf (stream, "\n");
	}
}
/*}}}*/
/*{{{  void ph_walk (int *bsizes, void ***table, void ***keys, int size, void (*func)(void *, void *, void *), void *p)*/
/*
 *	walks the contents of a pointer-hash
 */
void ph_walk (int *bsizes, void ***table, void ***keys, int size, void (*func)(void *, void *, void *), void *p)
{
	int i, j;

	for (i=0; i<size; i++) {
		if (bsizes[i]) {
			for (j=0; j<bsizes[i]; j++) {
				func (table[i][j], keys[i][j], p);
			}
		}
	}
	return;
}
/*}}}*/
/*{{{  void ph_lwalk (int *bsizes, void ***table, void ***keys, int bitsize, void *match, void (*func)(void *, void *, void *), void *p)*/
/*
 *	walks the contents of a pointer-hash for items that match
 */
void ph_lwalk (int *bsizes, void ***table, void ***keys, int bitsize, void *match, void (*func)(void *, void *, void *), void *p)
{
	unsigned int hcode = ph_hashcode (match, bitsize);
	int bucket = hcode & ((1 << bitsize) - 1);

	if (!bsizes[bucket] || !table[bucket] || !keys[bucket]) {
		return;
	} else {
		int i;

		for (i=0; i<bsizes[bucket]; i++) {
			if ((ph_hashcode (keys[bucket][i], bitsize) == hcode) && (!strcmp (keys[bucket][i], match))) {
				func (table[bucket][i], keys[bucket][i], p);
			}
		}
	}
	return;
}
/*}}}*/
/*{{{  void ph_trash (int *bsizes, void ***table, void ***keys, int size)*/
/*
 *	destroys a string-hash
 */
#ifdef TRACE_MEMORY
void ss_ph_trash (const char *file, const int line, int *bsizes, void ***table, void ***keys, int size)
#else
void ph_trash (int *bsizes, void ***table, void ***keys, int size)
#endif
{
	int i;

	for (i=0; i<size; i++) {
		if (table[i]) {
#ifdef TRACE_MEMORY
			ss_free (file, line, table[i]);
#else
			sfree (table[i]);
#endif
		}
		if (keys[i]) {
#ifdef TRACE_MEMORY
			ss_free (file, line, keys[i]);
#else
			sfree (keys[i]);
#endif
		}
	}

	return;
}
/*}}}*/


/*{{{  int decode_hex_byte (char b1, char b2, unsigned char *tptr)*/
/*
 *	turns 2 hex characters into an unsigned byte.  returns 0 on success, -1 on error
 */
int decode_hex_byte (char b1, char b2, unsigned char *tptr)
{
	*tptr = 0;
	if ((b1 >= '0') && (b1 <= '9')) {
		*tptr = ((b1 - '0') << 4);
	} else if ((b1 >= 'a') && (b1 <= 'f')) {
		*tptr = (((b1 - 'a') + 10) << 4);
	} else if ((b1 >= 'A') && (b1 <= 'F')) {
		*tptr = (((b1 - 'A') + 10) << 4);
	} else {
		return -1;
	}
	if ((b2 >= '0') && (b2 <= '9')) {
		*tptr |= ((b2 - '0') & 0x0f);
	} else if ((b2 >= 'a') && (b2 <= 'f')) {
		*tptr |= (((b2 - 'a') + 10) & 0x0f);
	} else if ((b2 >= 'A') && (b2 <= 'F')) {
		*tptr |= (((b2 - 'A') + 10) & 0x0f);
	} else {
		return -1;
	}
	return 0;
}
/*}}}*/
/*{{{  int parse_uint16hex (char *ch)*/
/*
 *	parses 4 hex digits to form an unsigned 16-bit number.  returns in a 32-bit word
 */
int parse_uint16hex (char *ch)
{
	int w = 0;
	unsigned char v;

	if (decode_hex_byte (ch[0], ch[1], &v)) {
		return 0;
	}
	w |= v;
	w <<= 8;
	if (decode_hex_byte (ch[2], ch[3], &v)) {
		return 0;
	}
	w |= v;
	return w;
}
/*}}}*/
/*{{{  char *mkhexbuf (unsigned char *buffer, int buflen)*/
/*
 *	turns a byte buffer into a nice hex string
 */
#ifdef TRACE_MEMORY
char *ss_mkhexbuf (const char *file, const int line, unsigned char *buffer, int buflen)
#else
char *mkhexbuf (unsigned char *buffer, int buflen)
#endif
{
	static char *hexstr = "0123456789abcdef";
#ifdef TRACE_MEMORY
	char *str = (char *)ss_malloc (file, line, (buflen << 1) + 1);
#else
	char *str = (char *)smalloc ((buflen << 1) + 1);
#endif
	int i;

	for (i=0; i<buflen; i++) {
		str[i << 1] = hexstr [(int)(buffer[i] >> 4)];
		str[(i << 1) + 1] = hexstr [(int)(buffer[i] & 0x0f)];
	}
	str[i << 1] = '\0';
	return str;
}
/*}}}*/
/*{{{  char **split_string (char *str, int copy)*/
/*
 *	splits a string up, returns an array of pointers
 *	if "copy" is non-zero, original string is unaffected -- returned bits are all copies
 *	otherwise original string is munged and returned bits point into it.
 */
#ifdef TRACE_MEMORY
char **ss_split_string (const char *file, const int line, char *str, int copy)
#else
char **split_string (char *str, int copy)
#endif
{
	char **bits;
	int nbits;
	char *ch, *start;

	/* skip any leading whitespace */
	for (ch=str; (*ch == ' ') || (*ch == '\t'); ch++);
	for (nbits=0; *ch != '\0';) {
		for (; (*ch != '\0') && (*ch != ' ') && (*ch != '\t'); ch++);		/* skip non-whitespace */
		nbits++;
		for (; (*ch == ' ') || (*ch == '\t'); ch++);				/* skip whitespace */
	}
#if 0
fprintf (stderr, "split_string: splitting [%s] into %d bits\n", str, nbits);
#endif
#ifdef TRACE_MEMORY
	bits = (char **)ss_malloc (file, line, (nbits + 1) * sizeof (char *));
#else
	bits = (char **)smalloc ((nbits + 1) * sizeof (char *));
#endif
	bits[nbits] = NULL;

	/* skip any leading whitespace */
	for (ch=str; (*ch == ' ') || (*ch == '\t'); ch++);
	for (nbits=0; *ch != '\0';) {
		start = ch;
		for (; (*ch != '\0') && (*ch != ' ') && (*ch != '\t'); ch++);
		if (copy) {
#ifdef TRACE_MEMORY
			bits[nbits] = ss_string_ndup (file, line, start, (int)(ch - start));
#else
			bits[nbits] = string_ndup (start, (int)(ch - start));
#endif
			if (*ch != '\0') {
				ch++;
			}
		} else {
			bits[nbits] = start;
			if (*ch != '\0') {
				*ch = '\0';
				ch++;
			}
		}
		nbits++;
		for (; (*ch == ' ') || (*ch == '\t'); ch++);
	}

	return bits;
}
/*}}}*/
/*{{{  char **split_string2 (char *str, char s1, char s2)*/
/*
 *	splits a string up, returns an array of pointers into the original string
 *	the "s1" and "s2" define seperators (use the same if only one)
 */
#ifdef TRACE_MEMORY
char **ss_split_string2 (const char *file, const int line, char *str, char s1, char s2)
#else
char **split_string2 (char *str, char s1, char s2)
#endif
{
	char **bits;
	int nbits;
	char *ch, *start;
	int instring = 0;

	/* skip any leading whitespace */
	for (ch=str; (*ch == s1) || (*ch == s2); ch++);
	for (nbits=0; *ch != '\0';) {
		for (; (*ch != '\0') && (instring || ((*ch != s1) && (*ch != s2))); ch++) {
			if ((*ch == '\"') && (!instring || (ch[-1] != '\\'))) {
				instring = !instring;
			}
		}
		nbits++;
		for (; (*ch == s1) || (*ch == s2); ch++);
	}
#ifdef TRACE_MEMORY
	bits = (char **)ss_malloc (file, line, (nbits + 1) * sizeof (char *));
#else
	bits = (char **)smalloc ((nbits + 1) * sizeof (char *));
#endif
	bits[nbits] = NULL;

	/* skip any leading whitespace */
	instring = 0;
	for (ch=str; (*ch == s1) || (*ch == s2); ch++);
	for (nbits=0; *ch != '\0';) {
		start = ch;
		for (; (*ch != '\0') && (instring || ((*ch != s1) && (*ch != s2))); ch++) {
			if ((*ch == '\"') && (!instring || (ch[-1] != '\\'))) {
				instring = !instring;
			}
		}
#ifdef TRACE_MEMORY
		bits[nbits] = ss_string_ndup (file, line, start, (int)(ch - start));
#else
		bits[nbits] = string_ndup (start, (int)(ch - start));
#endif
		if (*ch != '\0') {
			/* *ch = '\0'; */
			ch++;
		}
		nbits++;
		for (; (*ch == s1) || (*ch == s2); ch++);
	}

	return bits;
}
/*}}}*/
/*{{{  int string_dequote (char *str)*/
/*
 *	removes quotes from a string and puts right escaped characters (escaped with backslash)
 *	modifies the string passed (only ever gets shorter);  if no quotes, will not de-escape characters
 *	returns 0 on success, non-zero on failure
 */
int string_dequote (char *str)
{
	int slen;
	char *ch, *dh;

	if (!str) {
		return -1;
	}
	if (*str != '\"') {
		/* unquoted string -- leave it alone */
		return 0;
	}
	slen = strlen (str);
	if ((slen == 1) || (str[slen-1] != '\"')) {
		/* mangled quotes -- leave it alone */
		return 0;
	}

#if 0
fprintf (stderr, "string_dequote(): on [%s]\n", str);
#endif
	/* copy back and handle escapes */
	for (dh=str, ch=str+1, slen -= 2; slen && (*ch != '\0'); ch++, dh++, slen--) {
		if (*ch == '\\') {
			ch++;
			slen--;
			switch (*ch) {
			default:	/* (assume) simple escaped character */
				*dh = *ch;
				break;
			case 'n':	/* newline character */
				*dh = '\n';
				break;
			case 'r':	/* CR */
				*dh = '\r';
				break;
			case 't':	/* tab */
				*dh = '\t';
				break;
			}
		} else {
			*dh = *ch;
		}
	}
	*dh = '\0';
#if 0
fprintf (stderr, "string_dequote(): output --> [%s]\n", str);
#endif

	return 0;
}
/*}}}*/
/*{{{  char *decode_hexstr (char *str, int *slen)*/
/*
 *	this turns a string of HEX values into a regular string (undoes "mkhexbuf")
 *	returns NULL on error.  stores the resulting string length in `*slen'
 */
#ifdef TRACE_MEMORY
char *ss_decode_hexstr (const char *file, const int line, char *str, int *slen)
#else
char *decode_hexstr (char *str, int *slen)
#endif
{
	int len, i;
	char *newstr;

	*slen = 0;
	if (!str) {
		return NULL;
	}
	len = strlen (str);
	if (len & 0x01) {
		return NULL;
	}
	*slen = (len >> 1);
#ifdef TRACE_MEMORY
	newstr = (char *)ss_malloc (file, line, *slen + 1);
#else
	newstr = (char *)smalloc (*slen + 1);
#endif
	newstr[*slen] = '\0';
	for (i=0; i<*slen; i++) {
		if (decode_hex_byte (str[(i << 1)], str[(i << 1) + 1], (unsigned char *)newstr + i) < 0) {
#ifdef TRACE_MEMORY
			ss_free (file, line, newstr);
#else
			sfree (newstr);
#endif
			*slen = 0;
			return NULL;
		}
	}
	return newstr;
}
/*}}}*/
#if 0
/*{{{  int time_after (struct timeval *t1, struct timeval *t2)*/
/*
 *	determines whether t1 is after t2
 */
int time_after (struct timeval *t1, struct timeval *t2)
{
	return ((t1->tv_sec > t2->tv_sec) || ((t1->tv_sec == t2->tv_sec) && (t1->tv_usec > t2->tv_usec)));
}
/*}}}*/
/*{{{  void time_minus (struct timeval *t1, struct timeval *t2, struct timeval *t3)*/
/*
 *	subtracts t2 from t1 and places the resulting time in t3
 */
void time_minus (struct timeval *t1, struct timeval *t2, struct timeval *t3)
{
	t3->tv_sec = t1->tv_sec - t2->tv_sec;
	t3->tv_usec = t1->tv_usec - t2->tv_usec;
	while (t3->tv_usec < 0) {
		t3->tv_usec += 1000000;
		t3->tv_sec--;
	}
	return;
}
/*}}}*/
/*{{{  void time_setin (struct timeval *t1, struct timeval *t2)*/
/*
 *	adds t2 to the current time and places the result in t1
 */
void time_setin (struct timeval *t1, struct timeval *t2)
{
	struct timeval now;

	gettimeofday (&now, NULL);
	t1->tv_sec = now.tv_sec + t2->tv_sec;
	t1->tv_usec = now.tv_usec + t2->tv_usec;
	while (t1->tv_usec > 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
	return;
}
/*}}}*/
#endif
/*{{{  char *strstrip (char *str)*/
/*
 *	removes leading and trailing whitespace from the string.  returns the same pointer (moves contents)
 */
char *strstrip (char *str)
{
	char *ch;

	if (!str) {
		return NULL;
	}
	for (ch = str; (*ch == ' ') || (*ch == '\t'); ch++);
	if (ch != str) {
		char *dh, *eh;

		for (dh = str, eh = ch; (*eh != '\0'); dh++, eh++) {
			*dh = *eh;
		}
		*dh = *eh;
	}
	for (ch = str + (strlen (str) - 1); (*ch == ' ') || (*ch == '\t'); ch--);
	ch++;
	*ch = '\0';
	return str;
}
/*}}}*/


