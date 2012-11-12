;
;	test_avr2.asm -- AVR assembler test
;

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


