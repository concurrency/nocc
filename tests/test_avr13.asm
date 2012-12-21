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
	; Note: don't need to shift these, automatically gets byte address
	ldi	r17, hi(ctable)
	ldi	r16, lo(ctable)

ctable:
.const	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07

.eeprom
.org 0
.const16	1b, 0b
	

