/*
 *	crypto.c -- cryptographic/security stuff for nocc
 *	Copyright (C) 2006-2007 Fred Barnes <frmb@kent.ac.uk>
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
#include <arpa/inet.h>		/* for htonl/ntohl */
#include <errno.h>
#include <fcntl.h>

#if defined(USE_LIBGCRYPT)
#include <gcrypt.h>
#endif

#include "nocc.h"
#include "support.h"
#include "version.h"
#include "opts.h"
#include "origin.h"
#include "library.h"
#include "crypto.h"

/*}}}*/
/*{{{  private types/data*/
#define ICRYPTO_PUBKEY_MAGIC 0x136a8bf5
#if defined(USE_LIBGCRYPT)

typedef struct TAG_crypto_gcrypt {
	gcry_md_hd_t handle;
	gcry_error_t err;
	gcry_sexp_t signedhash;
	int dlen;
} crypto_gcrypt_t;

static int gcrypt_digestalgo = 0;

#endif

static int icrypto_initialised = 0;

/*}}}*/


#if defined(USE_LIBGCRYPT)

/*
 *	Note: some of the code here is inspired from the libgcrypt test-suite,
 *	see: http://www.gnupg.org/
 */

/*{{{  private types/data*/

static char *keycomment = NULL;

typedef struct TAG_cryptogenkey {
	char *keytype;
	char *nbits;
	char *privkeyfile;
	char *pubkeyfile;
	char *comment;
} cryptogenkey_t;


/*}}}*/


/*{{{  static int icrypto_loadkey (gcry_sexp_t *sexpp, char **commentp, const char *keyfile, int secure)*/
/*
 *	reads a key from a file into a libgcrypt S-expression
 *	returns zero on success, non-zero on failure
 */
