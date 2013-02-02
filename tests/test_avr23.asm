;
;	test_avr23.asm -- encoding
;

.text
.org	0

reset:
	ldi	r29, hi(lab_zog >> 1)
	ldi	r28, lo(lab_zog >> 1)
	nop

	jmp	reset

.function zog ()
lab_zog:
	nop
.endfunction

