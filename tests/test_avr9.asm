;
;	test_avr9.asm -- testing hi/lo
;

.mcu	"ATMEGA1280"

.equ	PORTB	=0x05
.equ	DDRB	=0x04
.equ	PINB	=0x03

.equ	SREG	=0x3f
.equ	SPH	=0x3e
.equ	SPL	=0x3d
.equ	RAMEND	=0x1fff

.text
.org	0
	rjmp	reset

.org	0x100
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

	rjmp	loop

