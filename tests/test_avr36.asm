; test_avr36.asm -- testing something particular

.mcu "atmega2560"

.data
.org 0x400

V_foo:	.space 2

.text
.org 0

PROG_START:
	clr	r16
	sts	V_foo + 0, r16
	sts	V_foo + 1, r16

