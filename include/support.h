/*
 *	support.h - support routine interface
 *	Copyright (C) 2000-2016 Fred Barnes <frmb@kent.ac.uk>
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


#ifndef __SUPPORT_H
#define __SUPPORT_H

static inline int bsr (int v)
{
#ifdef TARGET_CPU_IS_I486
	int res;

	__asm__ __volatile__ ("		\n" \
		"	bsr	%1,%0	\n" \
		: "=r" (res) : "r" (v) : "cc");
	return res;
#else
	unsigned int res;
	int i;

	for (i = 31, res = (1 << 31); i; i--, res >>= 1) {
		if (v & res) {
			return i;
		}
	}
	return 0;
#endif
}


extern void dmem_init (void);
extern void dmem_shutdown (void);

extern int dmem_slot_to_size (int idx);
extern int dmem_size_to_slot (int bytes);
extern int dmem_size_of_addr (void *ptr);
extern void *dmem_new (int idx);
extern void *dmem_alloc (int size);
extern void dmem_release (void *ptr);
extern void dmem_usagedump (void);



#ifdef TRACE_MEMORY
	#define smalloc(X) ss_malloc(__FILE__,__LINE__,X)
	#define srealloc(X,A,B) ss_realloc(__FILE__,__LINE__,X,A,B)
	#define sfree(X) ss_free(__FILE__,__LINE__,X)

	#define string_ndup(X,A) ss_string_ndup(__FILE__,__LINE__,X,A)
	#define string_dup(X) ss_string_dup(__FILE__,__LINE__,X)
	#define string_xmlfixup(X,A) ss_string_xmlfixup(__FILE__,__LINE__,X,A)
	#define mem_ndup(X,A) ss_mem_ndup(__FILE__,__LINE__,X,A)

	extern void *ss_malloc (const char *, const int, size_t);
	extern void *ss_realloc (const char *, const int, void *, size_t, size_t);
	extern void ss_free (const char *, const int, void *);
	extern void ss_cleanup (void);

	extern char *ss_string_ndup (const char *, const int, const char *, int);
	extern char *ss_string_dup (const char *, const int, const char *);
	extern char *ss_string_xmlfixup (const char *, const int, const char *, int);
	extern void *ss_mem_ndup (const char *, const int, const void *, int);
#else
	extern void *smalloc (size_t);
	extern void *srealloc (void *, size_t, size_t);
	extern void sfree (void *);

	extern char *string_ndup (const char *, int);
	extern char *string_dup (const char *);
	extern char *string_xmlfixup (const char *, int);
	extern void *mem_ndup (const void *, int);
#endif

extern char *string_fmt (const char *, ...) __attribute__ ((format (printf, 1, 2)));
extern char *string_upper (const char *);
extern char *string_lower (const char *);


/* fresh dynamic array stuff: HACK */
#define DA_CUR(NAME) NAME ## _cur
#define DA_MAX(NAME) NAME ## _max
#define DA_PTR(NAME) NAME
#define DA_NTHITEM(NAME,N) (NAME)[(N)]
#define DA_NTHITEMADDR(NAME,N) (&((NAME)[(N)]))
#define DA_SETNTHITEM(NAME,N,ITEM) (NAME)[(N)] = (ITEM)
#define DYNARRAY(TYPE,NAME) int DA_CUR(NAME), DA_MAX(NAME); TYPE* DA_PTR(NAME)
#define DYNSARRAY(TYPE,NAME) int DA_CUR(NAME), DA_MAX(NAME); struct TYPE* DA_PTR(NAME)
#define STATICDYNARRAY(TYPE,NAME) static int DA_CUR(NAME), DA_MAX(NAME); static TYPE* DA_PTR(NAME)
#define DA_CONSTINITIALISER(NAME) .DA_PTR(NAME)= NULL, .DA_CUR(NAME)= 0, .DA_MAX(NAME)= 0

