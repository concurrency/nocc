; test_avr24.asm -- testing T4 scheduler for AVR/Arduino

.mcu "ATMEGA328"
.include "atmega328.inc"

.text
.include "atmega328-imap.inc"

.data
.org	RAMSTART

.text
.include "../extras/avrt4sched.asm"

.data
V_ws:
	.space	512			; 256 words to start with
V_wstop:

.text

VEC_reset:
	ldi	r16, hi(RAMEND)
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei

	call	t4_sched_init
	ldi	YH:YL, V_wstop		; Y points at top of workspace (invalid)
	sbiw	Y, 2

	mov	XH, YH
	mov	XL, YL			; X = workspace for 'PROC_test'
	ldi	ZH:ZL, PROC_test	; Z = addr of 'PROC_test'
	call	t4_sched_startp

loop:
	nop
	nop
	sleep
	rjmp	loop


PROC_test:




