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
#include "library.h"
#include "crypto.h"

/*}}}*/
/*{{{  private types/data*/
#if defined(USE_LIBGCRYPT)

typedef struct TAG_crypto_gcrypt {
	gcry_md_hd_t handle;
	gcry_error_t err;
	gcry_sexp_t signedhash;
	int dlen;
} crypto_gcrypt_t;

static int gcrypt_digestalgo = 0;

#endif

/*}}}*/


#if defined(USE_LIBGCRYPT)

/*
 *	Note: some of the code here is inspired from the libgcrypt test-suite,
 *	see: http://www.gnupg.org/
 */


/*{{{  static int icrypto_loadkey (gcry_sexp_t *sexpp, char *keyfile, int secure)*/
/*
 *	reads a key from a file into a libgcrypt S-expression
 *	returns zero on success, non-zero on failure
 */
static int icrypto_loadkey (gcry_sexp_t *sexpp, char *keyfile, int secure)
{
	int fd, in;
	char *sbuf;
	gcry_error_t gerr;

	fd = open (keyfile, O_RDONLY);
	if (fd < 0) {
		nocc_error ("icrypto_loadprivkey(): failed to open %s: %s", keyfile, strerror (errno));
		return -1;
	}

	sbuf = secure ? (char *)gcry_xmalloc_secure (4096) : (char *)smalloc (4096);
	if (!sbuf) {
		close (fd);
		nocc_error ("icrypto_loadprivkey(): failed to allocate secure memory!");
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
			nocc_error ("icrypto_loadprivkey(): read error while reading from %s: %s", keyfile, strerror (saved_errno));
			return -1;
		}
		in += x;
	}
	/* file is binary, but stick a null on the end anyway :) */
	sbuf[in] = '\0';
	close (fd);

	*sexpp = NULL;
	gerr = gcry_sexp_new (sexpp, sbuf, in, 1);
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
		nocc_error ("icrypto_loadprivkey(): failed to convert S-expression: %s", gpg_strerror (gerr));
		return -1;
	}

	return 0;
}
/*}}}*/
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

	/*{{{  write private key*/
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
	/*}}}*/
	/*{{{  write public key*/
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
	/*}}}*/

	gcry_sexp_release (pubkey);
	gcry_sexp_release (privkey);
	gcry_free (sbuf);

	return 0;
}
/*}}}*/
/*{{{  static int icrypto_verifysig (char *pubpath, char *libfile, char *codefile)*/
/*
 *	does signature validation -- ensures that the "codefile" and entry-data in "libfile"
 *	match the signature in "libfile", using public-key in "pubpath".
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_verifysig (char *pubpath, char *libfile, char *codefile)
{
	gcry_sexp_t hash, signedhash, pubkey;
	gcry_error_t gerr;
	crypto_t *cry;
	int fd;
	char *sbuf, *ch;
	char *hashalgo, *hashvalue;	/* from the library file */
	int i;
	size_t geoff;

	cry = crypto_newdigest ();
	if (!cry) {
		nocc_error ("icrypto_verifysig(): failed to create digest!");
		return -1;
	}

	/*{{{  read "codefile" and add to the digest*/
	fd = open (codefile, O_RDONLY);
	if (fd < 0) {
		nocc_error ("icrypto_verifysig(): failed to open %s: %s", codefile, strerror (errno));
		crypto_freedigest (cry);
		return -1;
	}
	sbuf = (char *)smalloc (4096);
	for (;;) {
		int in;

		in = read (fd, sbuf, 4095);
		if (!in) {
			/* eof */
			break;		/* for() */
		} else if (in < 0) {
			nocc_error ("icrypto_verifysig(): read error from %s: %s", codefile, strerror (errno));
			crypto_freedigest (cry);
			sfree (sbuf);
			close (fd);
			return -1;
		}
		sbuf[in] = '\0';

		crypto_writedigest (cry, (unsigned char *)sbuf, in);
	}
	sfree (sbuf);
	close (fd);

	/*}}}*/
	/*{{{  read "libfile" and add to the digest*/
	hashalgo = NULL;
	hashvalue = NULL;

	if (library_readlibanddigest (libfile, cry, NULL, &hashalgo, &hashvalue)) {
		nocc_error ("icrypto_verifysig(): failed to read and digest library");
		crypto_freedigest (cry);
		return -1;
	}

	/*}}}*/
	/*{{{  read public key*/
	if (icrypto_loadkey (&pubkey, pubpath, 0)) {
		nocc_error ("icrypto_verifysig(): failed to load public key from %s", pubpath);
		crypto_freedigest (cry);
		sfree (hashalgo);
		sfree (hashvalue);
		return -1;
	}

	/*}}}*/
	/*{{{  get our recently computed digest in hex*/
	sbuf = crypto_readdigest (cry, &i);
	if (!sbuf || i) {
		if (!sbuf) {
			nocc_error ("icrypto_verifysig(): failed to read digest");
		} else {
			nocc_error ("icrypto_verifysig(): digest is signed -- it should not be!");
		}
		crypto_freedigest (cry);
		sfree (hashalgo);
		sfree (hashvalue);
		gcry_sexp_release (pubkey);
		return -1;
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
	i = sprintf (ch, "(data\n (flags pkcs1)\n (hash %s #%s#))\n", compopts.hashalgo, sbuf);
	hash = NULL;
	gerr = gcry_sexp_sscan (&hash, &geoff, ch, i);
	sfree (ch);

#if 0
fprintf (stderr, "gerr = %d, geoff = %d, hexstr = [%s]\n", gerr, (int)geoff, hexstr);
#endif
	sfree (sbuf);
	if (gerr) {
		nocc_error ("icrypto_verifysig(): failed to build S-expression: %s", gpg_strerror (gerr));
		crypto_freedigest (cry);
		sfree (hashalgo);
		sfree (hashvalue);
		gcry_sexp_release (pubkey);
		return -1;
	}

	/* done with the digest now */
	crypto_freedigest (cry);

	/*}}}*/
	/*{{{  turn "hashvalue" back into an S-expression in "signedhash"*/
	{
		int sslen;
		char *ssexp = decode_hexstr (hashvalue, &sslen);

		if (!ssexp) {
			nocc_error ("icrypto_verifysig(): bad hash hex-string in library [%s]", libfile);
			sfree (hashalgo);
			sfree (hashvalue);
			gcry_sexp_release (hash);
			gcry_sexp_release (pubkey);
			return -1;
		}

		signedhash = NULL;
		gerr = gcry_sexp_new (&signedhash, (unsigned char *)ssexp, sslen, 1);
		sfree (ssexp);

		if (gerr) {
			nocc_error ("icrypto_verifysig(): bad hash in library [%s]: %s", libfile, gpg_strerror (gerr));
			sfree (hashalgo);
			sfree (hashvalue);
			gcry_sexp_release (hash);
			gcry_sexp_release (pubkey);
			return -1;
		}
	}

	/*}}}*/
	/*{{{  do the actual check (finally!)*/
	gerr = gcry_pk_verify (signedhash, hash, pubkey);

	gcry_sexp_release (hash);
	gcry_sexp_release (signedhash);
	gcry_sexp_release (pubkey);

	if (gerr) {
		/* failed gracefully */
		nocc_error ("failed to verify signature: %s", gpg_strerror (gerr));
		return -1;
	}

	/*}}}*/

	/* otherwise good :) */
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

			/* and now set for exit */
			nocc_cleanexit ();
		}
		break;
		/*}}}*/
		/*{{{  --verify <xlo-path>,<file-path>,<pub-path>*/
	case 1:
		{
			char *argcopy = NULL;
			char *argbits[3] = {NULL, };
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

			if (icrypto_verifysig (argbits[2], argbits[0], argbits[1])) {
				nocc_fatal ("failed to verify signature");
			}
			if (compopts.verbose) {
				nocc_message ("signature in %s checks out ok", argbits[0]);
			}

			sfree (argcopy);
			nocc_cleanexit ();
		}
		break;
		/*}}}*/
		/*{{{  --hashalgo <name>*/
	case 2:
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

			if (compopts.privkey) {
				sfree (compopts.privkey);
			}
			compopts.privkey = string_dup (ch);
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
 *	signs the digest using the specified private key
 *	returns 0 on success, non-zero on failure
 */