#ifdef TRACE_MEMORY
	#define da_additem(X,A,B,C) ss_da_additem(__FILE__,__LINE__,X,A,B,C)
	#define da_insertitem(X,A,B,C,D) ss_da_insertitem(__FILE__,__LINE__,X,A,B,C,D)
	#define da_maybeadditem(X,A,B,C) ss_da_maybeadditem(__FILE__,__LINE__,X,A,B,C)
	#define da_trash(X,A,B) ss_da_trash(__FILE__,__LINE__,X,A,B)
	#define da_setsize(X,A,B,C) ss_da_setsize(__FILE__,__LINE__,X,A,B,C)
	#define da_setmax(X,A,B,C) ss_da_setmax(__FILE__,__LINE__,X,A,B,C)
	#define da_copy(X,A,B,C,D,E) ss_da_copy(__FILE__,__LINE__,X,A,B,C,D,E)

	extern void ss_da_additem (const char *file, const int line, int *cur, int *max, void ***array, void *item);
	extern void ss_da_insertitem (const char *file, const int line, int *cur, int *max, void ***array, void *item, int idx);
	extern int ss_da_maybeadditem (const char *file, const int line, int *cur, int *max, void ***array, void *item);
	extern void ss_da_trash (const char *file, const int line, int *cur, int *max, void ***array);
	extern void ss_da_setsize (const char *file, const int line, int *cur, int *max, void ***array, int size);
	extern void ss_da_setmax (const char *file, const int line, int *cur, int *max, void ***array, int size);
	extern void ss_da_copy (const char *file, const int line, int srccur, int srcmax, void **srcarray, int *dstcur, int *dstmax, void ***dstarray);
#else
	extern void da_additem (int *cur, int *max, void ***array, void *item);
	extern void da_insertitem (int *cur, int *max, void ***array, void *item, int idx);
	extern int da_maybeadditem (int *cur, int *max, void ***array, void *item);
	extern void da_trash (int *cur, int *max, void ***array);
	extern void da_setsize (int *cur, int *max, void ***array, int size);
	extern void da_setmax (int *cur, int *max, void ***array, int size);
	extern void da_copy (int srccur, int srcmax, void **srcarray, int *dstcur, int *dstmax, void ***dstarray);
#endif
extern void da_init (int *cur, int *max, void ***array);
extern void da_delitem (int *cur, int *max, void ***array, int idx);
extern void da_rmitem (int *cur, int *max, void ***array, void *item);
extern void da_qsort (void **array, int first, int last, int (*)(void *, void *));
extern int da_hasitem (int *cur, int *max, void ***array, void *item);

#define dynarray_init(ARRAY) da_init(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)))
#define dynarray_add(ARRAY,ITEM) da_additem(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)), (void *)(ITEM))
#define dynarray_insert(ARRAY,ITEM,IDX) da_insertitem(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)), (void *)(ITEM), (int)(IDX))
#define dynarray_maybeadd(ARRAY,ITEM) da_maybeadditem(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)), (void *)(ITEM))
#define dynarray_delitem(ARRAY,IDX) da_delitem(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)), IDX)
#define dynarray_rmitem(ARRAY,ITEM) da_rmitem(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)), (void *)(ITEM))
#define dynarray_trash(ARRAY) da_trash(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)))
#define dynarray_qsort(ARRAY,FCN) da_qsort((void **)(DA_PTR(ARRAY)), 0, DA_CUR(ARRAY) - 1, (int (*)(void *, void *))(FCN))
#define dynarray_setsize(ARRAY,SIZE) da_setsize(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)), SIZE)
#define dynarray_setmax(ARRAY,SIZE) da_setmax(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)), SIZE)
#define dynarray_copy(DSTARRAY,SRCARRAY) da_copy(DA_CUR(SRCARRAY), DA_MAX(SRCARRAY), (void **)(DA_PTR(SRCARRAY)), &(DA_CUR(DSTARRAY)), &(DA_MAX(DSTARRAY)), (void ***)(&(DA_PTR(DSTARRAY))))
#define dynarray_hasitem(ARRAY,ITEM) da_hasitem(&(DA_CUR(ARRAY)), &(DA_MAX(ARRAY)), (void ***)&(DA_PTR(ARRAY)), (void *)(ITEM))
#define dynarray_move(DSTARRAY,SRCARRAY) do { dynarray_trash(DSTARRAY); DA_PTR(DSTARRAY) = DA_PTR(SRCARRAY); \
						DA_CUR(DSTARRAY) = DA_CUR(SRCARRAY); DA_MAX(DSTARRAY) = DA_MAX(SRCARRAY); dynarray_init (SRCARRAY); } while (0)

/* stuff for string-based hashes */

