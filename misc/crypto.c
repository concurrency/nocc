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
#include <fcntl.h>

#if defined(USE_LIBGCRYPT)
#include <gcrypt.h>
#endif

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "opts.h"
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


/*{{{  static int icrypto_genkeypair (char *privpath, char *pubpath, char *type, char *nbits)*/
/*
 *	deals with making a new public/private key-pair
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_genkeypair (char *privpath, char *pubpath, char *type, char *nbits)
{
	gcry_sexp_t keyspec, key, pubkey, privkey;
	int i, fd, gone;
	char *sbuf;

	sbuf = (char *)gcry_malloc_secure (64);
	if (!sbuf) {
		nocc_error ("icrypto_genkeypair(): failed to allocate secure memory!");
		return -1;
	}
	memset (sbuf, 'G', 64);

	sprintf (sbuf, "(genkey (%s (nbits %d:%s)))", type, strlen (nbits), nbits);
	i = gcry_sexp_new (&keyspec, sbuf, 0, 1);
	gcry_free (sbuf);
	if (i) {
		nocc_error ("icrypto_genkeypair(): failed to S-expression: %s", gpg_strerror (i));
		return -1;
	}
#if 0
fprintf (stderr, "icrypto_genkeypair(): here 1!\n");
#endif

	i = gcry_pk_genkey (&key, keyspec);
	gcry_sexp_release (keyspec);
	if (i) {
		nocc_error ("icrypto_genkeypair(): failed to create %s key-pair: %s", type, gpg_strerror (i));
		return -1;
	}
#if 0
fprintf (stderr, "icrypto_genkeypair(): here 2!\n");
#endif

	pubkey = gcry_sexp_find_token (key, "public-key", 0);
	if (!pubkey) {
		nocc_error ("icrypto_genkeypair(): failed to find public-key in generated key!");
		return -1;
	}

	privkey = gcry_sexp_find_token (key, "private-key", 0);
	if (!privkey) {
		nocc_error ("icrypto_genkeypair(): failed to find private-key in generated key!");
		return -1;
	}

	gcry_sexp_release (key);

	if (compopts.verbose) {
		nocc_message ("created key-pair");
	}

	/* oki, write these out! */
	sbuf = (char *)gcry_xmalloc_secure (4096);
	if (!sbuf) {
		nocc_error ("icrypto_genkeypair(): failed to allocate secure memory!");
		return -1;
	}

	fd = open (privpath, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	if (fd < 0) {
		nocc_error ("icrypto_genkeypair(): failed to open %s for writing: %s", privpath, strerror (errno));
		gcry_free (sbuf);
		return -1;
	}
	i = gcry_sexp_sprint (privkey, GCRYSEXP_FMT_DEFAULT, sbuf, 4095);
	sbuf[i] = '\0';
	for (gone=0; gone < i;) {
		int x;

		x = write (fd, sbuf + gone, i - gone);
		if (x <= 0) {
			nocc_error ("icrypto_genkeypair(): error writing to %s: %s", privpath, strerror (errno));
			gcry_free (sbuf);
			close (fd);
			return -1;
		}
		gone += x;
	}
	close (fd);

	fd = open (pubpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0) {
		nocc_error ("icrypto_genkeypair(): failed to open %s for writing: %s", pubpath, strerror (errno));
		gcry_free (sbuf);
		return -1;
	}
	i = gcry_sexp_sprint (pubkey, GCRYSEXP_FMT_DEFAULT, sbuf, 4095);
	sbuf[i] = '\0';
	for (gone=0; gone < i;) {
		int x;

		x = write (fd, sbuf + gone, i - gone);
		if (x <= 0) {
			nocc_error ("icrypto_genkeypair(): error writing to %s: %s", pubpath, strerror (errno));
			gcry_free (sbuf);
			close (fd);
			return -1;
		}
		gone += x;
	}
	close (fd);

	gcry_sexp_release (pubkey);
	gcry_sexp_release (privkey);
	gcry_free (sbuf);

	return 0;
}
/*}}}*/
/*{{{  static int icrypto_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for cryptographic options
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)opt->arg;

	switch (optv) {
		/*{{{  --genkey <priv-path>,<pub-path>,<type>,<nbits>*/
	case 0:
		{
			char *argcopy = NULL;
			char *argbits[4] = {NULL, };
			int i;
			char *ch;

			if ((ch = strchr (**argwalk, '=')) != NULL) {
				argcopy = string_dup (ch + 1);
			} else {
				(*argwalk)++;
				(*argleft)--;
				if (!**argwalk || !*argleft) {
					nocc_error ("missing argument for option %s", (*argwalk)[-1]);
					return -1;
				}
				argcopy = string_dup (**argwalk);
			}

			/* demangle options */
			for (i=0, ch=argcopy; (i<4) && (*ch != '\0'); i++) {
				char *dh;

				for (dh=ch+1; (*dh != '\0') && (*dh != ','); dh++);
				if ((*dh == '\0') && (i < 3)) {
					nocc_error ("malformed argument for genkey");
					sfree (argcopy);
					return -1;
				}
				*dh = '\0';
				argbits[i] = string_dup (ch);
				ch = dh + 1;
			}

			if (compopts.verbose) {
				nocc_message ("generating [%s] type key of [%s] bits, saving private key into [%s], public key into [%s]", argbits[2], argbits[3], argbits[0], argbits[1]);
			}

			if (icrypto_genkeypair (argbits[0], argbits[1], argbits[2], argbits[3])) {
				nocc_error ("failed to generate public/private key-pair");
			}

			sfree (argcopy);
		}
		break;
		/*}}}*/
	default:
		nocc_error ("icrypto_opthandler(): unknown option [%s]", **argwalk);
		return -1;
	}

	return 0;
}
/*}}}*/


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

	/* sort out secure memory for cryptography */
	gcry_control (GCRYCTL_DISABLE_SECMEM_WARN);
	gcry_control (GCRYCTL_INIT_SECMEM, 16384, 0);

	if (!compopts.hashalgo) {
		compopts.hashalgo = string_dup ("sha256");
	}
	gcrypt_digestalgo = gcry_md_map_name (compopts.hashalgo);
	if (!gcrypt_digestalgo) {
		nocc_error ("icrypto_init(): do not understand hashing algorithm [%s]", compopts.hashalgo);
		return -1;
	}
	if (compopts.verbose) {
		nocc_message ("using message digest algorithm [%s] for hashing", compopts.hashalgo);
	}

	/*{{{  command-line options: "--genkey=<private-key-path>,<public-key-path>,<type>,<nbits>"*/
	opts_add ("genkey", '\0', icrypto_opthandler, (void *)0, "1generate new public/private key pair");

	/*}}}*/

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



