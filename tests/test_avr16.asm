;
;	test_avr16.asm -- testing undefined labels
;

.mcu	"ATMEGA328"

.text
.org	0
	rjmp reset
	rjmp int0vec ? : skipvec

.org	0x100

skipvec:
	reti

int0vec:
	reti

reset:
	nop
	nop
	nop
	rjmp	reset

