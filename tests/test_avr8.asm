;
;	test_avr8.asm -- proper test for ATMEGA1280
;

.mcu	"ATMEGA1280"


; include files part of the compiler infrastructure
.include "atmega1280.inc"

.text
.include "atmega1280-imap.inc"


delayloop:
	push	r18
	push	r17
	push	r16

	ldi	r16, 0x7
dloop_0:
	ldi	r17, 0xff
dloop_1:
	ldi	r18, 0xff
dloop_2:
	nop
	nop
	nop
	nop

	dec	r18
	brne	dloop_2

	dec	r17
	brne	dloop_1

	dec	r16
	brne	dloop_0

	pop	r16
	pop	r17
	pop	r18
	ret

; start here

reset:
	cli
	ldi	r16, hi(RAMEND)
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei

	ldi	r16, 0x80
	out	DDRB, r16

loop:
	sbi	PORTB, 7
	call	delayloop
	cbi	PORTB, 7
	call	delayloop
	rjmp	loop

