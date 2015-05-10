/*
 *	guppy_ev3_lib.c -- routines for Guppy on the LEGO EV3
 *	Copyright (C) 2014-2015 Fred Barnes, University of Kent <frmb@kent.ac.uk>
 */


#include <cccsp/verb-header.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
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
	MDEV_L_MOTOR	= 1
} ev3_mdev_e;

typedef enum ENUM_ev3_mst {
	MST_OFF		= 0,
	MST_FWD		= 1,
	MST_REF		= 2
} ev3_mst_e;

/* motor mapping for EV3 (by port 0-3 == A-D) */
typedef struct TAG_ev3_motmap {
	ev3_mdev_e dev;		/* device motor type */
	int mid;		/* motor ID (assigned by kernel) */
	ev3_mst_e st;		/* motor state */
} ev3_motmap_t;

static ev3_motmap_t ev3_motmap[4] = {
		{MDEV_UNKNOWN, -1, MST_OFF},
		{MDEV_UNKNOWN, -1, MST_OFF},
		{MDEV_UNKNOWN, -1, MST_OFF},
		{MDEV_UNKNOWN, -1, MST_OFF}
	};

/*{{{  define ev3_mot_init (val int port, type) -> bool*/

static void igcf_ev3_mot_init (int *result, int port, int type)
{
	if ((port < 0) || (port > 3)) {
		*result = 0;
		return;
	}

	if (type == MDEV_UNKNOWN) {
		/* trash it */
		ev3_motmap[port].dev = MDEV_UNKNOWN;
		ev3_motmap[port].mid = -1;
		ev3_motmap[port].st = MST_OFF;

		*result = 1;
	} else if (type == MDEV_L_MOTOR) {
		/* see if the appropriate motor is there */
		char tpath[PATH_MAX];
		DIR *dir;
		struct dirent *dent;

		snprintf (tpath, PATH_MAX, SYS_ROOT "/class/lego-port/port%d/out%c:lego-ev3-l-motor/tacho-motor", (port+4), 'A' + port);
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

				ev3_motmap[port].dev = MDEV_L_MOTOR;
				ev3_motmap[port].mid = mid;
				ev3_motmap[port].st = MST_OFF;
				break;		/* for() */
			}
		}
		closedir (dir);

		/* if we get here, found it! */
		fprintf (stderr, "ev3:mot_init(): found motor %d on port out%c\n", ev3_motmap[port].mid, 'A' + port);

		*result = 1;
	} else {
		fprintf (stderr, "ev3:mot_init(): invalid type for port out%c (%d)", 'A' + port, type);
		*result = 0;
	}

	return;
fail_out:
	fprintf (stderr, "ev3:mot_init(): no tacho motor on out%c?", 'A' + port);
	*result = 0;
	return;
}


void gcf_ev3_mot_init (Workspace wptr, int *result, int port, int type)
{
	ExternalCallN (igcf_ev3_mot_init, 3, result, port, type);
}

/*}}}*/


#if 0

static void i_dowrite (const char *buf, const int len)
{
	int i;

	i = write (ev3_pwm_fd, buf, len);
	if (i < 0) {
		fprintf (stderr, "ev3:i_dowrite(): failed to write to device, error %d\n", errno);
	}
}


/*{{{  define ev3_pwm_init () -> bool*/
/*
 *	initialises the PWM stuff, returns true/false.
 */

static void igcf_ev3_pwm_init (int *result)
{
	if (ev3_pwm_fd >= 0) {
		/* already initialised */
		*result = 0;
	}

	ev3_pwm_fd = open ("/dev/ev3dev_pwm", O_WRONLY);
	if (ev3_pwm_fd < 0) {
		fprintf (stderr, "ev3:pwm_init(): failed to open device, error %d\n", errno);
		*result = 0;
		return;
	}

	/* send the "program start" message */
	i_dowrite ("\x03", 1);
	*result = 1;
}