static int icrypto_loadkey (gcry_sexp_t *sexpp, char **commentp, const char *keyfile, int secure)
{
	int fd, in;
	char *sbuf;
	gcry_error_t gerr;
	int koffs;
	gcry_sexp_t signedhash = NULL;
	gcry_sexp_t dghash = NULL;

	fd = open (keyfile, O_RDONLY);
	if (fd < 0) {
		nocc_error ("icrypto_loadkey(): failed to open %s: %s", keyfile, strerror (errno));
		return -1;
	}

	sbuf = secure ? (char *)gcry_xmalloc_secure (4096) : (char *)smalloc (4096);
	if (!sbuf) {
		close (fd);
		nocc_error ("icrypto_loadkey(): failed to allocate secure memory!");
		return -1;
	}

	for (in=0;;) {
		int x;

		x = read (fd, sbuf + in, 4095 - in);
		if (!x) {
			/* eof */
			break;		/* for() */
		} else if (x < 0) {
			int saved_errno = errno;

			close (fd);
			if (secure) {
				gcry_free (sbuf);
			} else {
				sfree (sbuf);
			}
			nocc_error ("icrypto_loadkey(): read error while reading from %s: %s", keyfile, strerror (saved_errno));
			return -1;
		}
		in += x;
	}
	/* file is binary, but stick a null on the end anyway :) */
	sbuf[in] = '\0';
	close (fd);

	*sexpp = NULL;
	*commentp = NULL;

	/* if it's a public key, may have a key comment and signed digest of it */
	if ((in > sizeof (unsigned int)) && (ntohl (*(unsigned int *)sbuf) == ICRYPTO_PUBKEY_MAGIC)) {
		int clen = ntohl (*(unsigned int *)(sbuf + sizeof (unsigned int)));
		int offs = 2 * sizeof (unsigned int);
		int pxlen = 0;
		char *ch, *pexp, *digest;
		crypto_t *cry = NULL;
		int issigned;
		size_t geoff;
		int i;

		for (ch=sbuf+offs; (*ch != '\0') && (ch < (sbuf + in)); ch++);
		if (ch == (sbuf + in)) {
			if (secure) {
				gcry_free (sbuf);
			} else {
				sfree (sbuf);
			}
			nocc_error ("icrypto_loadkey(): damaged key comment in file %s", keyfile);
			return -1;
		}
		*commentp = string_dup (sbuf + offs);
		offs += strlen (*commentp);
		offs++;					/* past null terminator */

		if (clen != strlen (*commentp)) {
			/*{{{  error*/
			if (secure) {
				gcry_free (sbuf);
			} else {
				sfree (sbuf);
			}
			nocc_error ("icrypto_loadkey(): damaged key length in file %s", keyfile);
			return -1;
			/*}}}*/
		}

		/* what follows next should be a null-terminated signed digest (in hex) */
		for (ch=sbuf+offs; (*ch != '\0') && (ch < (sbuf + in)); ch++);
		if (ch == (sbuf + in)) {
			/*{{{  error*/
			if (secure) {
				gcry_free (sbuf);
			} else {
				sfree (sbuf);
			}
			nocc_error ("icrypto_loadkey(): damaged comment digest in file %s", keyfile);
			return -1;
			/*}}}*/
		}

		/* try and recover it */
		pexp = decode_hexstr (sbuf+offs, &pxlen);
		if (!pexp) {
			/*{{{  error*/
			if (secure) {
				gcry_free (sbuf);
			} else {
				sfree (sbuf);
			}
			nocc_error ("icrypto_loadkey(): failed to decode signed digest in file %s", keyfile);
			return -1;
			/*}}}*/
		}

		offs += strlen (sbuf+offs);
		offs++;					/* past null terminator */

		signedhash = NULL;
		gerr = gcry_sexp_new (&signedhash, (unsigned char *)pexp, pxlen, 1);
		if (gerr) {
			/*{{{  error*/
			if (secure) {
				gcry_free (sbuf);
			} else {
				sfree (sbuf);
			}
			nocc_error ("icrypto_loadkey(): failed to decode signed digest S-expression in file %s", keyfile);
			return -1;
			/*}}}*/
		}

		/* digest run */
		cry = crypto_newdigest ();
		if (!cry) {
			/*{{{  error*/
			if (secure) {
				gcry_free (sbuf);
			} else {
				sfree (sbuf);
			}
			nocc_error ("icrypto_loadkey(): failed to create digest");
			return -1;
			/*}}}*/
		}
		crypto_writedigest (cry, (unsigned char *)(*commentp), clen);

		digest = crypto_readdigest (cry, &issigned);
		if (!digest || issigned) {
			/*{{{  error*/
			crypto_freedigest (cry);

			if (secure) {
				gcry_free (sbuf);
			} else {
				sfree (sbuf);
			}
			nocc_error ("icrypto_loadkey(): failed to read digest");
			return -1;
			/*}}}*/
		}

		/* convert to upper case */
		for (ch=digest; *ch != '\0'; ch++) {
			if ((*ch >= 'a') && (*ch <= 'f')) {
				*ch -= 'a';
				*ch += 'A';
			}
		}

		/* turn hash back into S-expression*/
		ch = (char *)smalloc (2048);
		i = snprintf (ch, 2047, "(data\n (flags pkcs1)\n (hash %s #%s#))\n", compopts.hashalgo, digest);
		gerr = gcry_sexp_sscan (&dghash, &geoff, ch, i);
		sfree (ch);
		sfree (digest);
		crypto_freedigest (cry);

		if (gerr) {
			/*{{{  error*/
			if (secure) {
				gcry_free (sbuf);
			} else {
				sfree (sbuf);
			}
			nocc_error ("icrypto_loadkey(): failed to turn digest into S-expression: %s", gpg_strerror (gerr));
			return -1;
			/*}}}*/
		}

		koffs = offs;
	} else {
		koffs = 0;
	}

	gerr = gcry_sexp_new (sexpp, sbuf+koffs, in-koffs, 1);
	if (secure) {
		gcry_free (sbuf);
	} else {
		sfree (sbuf);
	}
	if (gerr) {
		if (*sexpp) {
			gcry_sexp_release (*sexpp);
			*sexpp = NULL;
		}
		nocc_error ("icrypto_loadkey(): failed to convert S-expression: %s", gpg_strerror (gerr));
		return -1;
	}

	/* finally, verify that digest is signed with the public key */
	if (*commentp && signedhash && dghash) {
		gerr = gcry_pk_verify (signedhash, dghash, *sexpp);

		gcry_sexp_release (signedhash);
		gcry_sexp_release (dghash);

		if (gerr) {
			sfree (*commentp);
			*commentp = NULL;
			nocc_error ("icrypto_loadkey(): failed to verify comment in key-file %s: %s", keyfile, gpg_strerror (gerr));
			return -1;
		}
	}

	return 0;
}
/*}}}*/
/*{{{  static int icrypto_intsigndigest (crypto_t *cry, gcry_sexp_t privkey)*/
/*
 *	signs the digest using the specified private key
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_intsigndigest (crypto_t *cry, gcry_sexp_t privkey)
{
	crypto_gcrypt_t *gc = (crypto_gcrypt_t *)cry->priv;
	unsigned char *digest;
	gcry_sexp_t hash, signedhash;
	gcry_error_t gerr;
	size_t geoff;
	char *hexstr, *ch, *secbuf;
	int i;

	/*{{{  get the digest in hex*/
	digest = gcry_md_read (gc->handle, gcrypt_digestalgo);
	if (!digest) {
		nocc_error ("icrypto_signdigest(): gcry_md_read() returned NULL!");
		return -1;
	}
	hexstr = mkhexbuf (digest, gc->dlen);
	for (ch=hexstr; *ch != '\0'; ch++) {
		if ((*ch >= 'a') && (*ch <= 'f')) {
			*ch -= 'a';
			*ch += 'A';
		}
	}

	/*}}}*/
	/*{{{  convert hash into S-expression*/
	secbuf = (char *)gcry_malloc_secure (512);
	if (!secbuf) {
		nocc_error ("icrypto_signdigest(): failed to allocate secure memory");
		sfree (hexstr);
		return -1;
	}
	i = sprintf (secbuf, "(data\n (flags pkcs1)\n (hash %s #%s#))\n", compopts.hashalgo, hexstr);
	gerr = gcry_sexp_sscan (&hash, &geoff, secbuf, i);
	gcry_free (secbuf);
	secbuf = NULL;