#define SH_TABLE(NAME) NAME
#define SH_KEYS(NAME) NAME ## _keys
#define SH_BSIZES(NAME) NAME ## _bsizes
#define SH_SIZE(NAME) NAME ## _size
#define SH_BITSIZE(NAME) NAME ## _bitsize
#define SH_LOOKUP(NAME) NAME ## _lookup
#define STATICSTRINGHASH(TYPE,NAME,BITSIZE) static TYPE* SH_TABLE(NAME)[1 << (BITSIZE)]; \
		static char** SH_KEYS(NAME)[1 << (BITSIZE)]; \
		static int SH_BSIZES(NAME)[1 << (BITSIZE)]; \
		static const int SH_SIZE(NAME) = (1 << (BITSIZE)); \
		static const int SH_BITSIZE(NAME) = (BITSIZE); \
		static TYPE (*SH_LOOKUP(NAME))(int *, void ***, char ***, int, char *) = (TYPE(*)(int *, void ***, char ***, int, char *))sh_lookup
#define STRINGHASH(TYPE,NAME,BITSIZE) TYPE* SH_TABLE(NAME)[1 << (BITSIZE)]; \
		char** SH_KEYS(NAME)[1 << (BITSIZE)]; \
		int SH_BSIZES(NAME)[1 << (BITSIZE)]; \
		int SH_SIZE(NAME); \
		int SH_BITSIZE(NAME); \
		TYPE (*SH_LOOKUP(NAME))(int *, void ***, char ***, int, char *)


#ifdef TRACE_MEMORY
	#define sh_insert(X,A,B,C,D,E) ss_sh_insert(__FILE__,__LINE__,X,A,B,C,D,E)
	#define sh_remove(X,A,B,C,D,E) ss_sh_remove(__FILE__,__LINE__,X,A,B,C,D,E)
	#define sh_trash(X,A,B,C) ss_sh_trash(__FILE__,__LINE__,X,A,B,C)

	extern void ss_sh_insert (const char *file, const int line, int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key);
	extern void ss_sh_remove (const char *file, const int line, int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key);
	extern void ss_sh_trash (const char *file, const int line, int *bsizes, void ***table, char ***keys, int size);
#else
	extern void sh_insert (int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key);
	extern void sh_remove (int *bsizes, void ***table, char ***keys, int bitsize, void *item, char *key);
	extern void sh_trash (int *bsizes, void ***table, char ***keys, int size);
#endif

extern void sh_init (int *bsizes, void ***table, char ***keys, int *szptr, int *bszptr, void **fnptr, int bitsize);
extern void sh_sinit (int *bsizes, void ***table, char ***keys, int size);
extern void *sh_lookup (int *bsizes, void ***table, char ***keys, int bitsize, char *match);
extern void sh_dump (FILE *stream, int *bsizes, void ***table, char ***keys, int size);
extern void sh_walk (int *bsizes, void ***table, char ***keys, int size, void (*func)(void *, char *, void *), void *p);
extern unsigned int sh_stringhash (const char *str, const int len);

#define stringhash_init(SHASH,BITSIZE) sh_init((int *)&((SH_BSIZES(SHASH))[0]), (void ***)&((SH_TABLE(SHASH))[0]), (char ***)&((SH_KEYS(SHASH))[0]), &SH_SIZE(SHASH), &SH_BITSIZE(SHASH), (void *)&(SH_LOOKUP(SHASH)), (BITSIZE))
#define stringhash_sinit(SHASH) sh_sinit((int *)&((SH_BSIZES(SHASH))[0]), (void ***)&((SH_TABLE(SHASH))[0]), (char ***)&((SH_KEYS(SHASH))[0]), SH_SIZE(SHASH))
#define stringhash_insert(SHASH,ITEM,KEY) sh_insert((int *)&((SH_BSIZES(SHASH))[0]), (void ***)&((SH_TABLE(SHASH))[0]), (char ***)&((SH_KEYS(SHASH))[0]), SH_BITSIZE(SHASH), (void *)(ITEM), (char *)(KEY))
#define stringhash_remove(SHASH,ITEM,KEY) sh_remove((int *)&((SH_BSIZES(SHASH))[0]), (void ***)&((SH_TABLE(SHASH))[0]), (char ***)&((SH_KEYS(SHASH))[0]), SH_BITSIZE(SHASH), (void *)(ITEM), (char *)(KEY))
#define stringhash_lookup(SHASH,ITEM) SH_LOOKUP(SHASH) ((int *)&((SH_BSIZES(SHASH))[0]), (void ***)&((SH_TABLE(SHASH))[0]), (char ***)&((SH_KEYS(SHASH))[0]), SH_BITSIZE(SHASH), (char *)(ITEM))
#define stringhash_dump(STREAM,SHASH) sh_dump((STREAM),(int *)&((SH_BSIZES(SHASH))[0]), (void ***)&((SH_TABLE(SHASH))[0]), (char ***)&((SH_KEYS(SHASH))[0]), SH_SIZE(SHASH))
#define stringhash_walk(SHASH,FUNC,P) sh_walk((int *)&((SH_BSIZES(SHASH))[0]), (void ***)&((SH_TABLE(SHASH))[0]), (char ***)&((SH_KEYS(SHASH))[0]), SH_SIZE(SHASH), (void (*)(void *, char *, void *))(FUNC), (P))
#define stringhash_trash(SHASH) sh_trash((int *)&((SH_BSIZES(SHASH))[0]), (void ***)&((SH_TABLE(SHASH))[0]), (char ***)&((SH_KEYS(SHASH))[0]), SH_SIZE(SHASH))

