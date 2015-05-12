/*
 *	guppy_ev3_lib.c -- routines for Guppy on the LEGO EV3
 *	Copyright (C) 2014-2015 Fred Barnes, University of Kent <frmb@kent.ac.uk>
 */


#include <cccsp/verb-header.h>		/* ++ stdio, stdlib, stdarg, string, sys/types, cif */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <dirent.h>

#define SYS_ROOT "/sys"


/*
 *	XXX: this goes via sysfs now, so has a potentially significant overhead in terms
 *	of system calls being made (possible fix down the road somewhere).
 *
 *	Further (slight) grot: enumeration of things like tacho-motors is a minor hinderance,
 *	so we have to go hunting..
 */

typedef enum ENUM_ev3_mdev {
	MDEV_UNKNOWN	= 0,
	MDEV_L_MOTOR	= 1,		/* EV3 and NXT "large" motors */
	MDEV_M_MOTOR	= 2		/* EV3 "medium" (?) motor */
} ev3_mdev_e;

typedef enum ENUM_ev3_mst {
	MST_OFF		= 0,
	MST_FWD		= 1,
	MST_REF		= 2
} ev3_mst_e;

typedef enum ENUM_ev3_stm {
	STM_COAST	= 0,
	STM_BRAKE	= 1,
	STM_HOLD	= 2
} ev3_stm_e;

/* motor mapping for EV3 (by port 0-3 == A-D) */
typedef struct TAG_ev3_motmap {
	ev3_mdev_e dev;			/* device motor type */
	int mid;			/* motor ID (assigned by kernel) */
	ev3_mst_e st;			/* motor state */
	char path[FILENAME_MAX];	/* path to tacho-motor directory */
} ev3_motmap_t;

static ev3_motmap_t ev3_motmap[4] = {
		{MDEV_UNKNOWN, -1, MST_OFF, ""},
		{MDEV_UNKNOWN, -1, MST_OFF, ""},
		{MDEV_UNKNOWN, -1, MST_OFF, ""},
		{MDEV_UNKNOWN, -1, MST_OFF, ""}
	};

/*{{{  static int ev3_set_mot_attr_int (int port, const char *attr, int val)*/
/*
 *	writes an integer to a particular file in /sys
 *	returns 0 on success, non-zero on failure
 */
static int ev3_set_mot_attr_int (int port, const char *attr, int val)
{
	char tpath[FILENAME_MAX];
	char xbuf[32];
	int fd, wlen, i;

	snprintf (tpath, FILENAME_MAX, "%s/%s", ev3_motmap[port].path, attr);
	fd = open (tpath, O_WRONLY);
	if (fd < 0) {
		fprintf (stderr, "ev3_set_mot_attr_int(): failed to open [%s]: %s\n", tpath, strerror (errno));
		return -1;
	}

	wlen = snprintf (xbuf, 32, "%d\n", val);
	i = write (fd, xbuf, wlen);

	if (i < 0) {
		fprintf (stderr, "ev3_set_mot_attr_int(): failed to write to [%s]: %s\n", tpath, strerror (errno));
		close (fd);
		return -1;
	} else if (i != wlen) {
		fprintf (stderr, "ev3_set_mot_attr_int(): incomplete write to [%s]: %d of %d bytes\n", tpath, i, wlen);
		close (fd);
		return -1;
	}
	/* else good! */

	close (fd);
	return 0;
}
/*}}}*/
/*{{{  static int ev3_get_mot_attr_int (int port, const char *attr, int *valp)*/
/*
 *	reads an integer from a particular file in /sys.
 *	puts the value read into 'valp', returns 0 on success, non-zero on failure.
 */
static int ev3_get_mot_attr_int (int port, const char *attr, int *valp)
{
	char tpath[FILENAME_MAX];
	char xbuf[32];
	int fd, i;

	snprintf (tpath, FILENAME_MAX, "%s/%s", ev3_motmap[port].path, attr);
	fd = open (tpath, O_RDONLY);
	if (fd < 0) {
		fprintf (stderr, "ev3_get_mot_attr_int(): failed to open [%s]: %s\n", tpath, strerror (errno));
		return -1;
	}

	i = read (fd, xbuf, 31);

	if (i < 0) {
		fprintf (stderr, "ev3_get_mot_attr_int(): failed to read from [%s]: %s\n", tpath, strerror (errno));
		close (fd);
		return -1;
	} else {
		xbuf[i] = '\0';		/* add null */
		if (sscanf (xbuf, "%d", valp) != 1) {
			fprintf (stderr, "ev3_get_mot_attr_int(): failed to parse value in [%s]\n", tpath);
			close (fd);
			return -1;
		}
	}
	/* else good! */

	close (fd);
	return 0;
}
/*}}}*/
/*{{{  static int ev3_set_mot_attr_str (int port, const char *attr, const char *val)*/
/*
 *	writes an integer to a particular file in /sys
 *	returns 0 on success, non-zero on failure
 */
