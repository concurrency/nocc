;
;	test_avr20.asm -- testing includes and mixed segments (seems fragile)
;

.mcu "ATMEGA328"
.include "atmega328.inc"
.include "atmega328-imap.inc"

.data
.org	0
V_something:
	.space 2

.text

VEC_reset:
	rjmp	VEC_reset

