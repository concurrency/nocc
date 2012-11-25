;
;	test_avr6.asm -- more AVR assembler tests
;

.equ	DDRD	=2
.equ	PORTD	=3

.org 0
	rjmp	reset
	nop
	nop
	nop

reset:
	ldi	r16, 0xff
	out	DDRD, r16
	out	PORTD, r16

	nop
	sleep
	rjmp reset

