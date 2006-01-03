/*
 *	crypto.c -- cryptographic/security stuff for nocc
 *	Copyright (C) 2006 Fred Barnes <frmb@kent.ac.uk>
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

/*{{{  includes*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>

#if defined(USE_LIBGCRYPT)
#include <gcrypt.h>
#endif

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "crypto.h"


/*}}}*/
/*{{{  private types/data*/
#if defined(USE_LIBGCRYPT)

typedef struct TAG_crypto_gcrypt {
	gcry_md_hd_t handle;
	gcry_error_t err;
	int dlen;
} crypto_gcrypt_t;

static int gcrypt_digestalgo = 0;

#endif

/*}}}*/


#if defined(USE_LIBGCRYPT)

/*{{{  static int icrypto_newdigest (crypto_t *cry)*/
/*
 *	creates a new digest handle
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_newdigest (crypto_t *cry)
{
	crypto_gcrypt_t *gc = (crypto_gcrypt_t *)smalloc (sizeof (crypto_gcrypt_t));

	gc->err = gcry_md_open (&gc->handle, gcrypt_digestalgo, 0);
	if (gc->err) {
		nocc_error ("icrypto_newdigest(): failed to create digest: %s", gpg_strerror (gc->err));
		sfree (gc);
		return -1;
	}
	gc->dlen = gcry_md_get_algo_dlen (gcrypt_digestalgo);
	if (gc->dlen <= 0) {
		gcry_md_close (gc->handle);
		nocc_error ("icrypto_newdigest(): bad digest length");
		sfree (gc);
		return -1;
	}

	cry->priv = (void *)gc;

	return 0;
}
/*}}}*/
/*{{{  static void icrypto_freedigest (crypto_t *cry)*/
/*
 *	frees a digest
 */
static void icrypto_freedigest (crypto_t *cry)
{
	crypto_gcrypt_t *gc = (crypto_gcrypt_t *)cry->priv;

	if (!gc) {
		return;
	}

	gcry_md_close (gc->handle);
	sfree (gc);
	cry->priv = NULL;

	return;
}
/*}}}*/
/*{{{  static int icrypto_writedigest (crypto_t *cry, unsigned char *data, int bytes)*/
/*
 *	writes bytes into the digest
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_writedigest (crypto_t *cry, unsigned char *data, int bytes)
{
	crypto_gcrypt_t *gc = (crypto_gcrypt_t *)cry->priv;

	if (!gc) {
		return -1;
	}

	gcry_md_write (gc->handle, data, bytes);

	return 0;
}
/*}}}*/
/*{{{  static char *icrypto_readdigest (crypto_t *cry)*/
/*
 *	reads the generated digest so far
 *	returns a fresh formatted string on success, NULL on failure
 */
static char *icrypto_readdigest (crypto_t *cry)
{
	crypto_gcrypt_t *gc = (crypto_gcrypt_t *)cry->priv;
	unsigned char *digest;

	digest = gcry_md_read (gc->handle, gcrypt_digestalgo);
	if (!digest) {
		nocc_error ("icrypto_readdigest(): gcry_md_read() returned NULL!");
		return NULL;
	}

	return mkhexbuf (digest, gc->dlen);
}
/*}}}*/
/*{{{  static int icrypto_init (void)*/
/*
 *	initialises the gcrypt library
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_init (void)
{
	if (!gcry_check_version (GCRYPT_VERSION)) {
		nocc_error ("icrypto_init(): gcrypt version mismatch");
		return -1;
	}
	if (!compopts.hashalgo) {
		compopts.hashalgo = string_dup ("SHA1");
	}
	gcrypt_digestalgo = gcry_md_map_name (compopts.hashalgo);
	if (!gcrypt_digestalgo) {
		nocc_error ("icrypto_init(): do not understand hashing algorithm [%s]", compopts.hashalgo);
		return -1;
	}
	if (compopts.verbose) {
		nocc_message ("using message digest algorithm [%s] for hashing", compopts.hashalgo);
	}
	return 0;
}
/*}}}*/

#else	/* !defined(USE_LIBGCRYPT) */

/*{{{  static int icrypto_newdigest (crypto_t *cry)*/
/*
 *	dummy digest creation
 */
static int icrypto_newdigest (crypto_t *cry)
{
	nocc_error ("icrypto_newdigest(): hashing is not supported by this build");
	return -1;
}
/*}}}*/
/*{{{  static void icrypto_freedigest (crypto_t *cry)*/
/*
 *	dummy free digest
 */
static void icrypto_freedigest (crypto_t *cry)
{
	return;
}
/*}}}*/
/*{{{  static int icrypto_writedigest (crypto_t *cry, unsigned char *data, int bytes)*/
/*
 *	dummy write digest
 */
static int icrypto_writedigest (crypto_t *cry, unsigned char *data, int bytes)
{
	return -1;
}
/*}}}*/
/*{{{  static char *icrypto_readdigest (crypto_t *cry)*/
/*
 *	dummy read digest
 */
static char *icrypto_readdigest (crypto_t *cry)
{
	return NULL;
}
/*}}}*/
/*{{{  static int icrypto_init (void)*/
/*
 *	dummy initialisation function
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_init (void)
{
	return 0;
}
/*}}}*/

#endif	/* !defined(USE_LIBGCRYPT) */


/*{{{  crypto_t *crypto_newdigest (void)*/
/*
 *	creates a new crypto digest
 */
crypto_t *crypto_newdigest (void)
{
	crypto_t *cry = (crypto_t *)smalloc (sizeof (crypto_t));

	if (icrypto_newdigest (cry)) {
		sfree (cry);
		return NULL;
	}
	return cry;
}
/*}}}*/
/*{{{  void crypto_freedigest (crypto_t *cry)*/
/*
 *	frees a crypto digest
 */
void crypto_freedigest (crypto_t *cry)
{
	if (cry) {
		icrypto_freedigest (cry);
		sfree (cry);
	}
	return;
}
/*}}}*/
/*{{{  int crypto_writedigest (crypto_t *cry, unsigned char *data, int bytes)*/
/*
 *	writes bytes to a crypto digest
 *	returns 0 on success, non-zero on failure
 */
int crypto_writedigest (crypto_t *cry, unsigned char *data, int bytes)
{
	if (cry) {
		return icrypto_writedigest (cry, data, bytes);
	}
	return -1;
}
/*}}}*/
/*{{{  char *crypto_readdigest (crypto_t *cry)*/
/*
 *	reads a crypto digest
 *	return the digest (as a new formatted hex-string) on success, or NULL on failure
 */
char *crypto_readdigest (crypto_t *cry)
{
	if (cry) {
		return icrypto_readdigest (cry);
	}
	return NULL;
}
/*}}}*/


/*{{{  int crypto_init (void)*/
/*
 *	initialises the crypto bits
 *	returns 0 on success, non-zero on failure
 */
int crypto_init (void)
{
	return icrypto_init ();
}
/*}}}*/
/*{{{  int crypto_shutdown (void)*/
/*
 *	shuts-down the crypto bits
 *	returns 0 on success, non-zero on failure
 */
int crypto_shutdown (void)
{
	return 0;
}
/*}}}*/



