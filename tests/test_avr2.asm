;
;	test_avr2.asm -- AVR assembler test
;

.equ	PORTD	=2

.org 0
	rjmp	foo
	nop
	nop
	nop

foo:
	ldi	r16, 0xff
loop:
	dec	r16
	brne	loop

	sbi	PORTD, 7