/* stuff for pointer-based hashes (keys are just the name) */

#define PH_TABLE(NAME) NAME
#define PH_KEYS(NAME) NAME ## _keys
#define PH_BSIZES(NAME) NAME ## _bsizes
#define PH_SIZE(NAME) NAME ## _size
#define PH_BITSIZE(NAME) NAME ## _bitsize
#define PH_LOOKUP(NAME) NAME ## _lookup
#define STATICPOINTERHASH(TYPE,NAME,BITSIZE) static TYPE* PH_TABLE(NAME)[1 << (BITSIZE)]; \
		static void **PH_KEYS(NAME)[1 << (BITSIZE)]; \
		static int PH_BSIZES(NAME)[1 << (BITSIZE)]; \
		static const int PH_SIZE(NAME) = (1 << (BITSIZE)); \
		static const int PH_BITSIZE(NAME) = (BITSIZE); \
		static TYPE (*PH_LOOKUP(NAME))(int *, void ***, void ***, int, void *) = (TYPE(*)(int *, void ***, void ***, int, void *))ph_lookup
#define POINTERHASH(TYPE,NAME,BITSIZE) TYPE* PH_TABLE(NAME)[1 << (BITSIZE)]; \
		void **PH_KEYS(NAME)[1 << (BITSIZE)]; \
		int PH_BSIZES(NAME)[1 << (BITSIZE)]; \
		int PH_SIZE(NAME); \
		int PH_BITSIZE(NAME); \
		TYPE (*PH_LOOKUP(NAME))(int *, void ***, void ***, int, void *)

#ifdef TRACE_MEMORY
	#define ph_insert(X,A,B,C,D,E) ss_ph_insert(__FILE__,__LINE__,X,A,B,C,D,E)
	#define ph_remove(X,A,B,C,D,E) ss_ph_remove(__FILE__,__LINE__,X,A,B,C,D,E)
	#define ph_trash(X,A,B,C) ss_ph_trash(__FILE__,__LINE__,X,A,B,C)

	extern void ss_ph_insert (const char *file, const int line, int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key);
	extern void ss_ph_remove (const char *file, const int line, int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key);
	extern void ss_ph_trash (const char *file, const int line, int *bsizes, void ***table, void ***keys, int size);
#else
	extern void ph_insert (int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key);
	extern void ph_remove (int *bsizes, void ***table, void ***keys, int bitsize, void *item, void *key);
	extern void ph_trash (int *bsizes, void ***table, void ***keys, int size);
#endif
extern void ph_init (int *bsizes, void ***table, void ***keys, int *szptr, int *bszptr, void **fnptr, int bitsize);
extern void ph_sinit (int *bsizes, void ***table, void ***keys, int size);
extern void *ph_lookup (int *bsizes, void ***table, void ***keys, int bitsize, void *match);
extern void ph_dump (FILE *stream, int *bsizes, void ***table, void ***keys, int size);
extern void ph_walk (int *bsizes, void ***table, void ***keys, int size, void (*func)(void *, void *, void *), void *p);
extern void ph_lwalk (int *bsizes, void ***table, void ***keys, int bitsize, void *match, void (*func)(void *, void *, void *), void *p);

