/*
 *	opts.h -- command-line options processing
 *	Copyright (C) 2004 Fred Barnes <frmb@kent.ac.uk>
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

#ifndef __OPTS_H
#define __OPTS_H

typedef struct TAG_cmd_option {
	char *name;
	char sopt;
	int (*opthandler)(struct TAG_cmd_option *, char ***arg_walk, int *arg_left);
	void *arg;
	char *help;
	int order;
} cmd_option_t;

extern void opts_init (void);
extern cmd_option_t *opts_getlongopt (const char *optname);
extern cmd_option_t *opts_getshortopt (const char optchar);
extern int opts_process (cmd_option_t *opt, char ***arg_walk, int *arg_left);
extern void opts_add (const char *optname, const char optchar, int (*opthandler)(cmd_option_t *, char ***, int *), void *arg, char *help);


#endif	/* !__OPTS_H */

