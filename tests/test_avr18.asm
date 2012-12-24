;
;	test_avr18.asm -- testing small things
;

.mcu "atmega1280"

.text
.org	0

reset:
	clr	r16
	clr	r17
	call	thing
	sleep
	rjmp	reset

.function thing (unsigned A = r16, unsigned B = r17)
	.let C = r1
	.let D = r2

	.if (A == B)
		nop
	.else
		nop
	.endif

	; .eval C = A

.endfunction



