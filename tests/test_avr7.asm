;
;	test_avr7.asm -- more assembler tests (expressions)
;

.mcu	"ATMEGA1280"

.equ	VAL	=0x42

.data
.org	0
stuff:
.org	2
next:

.text
.org	0

	rjmp	reset
	nop
	nop
	nop

reset:
	ldi	r16, VAL + 2
	sleep
	rjmp	reset
