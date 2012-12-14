;
;	test_avr13.asm -- testing local labels
;

.mcu	"ATMEGA1280"

.text
.org	0
	jmp	reset

.org	0x100
reset:
	nop
	nop
.L0:
	sleep
	rjmp	reset
