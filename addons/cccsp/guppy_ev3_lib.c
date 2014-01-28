/*
 *	guppy_ev3_lib.c -- routines for Guppy on the LEGO EV3
 *	Copyright (C) 2014 Fred Barnes, University of Kent <frmb@kent.ac.uk>
 */


#include <cccsp/verb-header.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static int ev3_pwm_fd = -1;


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