void gcf_ev3_pwm_init (Workspace wptr, int *result)
{
	ExternalCallN (igcf_ev3_pwm_init, 1, result);
}
/*}}}*/
/*{{{  define ev3_pwm_shutdown ()*/
/*
 *	shuts-down PWM related things.
 */

static void igcf_ev3_pwm_shutdown (void)
{
	if (ev3_pwm_fd < 0) {
		return;
	}
	/* send the "program stop" message */
	i_dowrite ("\x02", 1);

	close (ev3_pwm_fd);
	ev3_pwm_fd = -1;
	return;
}

void gcf_ev3_pwm_shutdown (Workspace wptr)
{
	ExternalCallN (igcf_ev3_pwm_shutdown, 0);
}
/*}}}*/
/*{{{  define ev3_pwm_on_fwd (val int motor, val int power)*/
/*
 *	runs a motor forwards at the specified power-level (0-100).
 */

static void igcf_ev3_pwm_on_fwd (int motor, int power)
{
	unsigned char buf[3];

	buf[0] = 0xa4;
	buf[1] = (unsigned char)(motor & 0xff);
	buf[2] = (unsigned char)(power & 0xff);

	i_dowrite ((char *)buf, 3);

	buf[0] = 0xa7;
	buf[1] = (unsigned char)(motor & 0xff);
	buf[2] = 0x01;

	i_dowrite ((char *)buf, 3);

	buf[0] = 0xa6;
	buf[1] = (unsigned char)(motor & 0xff);

	i_dowrite ((char *)buf, 2);
}

void gcf_ev3_pwm_on_fwd (Workspace wptr, int motor, int power)
{
	ExternalCallN (igcf_ev3_pwm_on_fwd, 2, motor, power);
}
/*}}}*/
/*{{{  define ev3_pwm_on_rev (val int motor, val int power)*/
/*
 *	runs a motor in reverse at the specified power-level (0-100).
 */

static void igcf_ev3_pwm_on_rev (int motor, int power)
{
	unsigned char buf[3];

	buf[0] = 0xa4;
	buf[1] = (unsigned char)(motor & 0xff);
	buf[2] = (unsigned char)(power & 0xff);

	i_dowrite ((char *)buf, 3);

	buf[0] = 0xa7;
	buf[1] = (unsigned char)(motor & 0xff);
	buf[2] = 0xff;

	i_dowrite ((char *)buf, 3);

	buf[0] = 0xa6;
	buf[1] = (unsigned char)(motor & 0xff);

	i_dowrite ((char *)buf, 2);
}

void gcf_ev3_pwm_on_rev (Workspace wptr, int motor, int power)
{
	ExternalCallN (igcf_ev3_pwm_on_rev, 2, motor, power);
}
/*}}}*/
/*{{{  define ev3_pwm_toggle_dir (val int motor)*/
/*
 *	toggles the direction of the given motor.
 */

static void igcf_ev3_pwm_toggle_dir (int motor)
{
	unsigned char buf[3];

	buf[0] = 0xa7;
	buf[1] = (unsigned char)(motor & 0xff);
	buf[2] = 0x00;

	i_dowrite ((char *)buf, 3);
}

void gcf_ev3_pwm_toggle_dir (Workspace wptr, int motor)
{
	ExternalCallN (igcf_ev3_pwm_toggle_dir, 1, motor);
}

/*}}}*/
/*{{{  define ev3_pwm_off (val int motor)*/
/*
 *	turns off a motor (and brake).
 */

static void igcf_ev3_pwm_off (int motor)
{
	unsigned char buf[2];

	buf[0] = 0xa3;
	buf[1] = (unsigned char)(motor & 0xff);
	buf[2] = 0x01;					/* brake */

	i_dowrite ((char *)buf, 3);
}

void gcf_ev3_pwm_off (Workspace wptr, int motor)
{
	ExternalCallN (igcf_ev3_pwm_off, 1, motor);
}
/*}}}*/

#endif

