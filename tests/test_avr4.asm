;
;	test_avr4.asm -- AVR assembler test
;

.equ	FOO	=2

.macro	BAR
	sbi	FOO, 7
.endmacro

.org	0
	rjmp	reset

reset:
	nop
	BAR

