;
;	wibble.asm
;	simple test for the AVR
;

.equ	SREG	=0x3f
.equ	SPH	=0x3e
.equ	SPL	=0x3d

.equ	PORTC	=0x15
.equ	DDRC	=0x14
.equ	PINC	=0x13

.org	0
	rjmp	reset
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

reset:
	ldi	r16,0x3f		; PC0->PC5 are output LEDs
	out	DDRC,r16
	ldi	r16,0x43		; light up the 2 reds
	out	PORTC,r16

	; initialise stack pointer.. :)
	ldi	r16,0x00
	out	SPH,r16
	ldi	r16,0xf0
	out	SPL,r16

	ldi	r16,0x01
loop:
	rjmp	wait_change
wait_change_back:
	dec	r16
	breq	sig1
	dec	r16
	breq	sig2
	dec	r16
	breq	sig3
	dec	r16
	breq	sig4
	dec	r16
	breq	sig5
	ldi	r16,0x60
	out	PORTC,r16
	ldi	r16,0x01
	rjmp	loop
sig1:
	ldi	r16,0x50
	out	PORTC,r16
	ldi	r16,0x02
	rjmp	loop
sig2:
	ldi	r16,0x48
	out	PORTC,r16
	ldi	r16,0x03
	rjmp	loop
sig3:
	ldi	r16,0x44
	out	PORTC,r16
	ldi	r16,0x04
	rjmp	loop
sig4:
	ldi	r16,0x42
	out	PORTC,r16
	ldi	r16,0x05
	rjmp	loop
sig5:
	ldi	r16,0x41
	out	PORTC,r16
	ldi	r16,0x06
	rjmp	loop

wait_change:
	in	r17,PORTC
	sbrs	r17,6
	rjmp	wait_change
wait_change2:
	in	r17,PORTC
	sbrc	r17,6
	rjmp	wait_change2
	nop
	rjmp	wait_change_back
	nop
	nop

