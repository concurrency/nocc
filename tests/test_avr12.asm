;
;	test_avr12.asm -- proper test for ATMEGA1280
;

.mcu	"ATMEGA1280"


; include files part of the compiler infrastructure
.include "atmega1280.inc"

.text
.include "atmega1280-imap.inc"


; expects "intensity" in r2, running value in r3
add_and_light:
	add	r3, r2
	brcc	aal_1
	sbi	PORTB, 7
	sbi	PORTB, 6
	rjmp	aal_2
aal_1:
	cbi	PORTB, 7
	cbi	PORTB, 6
aal_2:
	ret

; runs the specified brightness (in r2) for a fixed amount of time
delayloop:
	push	r18
	push	r17
	push	r16

	ldi	r16, 0x2
dloop_0:
	ldi	r17, 0x7f
dloop_1:
	rcall	add_and_light
	ldi	r18, 0x1f
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

	ldi	r16, 0xc0
	out	DDRB, r16

	cbi	PORTB, 7
	cbi	PORTB, 6
	clr	r3
	clr	r2

	ldi	r16, 0xff
loop:
	call	delayloop		; will do lights as appropriate
	inc	r2
	brne	loop			; until r2 overflows to zero, loop
	dec	r2
loop2:
	call	delayloop
	dec	r2
	breq	loop
	rjmp	loop2

