;
;	test_avr5.asm -- AVR assembler tests
;

.equ	PORTD	=2

.macro	BIT_ON (Port,Bit)
	sbi	Port,Bit
.endmacro

.macro	BLIND
	nop
	nop
.endmacro

.text
.org	0
	rjmp	reset
	nop
	nop
	nop
	BLIND

reset:
	BIT_ON (PORTD, 7)

