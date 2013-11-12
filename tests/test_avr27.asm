;	test_avr27.asm -- dimming light for the Arduino Uno
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

	clr	r20			; brightness
	clr	r21			; accumulated value
	ldi	r22, 80			; limit (for the particular LED)
loop:


	ldi	r16, 250
.L0:
	ldi	r17, 250
.L1:
	ldi	r18, 5
.L2:
	dec	r18
	brne	2b			; inner loop

	; add brightness to accumulated value, use carry to turn LED on/off
	add	r21, r20
	brcc	3f
	sbi	PORTB, 5		; carry set, so LED on
	rjmp	4f
.L3:
	cbi	PORTB, 5		; carry clear, so LED off
.L4:

	dec	r17
	brne	1b			; middle loop
	dec	r16
	brne	0b			; outer loop

	inc	r20
	cp	r20, r22
	brlo	4f			; branch if lower
	clr	r20			; overflowed, reset to zero
.L4:
	rjmp	loop
	
