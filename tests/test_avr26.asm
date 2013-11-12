;	test_avr26.asm -- blinkenlight for the Arduino Uno
;	Fred Barnes, October 2013

.mcu	"atmega328"

.include "atmega328.inc"

.text
.include "atmega328-imap.inc"


VEC_reset:
	; setup stack-pointer
	ldi	r16, hi(RAMEND)
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei

	sbi	DDRB, 5			; PB5 output
	cbi	PORTB, 5		; PB5 low (LED off)

loop:

	ldi	r16, 250
.L0:
	ldi	r17, 250
.L1:
	ldi	r18, 20
.L2:
	dec	r18
	brne	2b			; inner loop
	dec	r17
	brne	1b			; middle loop
	dec	r16
	brne	0b			; outer loop

	sbic	PORTB, 5		; skip if PB5 low
	rjmp	3f
	; if here, means LED was off, so turn on
	sbi	PORTB, 5
	rjmp	loop
.L3:
	; if here, means LED was on, so turn off
	cbi	PORTB, 5
	rjmp	loop
	