#if 0
fprintf (stderr, "gerr = %d, geoff = %d, hexstr = [%s]\n", gerr, (int)geoff, hexstr);
#endif
	sfree (hexstr);
	if (gerr) {
		nocc_error ("icrypto_signdigest(): failed to build S-expression: %s", gpg_strerror (gerr));
		return -1;
	}

	/*}}}*/
	/*{{{  load private key and sign*/

	gerr = gcry_pk_sign (&signedhash, hash, privkey);

	gcry_sexp_release (hash);

	if (gerr) {
		nocc_error ("icrypto_signdigest(): failed to sign digest: %s", gpg_strerror (gerr));
		return -1;
	}

	/*}}}*/

	if (gc->signedhash) {
		gcry_sexp_release (gc->signedhash);
	}
	gc->signedhash = signedhash;

	return 0;
}
/*}}}*/
/*{{{  static int icrypto_genkeypair (cryptogenkey_t *cgkey)*/
/*
 *	deals with making a new public/private key-pair
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_genkeypair (cryptogenkey_t *cgkey)
{
	gcry_sexp_t keyspec, key, pubkey, privkey;
	int i, fd, gone;
	char *sbuf;
	char *digest = NULL;

	sbuf = (char *)gcry_malloc_secure (64);
	if (!sbuf) {
		nocc_error ("icrypto_genkeypair(): failed to allocate secure memory!");
		return -1;
	}
	memset (sbuf, 'G', 64);

	sprintf (sbuf, "(genkey (%s (nbits %d:%s)))", cgkey->keytype, (int)strlen (cgkey->nbits), cgkey->nbits);
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
		nocc_error ("icrypto_genkeypair(): failed to create %s key-pair: %s", cgkey->keytype, gpg_strerror (i));
		return -1;
	}
#if 0
fprintf (stderr, "icrypto_genkeypair(): here 2!\n");
#endif

	/*{{{  extract public and private keys*/
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

	/*}}}*/

	gcry_sexp_release (key);

	if (compopts.verbose) {
		nocc_message ("created key-pair");
	}

	/*{{{  if commented, digest and sign*/
	if (cgkey->comment) {
		crypto_t *cry = crypto_newdigest ();
		int issigned;

		if (!cry) {
			nocc_error ("icrypto_genkeypair(): failed to create digest");
			return -1;
		}
		crypto_writedigest (cry, (unsigned char *)cgkey->comment, strlen (cgkey->comment));

		if (icrypto_intsigndigest (cry, privkey)) {
			nocc_error ("icrypto_genkeypair(): failed to sign digest");
			return -1;
		}
		digest = crypto_readdigest (cry, &issigned);

		if (!digest) {
			nocc_error ("icrypto_genkeypair(): failed to read signed digest");
			return -1;
		}

		crypto_freedigest (cry);
	}

	/*}}}*/

	/* oki, write these out! */
	sbuf = (char *)gcry_xmalloc_secure (4096);
	if (!sbuf) {
		nocc_error ("icrypto_genkeypair(): failed to allocate secure memory");
		return -1;
	}

	/*{{{  write private key*/
	fd = open (cgkey->privkeyfile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	if (fd < 0) {
		nocc_error ("icrypto_genkeypair(): failed to open %s for writing: %s", cgkey->privkeyfile, strerror (errno));
		gcry_free (sbuf);
		return -1;
	}
	i = gcry_sexp_sprint (privkey, GCRYSEXP_FMT_DEFAULT, sbuf, 4095);
	sbuf[i] = '\0';
	for (gone=0; gone < i;) {
		int x;

		x = write (fd, sbuf + gone, i - gone);
		if (x <= 0) {
			nocc_error ("icrypto_genkeypair(): error writing to %s: %s", cgkey->privkeyfile, strerror (errno));
			gcry_free (sbuf);
			close (fd);
			return -1;
		}
		gone += x;
	}
	close (fd);
	/*}}}*/
	/*{{{  write public key*/
	fd = open (cgkey->pubkeyfile, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0) {
		nocc_error ("icrypto_genkeypair(): failed to open %s for writing: %s", cgkey->pubkeyfile, strerror (errno));
		gcry_free (sbuf);
		return -1;
	}
	if (cgkey->comment && digest) {
		*(unsigned int *)sbuf = htonl (ICRYPTO_PUBKEY_MAGIC);
		i = sizeof (unsigned int);
		*(unsigned int *)(sbuf+i) = htonl (strlen (cgkey->comment));
		i += sizeof (unsigned int);
		i += snprintf (sbuf+i, 4095-i, "%s", cgkey->comment);
		sbuf[i++] = '\0';
		i += snprintf (sbuf+i, 4095-i, "%s", digest);
		sbuf[i++] = '\0';
	} else {
		i = 0;
	}

	i += gcry_sexp_sprint (pubkey, GCRYSEXP_FMT_DEFAULT, sbuf+i, 4095-i);
	sbuf[i] = '\0';
	for (gone=0; gone < i;) {
		int x;

		x = write (fd, sbuf + gone, i - gone);
		if (x <= 0) {
			nocc_error ("icrypto_genkeypair(): error writing to %s: %s", cgkey->pubkeyfile, strerror (errno));
			gcry_free (sbuf);
			close (fd);
			return -1;
		}
		gone += x;
	}
	close (fd);
	/*}}}*/

	gcry_sexp_release (pubkey);
	gcry_sexp_release (privkey);
	gcry_free (sbuf);

	return 0;
}
/*}}}*/
/*{{{  static int icrypto_dogenkeypair (void *arg)*/
/*
 *	called to generate a key (by compiler initialisation function)
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_dogenkeypair (void *arg)
{
	cryptogenkey_t *cgk = (cryptogenkey_t *)arg;
	int r;

	if (compopts.verbose) {
		nocc_message ("generating [%s] type key of [%s] bits, saving private key into [%s], public key into [%s]",
				cgk->keytype, cgk->nbits, cgk->privkeyfile, cgk->pubkeyfile);
	}

	r = icrypto_genkeypair (cgk);
	if (r) {
		nocc_error ("failed to generate public/private key-pair");
	} else {
		/* and now set for exit */
		nocc_cleanexit ();
	}

	return r;
}
/*}}}*/
/*{{{  static char *icrypto_verifysig (char *pubpath, char *libfile, char *codefile)*/
/*
 *	does signature validation -- ensures that the "codefile" and entry-data in "libfile"
 *	match the signature in "libfile", using public-key in "pubpath".
 *	returns NULL on success, error-string on failure (freed by caller)
 */
