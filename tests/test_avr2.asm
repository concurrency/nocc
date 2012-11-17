;
;	test_avr2.asm -- AVR assembler test
;

.include "test_avr3.asm"

.macro LED_ON
	sbi	PORTD, 7
.endmacro

.equ	PORTD	=2

.eeprom
.const	0x01, 0x04, "AVR Assembler Test", 0x00

.text
.org 0
	rjmp	foo
	nop					; do nothing :)
	nop
	nop

foo:
	ldi	r16, 0xff
loop:
	dec	r16
	brne	loop

	sbi	PORTD, 7


