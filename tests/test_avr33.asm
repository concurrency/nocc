;
;	test_avr33.asm -- testing for the ATMEGA2560
;

.mcu	"atmega2560"

.include "atmega1280.inc"

; own constants
.equ	DWIDTH	=128
.equ	DHEIGHT	=96

.data
.org	0x400
V_framebuffer:
	.space	((DWIDTH >> 3) * DHEIGHT)

.text
.include "atmega1280-imap.inc"


VEC_reset:
	cli				; disable interrupts
	ldi	r16, hi(RAMEND)		; stack-pointer SPH:SPL = RAMEND
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei				; enable interrupts

loop:
	sleep
	rjmp	loop