static char *icrypto_verifysig (char *pubpath, char *libfile, char *codefile)
{
	gcry_sexp_t hash = NULL;
	gcry_sexp_t dhash = NULL;
	gcry_sexp_t signedhash = NULL;
	gcry_sexp_t signeddhash = NULL;
	gcry_sexp_t pubkey;
	gcry_error_t gerr;
	crypto_t *cry = NULL;
	crypto_t *dcry = NULL;
	int fd;
	char *sbuf, *ch;
	char *hashalgo, *hashvalue, *dhashvalue;	/* from the library file */
	int i;
	size_t geoff;
	char *estr = NULL;
	char *kcomment = NULL;

	if (codefile) {
		cry = crypto_newdigest ();
		if (!cry) {
			return string_dup ("icrypto_verifysig(): failed to create digest!");
		}
	}

	dcry = crypto_newdigest ();
	if (!dcry) {
		crypto_freedigest (cry);
		return string_dup ("icrypto_verifysig(): failed to create descriptor digest!");
	}

	if (codefile) {
		/*{{{  read "codefile" and add to the digest*/
		fd = open (codefile, O_RDONLY);
		if (fd < 0) {
			estr = string_fmt ("icrypto_verifysig(): failed to open %s: %s", codefile, strerror (errno));
			crypto_freedigest (cry);
			crypto_freedigest (dcry);
			return estr;
		}
		sbuf = (char *)smalloc (4096);
		for (;;) {
			int in;

			in = read (fd, sbuf, 4095);
			if (!in) {
				/* eof */
				break;		/* for() */
			} else if (in < 0) {
				estr = string_fmt ("icrypto_verifysig(): read error from %s: %s", codefile, strerror (errno));
				crypto_freedigest (cry);
				crypto_freedigest (dcry);
				sfree (sbuf);
				close (fd);
				return estr;
			}
			sbuf[in] = '\0';

			crypto_writedigest (cry, (unsigned char *)sbuf, in);
		}
		sfree (sbuf);
		close (fd);

		/*}}}*/
	}
	/*{{{  read "libfile" and add to the digests*/
	hashalgo = NULL;
	hashvalue = NULL;
	dhashvalue = NULL;

	/* do this first so we get hashalgo */
	if (library_readlibanddigest (libfile, dcry, NULL, &hashalgo, NULL, &dhashvalue)) {
		estr = string_dup ("icrypto_verifysig(): failed to read and digest library");
		goto out_error_2;
	}
	if (codefile && library_readlibanddigest (libfile, cry, NULL, NULL, &hashvalue, NULL)) {
		estr = string_dup ("icrypto_verifysig(): failed to read and digest library");
		goto out_error_1;
	}

	/*}}}*/
	/*{{{  read public key*/
	if (icrypto_loadkey (&pubkey, &kcomment, pubpath, 0)) {
		estr = string_fmt ("icrypto_verifysig(): failed to load public key from %s", pubpath);
		goto out_error_3;
	}

	/*}}}*/
	if (codefile) {
		/*{{{  get our recently computed digest in hex*/
		sbuf = crypto_readdigest (cry, &i);
		if (!sbuf || i) {
			if (!sbuf) {
				estr = string_dup ("icrypto_verifysig(): failed to read digest");
			} else {
				estr = string_dup ("icrypto_verifysig(): digest is signed -- it should not be!");
			}
			goto out_error_4;
		}
		/* convert to upper-case */
		for (ch=sbuf; *ch != '\0'; ch++) {
			if ((*ch >= 'a') && (*ch <= 'f')) {
				*ch -= 'a';
				*ch += 'A';
			}
		}

		/*}}}*/
		/*{{{  convert hash into S-expression*/
		ch = (char *)smalloc (2048);
		i = snprintf (ch, 2048, "(data\n (flags pkcs1)\n (hash %s #%s#))\n", compopts.hashalgo, sbuf);
		hash = NULL;
		gerr = gcry_sexp_sscan (&hash, &geoff, ch, i);
		sfree (ch);

#if 0
fprintf (stderr, "gerr = %d, geoff = %d, hexstr = [%s]\n", gerr, (int)geoff, hexstr);
#endif
		sfree (sbuf);
		if (gerr) {
			estr = string_fmt ("icrypto_verifysig(): failed to build S-expression: %s", gpg_strerror (gerr));
			goto out_error_4;
		}

		/*}}}*/
	}
	/*{{{  get the descriptor digest in hex*/
	sbuf = crypto_readdigest (dcry, &i);
	if (!sbuf || i) {
		if (!sbuf) {
			estr = string_dup ("icrypto_verifysig(): failed to read descriptor digest");
		} else {
			estr = string_dup ("icrypto_verifysig(): descriptor digest is signed -- it should not be!");
		}
		goto out_error_4;
	}
	/* convert to upper-case */
	for (ch=sbuf; *ch != '\0'; ch++) {
		if ((*ch >= 'a') && (*ch <= 'f')) {
			*ch -= 'a';
			*ch += 'A';
		}
	}

	/*}}}*/
	/*{{{  convert descriptor hash back into S-expression*/
	ch = (char *)smalloc (2048);
	i = snprintf (ch, 2047, "(data\n (flags pkcs1)\n (hash %s #%s#))\n", compopts.hashalgo, sbuf);
	dhash = NULL;
	gerr = gcry_sexp_sscan (&dhash, &geoff, ch, i);
	sfree (ch);

#if 0
fprintf (stderr, "gerr = %d, geoff = %d, hexstr = [%s]\n", gerr, (int)geoff, hexstr);
#endif
	sfree (sbuf);
	if (gerr) {
		estr = string_fmt ("icrypto_verifysig(): failed to build descriptor S-expression: %s", gpg_strerror (gerr));
		goto out_error_4;
	}

	/*}}}*/
	/*{{{  done with digests*/
	if (codefile) {
		crypto_freedigest (cry);
	}
	crypto_freedigest (dcry);
	cry = dcry = NULL;

	/*}}}*/
	if (codefile) {
		/*{{{  turn "hashvalue" back into an S-expression in "signedhash"*/
		{
			int sslen;
			char *ssexp = decode_hexstr (hashvalue, &sslen);

			if (!ssexp) {
				estr = string_fmt ("icrypto_verifysig(): bad hash hex-string in library [%s]", libfile);
				goto out_error_5;
			}

			signedhash = NULL;
			gerr = gcry_sexp_new (&signedhash, (unsigned char *)ssexp, sslen, 1);
			sfree (ssexp);

			if (gerr) {
				estr = string_fmt ("icrypto_verifysig(): bad hash in library [%s]: %s", libfile, gpg_strerror (gerr));
				goto out_error_5;
			}
		}

		/*}}}*/
	}
	/*{{{  turn "dhashvalue" back into an S-expression in "signeddhash"*/
	{
		int sslen;
		char *ssexp = decode_hexstr (dhashvalue, &sslen);

		if (!ssexp) {
			estr = string_fmt ("icrypto_verifysig(): bad descriptor hash hex-string in library [%s]", libfile);
			goto out_error_5;
		}

		signeddhash = NULL;
		gerr = gcry_sexp_new (&signeddhash, (unsigned char *)ssexp, sslen, 1);
		sfree (ssexp);

		if (gerr) {
			estr = string_fmt ("icrypto_verifysig(): bad descriptor hash in library [%s]: %s", libfile, gpg_strerror (gerr));
			goto out_error_5;
		}
	}

	/*}}}*/
	/*{{{  do the actual check (finally!)*/
	if (codefile) {
		gerr = gcry_pk_verify (signedhash, hash, pubkey);

		if (gerr) {
			/* failed gracefully */
			estr = string_fmt ("failed to verify signature: %s", gpg_strerror (gerr));
			goto out_error_5;
		}
	}

	gerr = gcry_pk_verify (signeddhash, dhash, pubkey);

	if (gerr) {
		/* failed gracefully */
		estr = string_fmt ("failed to verify descriptor signature: %s", gpg_strerror (gerr));
		goto out_error_5;
	}

	gcry_sexp_release (signedhash);
	gcry_sexp_release (signeddhash);
	gcry_sexp_release (hash);
	gcry_sexp_release (dhash);
	gcry_sexp_release (pubkey);

	sfree (hashalgo);
	sfree (hashvalue);
	sfree (dhashvalue);

	/*}}}*/

	/* otherwise good :) */
	return NULL;

out_error_5:
	if (signedhash) {
		gcry_sexp_release (signedhash);
	}
	if (signeddhash) {
		gcry_sexp_release (signeddhash);
	}
out_error_4:
	if (hash) {
		gcry_sexp_release (hash);
	}
	if (dhash) {
		gcry_sexp_release (dhash);
	}
	gcry_sexp_release (pubkey);
out_error_3:
	sfree (dhashvalue);
out_error_2:
	sfree (hashalgo);
	sfree (hashvalue);
out_error_1:
	if (kcomment) {
		sfree (kcomment);
	}
	if (cry) {
		crypto_freedigest (cry);
	}
	if (dcry) {
		crypto_freedigest (dcry);
	}
	return estr;
}
/*}}}*/
/*{{{  static int icrypto_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)*/
/*
 *	option handler for cryptographic options
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_opthandler (cmd_option_t *opt, char ***argwalk, int *argleft)
{
	int optv = (int)((uint64_t)opt->arg);

	switch (optv) {
		/*{{{  --genkey <priv-path>,<pub-path>,<type>,<nbits>*/
	case 0:
		{
			char *argcopy = NULL;
			char *argbits[4] = {NULL, };
			int i;
			char *ch;
			cryptogenkey_t *cgk = NULL;

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

			cgk = (cryptogenkey_t *)smalloc (sizeof (cryptogenkey_t));
			cgk->keytype = string_dup (argbits[2]);
			cgk->nbits = string_dup (argbits[3]);
			cgk->privkeyfile = string_dup (argbits[0]);
			cgk->pubkeyfile = string_dup (argbits[1]);
			cgk->comment = keycomment ?: NULL;
			keycomment = NULL;

			nocc_addcompilerinitfunc ("cryptokeygen", INTERNAL_ORIGIN, icrypto_dogenkeypair, (void *)cgk);

			sfree (argcopy);
		}
		break;
		/*}}}*/
		/*{{{  --verify <xlo-path>,<file-path>,<pub-path>*/
	case 1:
		{
			char *argcopy = NULL;
			char *argbits[3] = {NULL, };
			int i;
			char *ch, *estr;

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
			for (i=0, ch=argcopy; (i<3) && (*ch != '\0'); i++) {
				char *dh;

				for (dh=ch+1; (*dh != '\0') && (*dh != ','); dh++);
				if ((*dh == '\0') && (i < 2)) {
					nocc_error ("malformed argument for verify");
					sfree (argcopy);
					return -1;
				}
				*dh = '\0';
				argbits[i] = string_dup (ch);
				ch = dh + 1;
			}

			estr = icrypto_verifysig (argbits[2], argbits[0], argbits[1]);
			if (estr) {
				nocc_fatal ("failed to verify signature: %s", estr);
				sfree (estr);
			}
			if (compopts.verbose) {
				nocc_message ("signature in %s checks out ok", argbits[0]);
			}

			sfree (argcopy);
			nocc_cleanexit ();
		}
		break;
		/*}}}*/
		/*{{{ --verify-desc <xlo-path>,<pub-path>*/
	case 2:
		{
			char *argcopy = NULL;
			char *argbits[2] = {NULL, };
			int i;
			char *ch, *estr;

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
			for (i=0, ch=argcopy; (i<2) && (*ch != '\0'); i++) {
				char *dh;

				for (dh=ch+1; (*dh != '\0') && (*dh != ','); dh++);
				if ((*dh == '\0') && (i < 1)) {
					nocc_error ("malformed argument for verify");
					sfree (argcopy);
					return -1;
				}
				*dh = '\0';
				argbits[i] = string_dup (ch);
				ch = dh + 1;
			}

			estr = icrypto_verifysig (argbits[1], argbits[0], NULL);
			if (estr) {
				nocc_fatal ("failed to verify descriptor signature: %s", estr);
				sfree (estr);
			}
			if (compopts.verbose) {
				nocc_message ("descriptor signature in %s checks out ok", argbits[0]);
			}

			sfree (argcopy);
			nocc_cleanexit ();
		}
		break;
		/*}}}*/
		/*{{{  --hashalgo <name>*/
	case 3:
		{
			char *ch;

			if ((ch = strchr (**argwalk, '=')) != NULL) {
				ch++;
			} else {
				(*argwalk)++;
				(*argleft)--;
				if (!**argwalk || !*argleft) {
					nocc_error ("missing argument for option %s", (*argwalk)[-1]);
					return -1;
				}
				ch = **argwalk;
			}

			if (compopts.hashalgo) {
				sfree (compopts.hashalgo);
			}
			compopts.hashalgo = string_dup (ch);
		}
		break;
		/*}}}*/
		/*{{{  --privkey <keyfile>*/
	case 4:
		{
			char *ch;

			if ((ch = strchr (**argwalk, '=')) != NULL) {
				ch++;
			} else {
				(*argwalk)++;
				(*argleft)--;
				if (!**argwalk || !*argleft) {
					nocc_error ("missing argument for option %s", (*argwalk)[-1]);
					return -1;
				}
				ch = **argwalk;
			}

			if (compopts.privkey) {
				sfree (compopts.privkey);
			}
			compopts.privkey = string_dup (ch);
		}
		break;
		/*}}}*/
		/*{{{  --keycomment <comment>*/
	case 5:
		{
			char *ch;

			if ((ch = strchr (**argwalk, '=')) != NULL) {
				ch++;
			} else {
				(*argwalk)++;
				(*argleft)--;
				if (!**argwalk || !*argleft) {
					nocc_error ("missing argument for option %s", (*argwalk)[-1]);
					return -1;
				}
				ch = **argwalk;
			}

			if (keycomment) {
				sfree (keycomment);
			}
			keycomment = string_dup (ch);
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
	gc->signedhash = NULL;

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
/*{{{  static char *icrypto_readdigest (crypto_t *cry, int *issignedp)*/
/*
 *	reads the generated digest so far
 *	returns a fresh formatted string on success, NULL on failure
 */
static char *icrypto_readdigest (crypto_t *cry, int *issignedp)
{
	crypto_gcrypt_t *gc = (crypto_gcrypt_t *)cry->priv;
	unsigned char *digest;

	if (gc->signedhash) {
		/* got a signed one */
		int i;
		char *hbuf;

		digest = (unsigned char *)smalloc (4096);
		i = gcry_sexp_sprint (gc->signedhash, GCRYSEXP_FMT_CANON, digest, 4095);
		digest[i] = '\0';

		hbuf = mkhexbuf (digest, i);
		sfree (digest);

		if (issignedp) {
			*issignedp = 1;
		}

		return hbuf;
	}

	digest = gcry_md_read (gc->handle, gcrypt_digestalgo);
	if (!digest) {
		nocc_error ("icrypto_readdigest(): gcry_md_read() returned NULL!");
		return NULL;
	}
	if (issignedp) {
		*issignedp = 0;
	}

	return mkhexbuf (digest, gc->dlen);
}
/*}}}*/
/*{{{  static int icrypto_signdigest (crypto_t *cry, char *privfile)*/
/*
 *	internal digest sign routine
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_signdigest (crypto_t *cry, char *privfile)
{
	gcry_sexp_t privkey;
	int v;
	char *kcomment = NULL;

	/*{{{  load private key*/
	if (icrypto_loadkey (&privkey, &kcomment, privfile, 1)) {
		/* already complained */
		return -1;
	}

	/*}}}*/

	v = icrypto_intsigndigest (cry, privkey);
	gcry_sexp_release (privkey);

	if (kcomment) {
		sfree (kcomment);
	}

	return v;
}
/*}}}*/
/*{{{  static int icrypto_verifykeyfile (const char *fname, int secure)*/
/*
 *	verifies a key file
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_verifykeyfile (const char *fname, int secure)
{
	gcry_sexp_t key;
	char *kcomment = NULL;

	if (icrypto_loadkey (&key, &kcomment, fname, secure)) {
		return -1;
	}
	gcry_sexp_release (key);
	if (kcomment) {
		sfree (kcomment);
	}
	return 0;
}
/*}}}*/
/*{{{  static int icrypto_verifylibfile (const char *libfile, const char **pubkeys, int npubkeys)*/
/*
 *	verifies a library file (checks metadata only)
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_verifylibfile (const char *libfile, const char **pubkeys, int npubkeys)
{
	libdigestset_t *ldset;
	gcry_sexp_t *pkeys;
	char **pkcomments;
	int i;
	int r = 0;

	ldset = library_readlibanddigestset (libfile);
	if (!ldset) {
		nocc_error ("icrypto_verifylibfile(): failed to read and digest library [%s]", libfile);
		return -1;
	}

	for (i=0; i<DA_CUR (ldset->entries); i++) {
		libdigestinfo_t *ldi = DA_NTHITEM (ldset->entries, i);

		ldi->checked = 0;
	}

	/* read public keys */
	pkeys = (gcry_sexp_t *)smalloc (npubkeys * sizeof (gcry_sexp_t));
	pkcomments = (char **)smalloc (npubkeys * sizeof (char *));
	for (i=0; i<npubkeys; i++) {
		pkcomments[i] = NULL;
		if (icrypto_loadkey (&(pkeys[i]), &(pkcomments[i]), pubkeys[i], 0)) {
			nocc_warning ("icrypto_verifylibfile(): failed to load public key from [%s]", pubkeys[i]);
			pkeys[i] = NULL;
		}
	}

	/* for each entry, get the digest info */
	for (i=0; i<DA_CUR (ldset->entries); i++) {
		libdigestinfo_t *ldi = DA_NTHITEM (ldset->entries, i);
		int issigned;
		char *sbuf = crypto_readdigest (ldi->cry, &issigned);

		if (!sbuf) {
			nocc_warning ("icrypto_verifylibfile(): failed to get digest for src-unit [%s] in library [%s]", ldi->srcunit, libfile);
		} else if (issigned) {
			sfree (sbuf);
			nocc_warning ("icrypto_verifylibfile(): digest for src-unit [%s] in library [%s] is signed", ldi->srcunit, libfile);
		} else {
			gcry_sexp_t hash;
			gcry_error_t gerr;
			size_t geoff;
			char *ch;

			for (ch=sbuf; *ch != '\0'; ch++) {
				if ((*ch >= 'a') && (*ch <= 'f')) {
					*ch -= 'a';
					*ch += 'A';
				}
			}
			ch = (char *)smalloc (2048);
			i = snprintf (ch, 2048, "(data\n (flags pkcs1)\n (hash %s #%s#))\n", ldi->hashalgo, sbuf);
			hash = NULL;
			gerr = gcry_sexp_sscan (&hash, &geoff, ch, i);
			sfree (ch);
			sfree (sbuf);

			if (gerr) {
				nocc_warning ("icrypto_verifylibfile(): failed to build S-expression for src-unit [%s] in library [%s] : %s",
						ldi->srcunit, libfile, gpg_strerror (gerr));
			} else {
				int sslen;
				char *ssexp = decode_hexstr (ldi->sdhash, &sslen);

				if (!ssexp) {
					nocc_warning ("icrypto_verifylibfile(): bad descriptor hash hex-string for src-unit [%s] in library [%s]",
							ldi->srcunit, libfile);
				} else {
					gcry_sexp_t signeddhash;

					gerr = gcry_sexp_new (&signeddhash, (unsigned char *)ssexp, sslen, 1);
					sfree (ssexp);

					if (gerr) {
						nocc_warning ("icrypto_verifylibfile(): bad descriptor hash for src-unit [%s] in library [%s]: %s",
								ldi->srcunit, libfile, gpg_strerror (gerr));
					} else {
						/* do the check in here (finally!) */
						int j;

						for (j=0; (j<npubkeys) && !ldi->checked; j++) {
							if (pkeys[j]) {
								gerr = gcry_pk_verify (signeddhash, hash, pkeys[j]);
								if (!gerr) {
									/* succeeded! */
									if (compopts.verbose) {
										nocc_message ("verified src-unit [%s] in library [%s] with key [%s] id [%s]",
												ldi->srcunit, libfile, pubkeys[j], pkcomments[j] ?: "(none)");
									}
									ldi->checked = 1;
								}
							}
						}
						if (!ldi->checked) {
							nocc_error ("icrypto_verifylibfile(): failed to verify src-unit [%s] in library [%s]",
									ldi->srcunit, libfile);
							r++;
						}

						gcry_sexp_release (signeddhash);
					}
				}
				gcry_sexp_release (hash);
			}
		}
	}

	for (i=0; i<npubkeys; i++) {
		if (pkeys[i]) {
			gcry_sexp_release (pkeys[i]);
		}
		if (pkcomments[i]) {
			sfree (pkcomments[i]);
		}
	}
	sfree (pkeys);
	sfree (pkcomments);

	library_freelibdigestset (ldset);

	return r;
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

	/*{{{  command-line options: "--genkey=", "--verify", "--verify-desc", "--hashalgo", "--privkey", "--keycomment"*/
	opts_add ("genkey", '\0', icrypto_opthandler, (void *)0, "1generate new public/private key pair: <priv-key-path>,<pub-key-path>,<type>,<nbits>");
	opts_add ("verify", '\0', icrypto_opthandler, (void *)1, "1verify existing signature: <xlo-path>,<file-path>,<pub-key-path>");
	opts_add ("verify-desc", '\0', icrypto_opthandler, (void *)2, "1verify descriptor signature only: <xlo-path>,<pub-key-path>");
	opts_add ("hashalgo", 'H', icrypto_opthandler, (void *)3, "1use named algorithm for output hashing");
	opts_add ("privkey", 'P', icrypto_opthandler, (void *)4, "1sign compiler output with given key-file");
	opts_add ("keycomment", '\0', icrypto_opthandler, (void *)5, "1comment/ID for generated key (must be before --genkey)");

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
/*{{{  static char *icrypto_readdigest (crypto_t *cry, int *issignedp)*/
/*
 *	dummy read digest
 */
