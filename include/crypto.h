/*
 *	crypto.h -- cryptographic/security aspects
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

#ifndef __CRYPTO_H
#define __CRYPTO_H

typedef struct TAG_crypto {
	void *priv;
} crypto_t;


extern crypto_t *crypto_newdigest (void);
extern void crypto_freedigest (crypto_t *cry);
extern int crypto_writedigest (crypto_t *cry, unsigned char *data, int bytes);
extern char *crypto_readdigest (crypto_t *cry, int *issignedp);
extern int crypto_signdigest (crypto_t *cry, char *privfile);

extern int crypto_verifykeyfile (const char *fname, int secure);

extern int crypto_init (void);
extern int crypto_shutdown (void);

#endif	/* !__CRYPTO_H */

