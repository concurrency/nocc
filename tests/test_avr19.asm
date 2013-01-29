;
;	test_avr19.asm -- proper test for ATMEGA328 (Arduino Duemilanove)
;

.mcu	"ATMEGA328"

; include files part of the compiler infrastructure
.include "atmega328.inc"

.text
.include "atmega328-imap.inc"

.include "dfrobot-arduino-ether.inc"

.function delayloop ()				;/*{{{*/
	push	r18
	push	r17
	push	r16

	ldi	r16, 0x10
.L0:
	ldi	r17, 0xff
.L1:
	ldi	r18, 0xff
.L2:
	nop
	nop
	nop
	nop

	dec	r18
	brne	2b

	dec	r17
	brne	1b

	dec	r16
	brne	0b

	pop	r16
	pop	r17
	pop	r18
.endfunction					;/*}}}*/


VEC_reset:
	cli
	ldi	r16, hi(RAMEND)
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei

	call	dfr_init

	call	delayloop

	ldi	r27, hi(W5100_MR)
	ldi	r26, lo(W5100_MR)
	ldi	r16, 0x80			; s/w reset
	call	dfr_ether_write

	call	delayloop
	call	delayloop

	ldi	r27, hi(W5100_GAR0)
	ldi	r26, lo(W5100_GAR0)
	ldi	r16, 192
	call	dfr_ether_write
	ldi	r27, hi(W5100_GAR1)
	ldi	r26, lo(W5100_GAR1)
	ldi	r16, 168
	call	dfr_ether_write
	ldi	r27, hi(W5100_GAR2)
	ldi	r26, lo(W5100_GAR2)
	ldi	r16, 12
	call	dfr_ether_write
	ldi	r27, hi(W5100_GAR3)
	ldi	r26, lo(W5100_GAR3)
	ldi	r16, 2
	call	dfr_ether_write

	call	delayloop

	ldi	r27, hi(W5100_SUBR0)
	ldi	r26, lo(W5100_SUBR0)
	ldi	r16, 255
	call	dfr_ether_write
	ldi	r27, hi(W5100_SUBR1)
	ldi	r26, lo(W5100_SUBR1)
	ldi	r16, 255
	call	dfr_ether_write
	ldi	r27, hi(W5100_SUBR2)
	ldi	r26, lo(W5100_SUBR2)
	ldi	r16, 255
	call	dfr_ether_write
	ldi	r27, hi(W5100_SUBR3)
	ldi	r26, lo(W5100_SUBR3)
	ldi	r16, 0
	call	dfr_ether_write

	call	delayloop

	ldi	r27, hi(W5100_SHAR0)
	ldi	r26, lo(W5100_SHAR0)
	ldi	r16, 0x12
	call	dfr_ether_write
	ldi	r27, hi(W5100_SHAR1)
	ldi	r26, lo(W5100_SHAR1)
	ldi	r16, 0x21
	call	dfr_ether_write
	ldi	r27, hi(W5100_SHAR2)
	ldi	r26, lo(W5100_SHAR2)
	ldi	r16, 0x56
	call	dfr_ether_write
	ldi	r27, hi(W5100_SHAR3)
	ldi	r26, lo(W5100_SHAR3)
	ldi	r16, 0x65
	call	dfr_ether_write
	ldi	r27, hi(W5100_SHAR4)
	ldi	r26, lo(W5100_SHAR4)
	ldi	r16, 0x01
	call	dfr_ether_write
	ldi	r27, hi(W5100_SHAR5)
	ldi	r26, lo(W5100_SHAR5)
	ldi	r16, 0x10
	call	dfr_ether_write

	call	delayloop

	ldi	r27, hi(W5100_SIPR0)
	ldi	r26, lo(W5100_SIPR0)
	ldi	r16, 192
	call	dfr_ether_write
	ldi	r27, hi(W5100_SIPR1)
	ldi	r26, lo(W5100_SIPR1)
	ldi	r16, 168
	call	dfr_ether_write
	ldi	r27, hi(W5100_SIPR2)
	ldi	r26, lo(W5100_SIPR2)
	ldi	r16, 12
	call	dfr_ether_write
	ldi	r27, hi(W5100_SIPR3)
	ldi	r26, lo(W5100_SIPR3)
	ldi	r16, 222
	call	dfr_ether_write

	call	delayloop

.L0:
	rjmp	0b