static char *icrypto_readdigest (crypto_t *cry, int *issignedp)
{
	if (issignedp) {
		*issignedp = 0;
	}
	return NULL;
}
/*}}}*/
/*{{{  static int icrypto_signdigest (crypto_t *cry, char *privfile)*/
/*
 *	dummy sign digest
 */
static int icrypto_signdigest (crypto_t *cry, char *privfile)
{
	return -1;
}
/*}}}*/
/*{{{  static int icrypto_verifykeyfile (const char *fname, int secure)*/
/*
 *	dummy verify key-file
 */
static int icrypto_verifykeyfile (const char *fname, int secure)
{
	return -1;
}
/*}}}*/
/*{{{  static int icrypto_verifylibfile (const char *libfile, const char **pubkeys, int npubkeys)*/
/*
 *	dummy verify library-file
 */
static int icrypto_verifylibfile (const char *libfile, const char **pubkeys, int npubkeys)
{
	return -1;
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
/*{{{  char *crypto_readdigest (crypto_t *cry, int *issignedp)*/
/*
 *	reads a crypto digest
 *	return the digest (as a new formatted hex-string) on success, or NULL on failure
 */
char *crypto_readdigest (crypto_t *cry, int *issignedp)
{
	if (cry) {
		return icrypto_readdigest (cry, issignedp);
	}
	return NULL;
}
/*}}}*/
/*{{{  int crypto_signdigest (crypto_t *cry, char *privfile)*/
/*
 *	signs a digest with a private key.  if "privfile" is NULL, compopts.privkey is used
 *	returns 0 on success, non-zero on failure
 */
int crypto_signdigest (crypto_t *cry, char *privfile)
{
	if (!privfile) {
		privfile = compopts.privkey;
	}
	if (!privfile || !cry) {
		return -1;
	}

	return icrypto_signdigest (cry, privfile);
}
/*}}}*/

/*{{{  int crypto_verifykeyfile (const char *fname, int secure)*/
/*
 *	verifies a key file
 *	returns 0 on success, non-zero on failure
 */
int crypto_verifykeyfile (const char *fname, int secure)
{
	if (!fname) {
		return -1;
	}
	return icrypto_verifykeyfile (fname, secure);
}
/*}}}*/
/*{{{  int crypto_verifylibfile (const char *libfile, const char **pubkeys, int npubkeys)*/
/*
 *	verifies a library file -- that the descriptor digest is signed with the given key
 *	returns 0 on success, non-zero on failure
 */
int crypto_verifylibfile (const char *libfile, const char **pubkeys, int npubkeys)
{
	if (!libfile || !pubkeys) {
		return -1;
	}
	return icrypto_verifylibfile (libfile, pubkeys, npubkeys);
}
/*}}}*/


/*{{{  int crypto_init (void)*/
/*
 *	initialises the crypto bits
 *	returns 0 on success, non-zero on failure
 */
int crypto_init (void)
{
	if (!icrypto_initialised) {
		int v = icrypto_init ();

		if (!v) {
			icrypto_initialised++;
		}
		return v;
	}
	return 0;
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