static int icrypto_signdigest (crypto_t *cry, char *privfile)
{
	crypto_gcrypt_t *gc = (crypto_gcrypt_t *)cry->priv;
	unsigned char *digest;
	gcry_sexp_t hash, signedhash, privkey;
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
	if (icrypto_loadkey (&privkey, privfile, 1)) {
		/* already complained */
		gcry_sexp_release (hash);
		return -1;
	}

	gerr = gcry_pk_sign (&signedhash, hash, privkey);

	gcry_sexp_release (privkey);			/* done with these now */
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

	/*{{{  command-line options: "--genkey=<private-key-path>,<public-key-path>,<type>,<nbits>", "--verify=<.xlo>,<.etc>,<pub-key-path>"*/
	opts_add ("genkey", '\0', icrypto_opthandler, (void *)0, "1generate new public/private key pair: <priv-key-path>,<pub-key-path>,<type>,<nbits>");
	opts_add ("verify", '\0', icrypto_opthandler, (void *)1, "1verify existing signature: <xlo-path>,<file-path>,<pub-key-path>");
	opts_add ("hashalgo", 'H', icrypto_opthandler, (void *)2, "1use named algorithm for output hashing");
	opts_add ("privkey", 'P', icrypto_opthandler, (void *)3, "1sign compiler output with given key-file");

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