#define pointerhash_init(PHASH,BITSIZE) ph_init((int *)&((PH_BSIZES(PHASH))[0]), (void ***)&((PH_TABLE(PHASH))[0]), (void ***)&((PH_KEYS(PHASH))[0]), &PH_SIZE(PHASH), &PH_BITSIZE(PHASH), (void *)&(PH_LOOKUP(PHASH)), (BITSIZE))
#define pointerhash_sinit(PHASH) ph_sinit((int *)&((PH_BSIZES(PHASH))[0]), (void ***)&((PH_TABLE(PHASH))[0]), (void ***)&((PH_KEYS(PHASH))[0]), PH_SIZE(PHASH))
#define pointerhash_insert(PHASH,ITEM,KEY) ph_insert((int *)&((PH_BSIZES(PHASH))[0]), (void ***)&((PH_TABLE(PHASH))[0]), (void ***)&((PH_KEYS(PHASH))[0]), PH_BITSIZE(PHASH), (void *)(ITEM), (void *)(KEY))
#define pointerhash_remove(PHASH,ITEM,KEY) ph_remove((int *)&((PH_BSIZES(PHASH))[0]), (void ***)&((PH_TABLE(PHASH))[0]), (void ***)&((PH_KEYS(PHASH))[0]), PH_BITSIZE(PHASH), (void *)(ITEM), (void *)(KEY))
#define pointerhash_lookup(PHASH,KEY) PH_LOOKUP(PHASH) ((int *)&((PH_BSIZES(PHASH))[0]), (void ***)&((PH_TABLE(PHASH))[0]), (void ***)&((PH_KEYS(PHASH))[0]), PH_BITSIZE(PHASH), (void *)(KEY))
#define pointerhash_dump(STREAM,PHASH) ph_dump((STREAM), (int *)&((PH_BSIZES(PHASH))[0]), (void ***)&((PH_TABLE(PHASH))[0]), (void ***)&((PH_KEYS(PHASH))[0]), PH_SIZE(PHASH))
#define pointerhash_walk(PHASH,FUNC,P) ph_walk((int *)&((PH_BSIZES(PHASH))[0]), (void ***)&((PH_TABLE(PHASH))[0]), (void ***)&((PH_KEYS(PHASH))[0]), PH_SIZE(PHASH), (void (*)(void *, void *, void *))(FUNC), (P))
#define pointerhash_lwalk(PHASH,ITEM,FUNC,P) ph_walk((int *)&((PH_BSIZES(PHASH))[0]), (void ***)&((PH_TABLE(PHASH))[0]), (void ***)&((PH_KEYS(PHASH))[0]), PH_BITSIZE(PHASH), (void *)(ITEM), (void (*)(void *, void *, void *))(FUNC), (P))
#define pointerhash_trash(PHASH) ph_trash((int *)&((PH_BSIZES(PHASH))[0]), (void ***)&((PH_TABLE(PHASH))[0]), (void ***)&((PH_KEYS(PHASH))[0]), PH_SIZE(PHASH))


/* other useful things */

extern int decode_hex_byte (char b1, char b2, unsigned char *tptr);
extern int parse_uint16hex (char *ch);
#ifdef TRACE_MEMORY
	#define mkhexbuf(X,A) ss_mkhexbuf(__FILE__,__LINE__,(X),(A))
	#define split_string(X,A) ss_split_string(__FILE__,__LINE__,(X),(A))
	#define split_string2(X,A,B) ss_split_string2(__FILE__,__LINE__,(X),(A),(B))
	#define decode_hexstr(X,A) ss_decode_hexstr(__FILE__,__LINE__,(X),(A))

	extern char *ss_mkhexbuf (const char *file, const int line, unsigned char *buffer, int buflen);
	extern char **ss_split_string (const char *file, const int line, char *str, int copy);
	extern char **ss_split_string2 (const char *file, const int line, char *str, char s1, char s2);
	extern char *ss_decode_hexstr (const char *file, const int line, char *str, int *slen);
#else
	extern char *mkhexbuf (unsigned char *buffer, int buflen);
	extern char **split_string (char *str, int copy);
	extern char **split_string2 (char *str, char s1, char s2);
	extern char *decode_hexstr (char *str, int *slen);
#endif
extern void string_freebits (char **bits);
extern int string_dequote (char *str);

#if 0
extern int time_after (struct timeval *t1, struct timeval *t2);
extern void time_minus (struct timeval *t1, struct timeval *t2, struct timeval *t3);
extern void time_setin (struct timeval *t1, struct timeval *t2);
#endif
extern char *strstrip (char *str);

#endif	/* !__SUPPORT_H */

