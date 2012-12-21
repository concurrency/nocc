;
;	test_avr17.asm
;	experimenting with higher-level things..
;

.mcu	"ATMEGA1280"

.include "atmega1280.inc"
.include "atmega1280-imap.inc"

.function testfcn (Wptr = YH:YL, c = r16)
	.let tmp = r3:r2

	pop	tmp
	dstore	Wptr + 0, tmp		; take return-address and put in Wptr[0]

	; FIXME: do things

	dload	tmp, Wptr + 0		; load return-address from Wptr[0]
	push	tmp
.endfunction

VEC_reset:
	nop
	sleep
	rjmp	VEC_reset

