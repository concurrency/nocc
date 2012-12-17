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
	rjmp	1f
.L0:
	sleep
	rjmp	reset
	rjmp	0b
.L1:	wdr

.eeprom
.org 0
.const16	1b, 0b
	