static int ev3_set_mot_attr_str (int port, const char *attr, const char *val)
{
	char tpath[FILENAME_MAX];
	int fd, wlen, i;

	snprintf (tpath, FILENAME_MAX, "%s/%s", ev3_motmap[port].path, attr);
	fd = open (tpath, O_WRONLY);
	if (fd < 0) {
		fprintf (stderr, "ev3_set_mot_attr_str(): failed to open [%s]: %s\n", tpath, strerror (errno));
		return -1;
	}

	wlen = strlen (val);
	i = write (fd, val, wlen);

	if (i < 0) {
		fprintf (stderr, "ev3_set_mot_attr_str(): failed to write to [%s]: %s\n", tpath, strerror (errno));
		close (fd);
		return -1;
	} else if (i != wlen) {
		fprintf (stderr, "ev3_set_mot_attr_str(): incomplete write to [%s]: %d of %d bytes\n", tpath, i, wlen);
		close (fd);
		return -1;
	}
	/* else good! */

	close (fd);
	return 0;
}
/*}}}*/


/*{{{  define ev3_mot_init (val ev3_outport port, val ev3_mdev mtype) -> bool*/

static void igcf_ev3_mot_init (int *result, int port, ev3_mdev_e type)
{
	if ((port < 0) || (port > 3)) {
		*result = 0;
		return;
	}

	switch (type) {
	case MDEV_UNKNOWN:
		/*{{{  unsetting motor device*/
		/* trash it */
		ev3_motmap[port].dev = MDEV_UNKNOWN;
		ev3_motmap[port].mid = -1;
		ev3_motmap[port].st = MST_OFF;
		ev3_motmap[port].path[0] = '\0';

		*result = 1;
		break;
		/*}}}*/
	case MDEV_L_MOTOR:
	case MDEV_M_MOTOR:
		/*{{{  setting to EV3/NXT "large" or "medium" motor*/
		{
			/* see if the appropriate motor is there */
			char tpath[FILENAME_MAX];
			DIR *dir;
			struct dirent *dent;
			char mch = (type == MDEV_L_MOTOR) ? 'l' : 'm';

			snprintf (tpath, FILENAME_MAX, SYS_ROOT "/class/lego-port/port%d/out%c:lego-ev3-%c-motor/tacho-motor", (port+4), 'A' + port, mch);
			dir = opendir (tpath);
			if (!dir) {
				goto fail_out;
			}
			/* read out until we see something that starts 'motor' */
			for (;;) {
				dent = readdir (dir);
				if (!dent) {
					closedir (dir);
					goto fail_out;
				}
				if ((dent->d_type == DT_DIR) && !strncmp (dent->d_name, "motor", 5)) {
					/* found it! */
					int mid;

					if (sscanf (dent->d_name + 5, "%d", &mid) != 1) {
						closedir (dir);
						goto fail_out;
					}

					ev3_motmap[port].dev = type;
					ev3_motmap[port].mid = mid;
					ev3_motmap[port].st = MST_OFF;

					snprintf (ev3_motmap[port].path, FILENAME_MAX, "%s/motor%d", tpath, mid);

					break;		/* for() */
				}
			}
			closedir (dir);

			/* if we get here, found it! */
			fprintf (stderr, "ev3:mot_init(): found motor %d on port out%c\n", ev3_motmap[port].mid, 'A' + port);

			*result = 1;
		}
		break;
		/*}}}*/
	default:
		fprintf (stderr, "ev3:mot_init(): invalid type for port out%c (%d)", 'A' + port, type);
		*result = 0;
		break;
	}

	return;
fail_out:
	fprintf (stderr, "ev3:mot_init(): no tacho motor on out%c?", 'A' + port);
	*result = 0;
	return;
}


void gcf_ev3_mot_init (Workspace wptr, int *result, int port, int type)
{
	ExternalCallN (igcf_ev3_mot_init, 3, result, port, (ev3_mdev_e)type);
}

/*}}}*/
/*{{{  define ev3_mot_shutdown () -> bool*/

static void igcf_ev3_mot_shutdown (int *result)
{
	int p;

	for (p=0; p<4; p++) {
		ev3_motmap[p].dev = MDEV_UNKNOWN;
		ev3_motmap[p].mid = -1;
		ev3_motmap[p].st = MST_OFF;
		ev3_motmap[p].path[0] = '\0';
	}
	*result = 1;
	return;
}

void gcf_ev3_mot_shutdown (Workspace wptr, int *result)
{
	ExternalCallN (igcf_ev3_mot_shutdown, 1, result);
}
/*}}}*/
/*{{{  define ev3_mot_on_fwd (val ev3_outport port, val int power) -> bool*/

static void igcf_ev3_mot_on (int *result, int port, int power)
{
	if ((port < 0) || (port > 3)) {
		*result = 0;
		return;
	}
	if ((ev3_motmap[port].dev == MDEV_UNKNOWN) || (ev3_motmap[port].mid < 0)) {
		*result = 0;
		return;
	}

	if (ev3_set_mot_attr_int (port, "duty_cycle_sp", power)) {
		*result = 0;
		return;
	}
	if (ev3_set_mot_attr_str (port, "command", "run-forever\n")) {
		*result = 0;
		return;
	}

	*result = 1;
	return;
}

