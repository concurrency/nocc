; test_avr34.asm -- testing for external signal via transistor gate (latch)
; Fred Barnes, July 2014

.mcu "atmega328"

.include "atmega328.inc"

.text
.include "atmega328-imap.inc"

VEC_reset:
	sbi	DDRD, 2			; PB2:3 output
	sbi	DDRD, 3

	cbi	PORTD, 2
	cbi	PORTD, 3

	; start outer loop: toggle PORTD 2
.L1:
	sbic	PORTD, 2
	rjmp	2f
	sbi	PORTD, 2
	rjmp	3f
.L2:
	cbi	PORTD, 2
.L3:

	; unwound inner loop
	nop
	sbi	PORTD, 3
	nop
	nop
	nop
	nop
	nop
	nop
	cbi	PORTD, 3
	nop
	nop
	nop
	nop
	nop
	nop
	sbi	PORTD, 3
	nop
	nop
	nop
	nop
	nop
	nop
	cbi	PORTD, 3
	nop
	nop
	nop
	nop
	nop
	nop
	sbi	PORTD, 3
	nop
	nop
	nop
	nop
	nop
	nop
	cbi	PORTD, 3
	nop
	nop
	nop
	nop
	nop
	nop
	sbi	PORTD, 3
	nop
	nop
	nop
	nop
	nop
	nop
	cbi	PORTD, 3

	rjmp	1b