void gcf_ev3_mot_on_fwd (Workspace wptr, int *result, int port, int power)
{
	ExternalCallN (igcf_ev3_mot_on, 3, result, port, power);
}

/*}}}*/
/*{{{  define ev3_mot_on_rev (val ev3_outport port, val int power) -> bool*/

void gcf_ev3_mot_on_rev (Workspace wptr, int *result, int port, int power)
{
	ExternalCallN (igcf_ev3_mot_on, 3, result, port, -power);
}

/*}}}*/
/*{{{  define ev3_mot_off (val ev3_outport port) -> bool*/

static void igcf_ev3_mot_off (int *result, int port)
{
	if ((port < 0) || (port > 3)) {
		*result = 0;
		return;
	}
	if ((ev3_motmap[port].dev == MDEV_UNKNOWN) || (ev3_motmap[port].mid < 0)) {
		*result = 0;
		return;
	}

	if (ev3_set_mot_attr_str (port, "command", "stop\n")) {
		*result = 0;
		return;
	}

	*result = 1;
	return;
}

void gcf_ev3_mot_off (Workspace wptr, int *result, int port)
{
	ExternalCallN (igcf_ev3_mot_off, 2, result, port);
}

/*}}}*/
/*{{{  define ev3_mot_stop_mode (val ev3_outport port, val ev3_stm smode) -> bool*/

static void igcf_ev3_mot_stop_mode (int *result, int port, ev3_stm_e smode)
{
	static char *modestrs[] = {"coast", "brake", "hold"};

	if ((port < 0) || (port > 3)) {
		*result = 0;
		return;
	}
	if ((ev3_motmap[port].dev == MDEV_UNKNOWN) || (ev3_motmap[port].mid < 0)) {
		*result = 0;
		return;
	}

	if ((smode < STM_COAST) || (smode > STM_HOLD)) {
		*result = 0;
		return;
	}

	if (ev3_set_mot_attr_str (port, "stop_command", modestrs[smode])) {
		*result = 0;
		return;
	}

	*result = 1;
	return;
}

void gcf_ev3_mot_stop_mode (Workspace wptr, int *result, int port, int smode)
{
	ExternalCallN (igcf_ev3_mot_stop_mode, 3, result, port, smode);
}

/*}}}*/
/*{{{  define ev3_mot_count_per_rot (val ev3_outport port) -> int*/

static void igcf_ev3_mot_count_per_rot (int *result, int port)
{
	if ((port < 0) || (port > 3)) {
		*result = 0;
		return;
	}
	if ((ev3_motmap[port].dev == MDEV_UNKNOWN) || (ev3_motmap[port].mid < 0)) {
		*result = 0;
		return;
	}

	if (ev3_get_mot_attr_int (port, "count_per_rot", result)) {
		*result = 0;
		return;
	}
	return;
}

void gcf_ev3_mot_count_per_rot (Workspace wptr, int *result, int port)
{
	ExternalCallN (igcf_ev3_mot_count_per_rot, 2, result, port);
}

/*}}}*/
/*{{{  define ev3_mot_cur_pos (val ev3_outport port) -> int*/

static void igcf_ev3_mot_cur_pos (int *result, int port)
{
	if ((port < 0) || (port > 3)) {
		*result = 0;
		return;
	}
	if ((ev3_motmap[port].dev == MDEV_UNKNOWN) || (ev3_motmap[port].mid < 0)) {
		*result = 0;
		return;
	}

	if (ev3_get_mot_attr_int (port, "position", result)) {
		*result = 0;
		return;
	}
	return;
}

void gcf_ev3_mot_cur_pos (Workspace wptr, int *result, int port)
{
	ExternalCallN (igcf_ev3_mot_cur_pos, 2, result, port);
}

/*}}}*/
/*{{{  define ev3_mot_run_to_pos (val ev3_outport port, val int pos, power) -> bool*/

static void igcf_ev3_mot_run_to_pos (int *result, int port, int pos, int power)
{
	if ((port < 0) || (port > 3)) {
		*result = 0;
		return;
	}
	if ((ev3_motmap[port].dev == MDEV_UNKNOWN) || (ev3_motmap[port].mid < 0)) {
		*result = 0;
		return;
	}

	if (ev3_set_mot_attr_int (port, "duty_cycle_sp", power)) {
		*result = 0;
		return;
	}
	if (ev3_set_mot_attr_int (port, "position_sp", pos)) {
		*result = 0;
		return;
	}
	if (ev3_set_mot_attr_str (port, "command", "run-to-abs-pos\n")) {
		*result = 0;
		return;
	}

	*result = 1;
	return;
}

void gcf_ev3_mot_run_to_pos (Workspace wptr, int *result, int port, int pos, int power)
{
	ExternalCallN (igcf_ev3_mot_run_to_pos, 4, result, port, pos, power);
}

/*}}}*/


