;
;	test_avr11.asm -- testing transputing thoughts
;

.mcu	"ATMEGA1280"

.include "atmega1280.inc"

.data				; === DATA SEGMENT ===
.org	RAMSTART
V_fptr:	.space 2		; front queue pointer
V_bptr:	.space 2		; back queue pointer
V_tptr: .space 2		; timer queue pointer

.text				; === CODE SEGMENT ===
.org	0
	jmp	reset
				; space for vector table (all NOP)
.org	0x100


sched_init:			;{{{  SUB: scheduler initialisation
	push	r0
	clr	r0

	; clear front-pointer
	ldi	ZH, hi(V_fptr)
	ldi	ZL, lo(V_fptr)
	st	Z+0, r0
	st	Z+1, r0

	; clear back-pointer
	ldi	ZH, hi(V_bptr)
	ldi	ZL, lo(V_bptr)
	st	Z+0, r0
	st	Z+1, r0

	; clear timer-pointer
	ldi	ZH, hi(V_tptr)
	ldi	ZL, lo(V_tptr)
	st	Z+0, r0
	st	Z+1, r0

	pop	r0
	ret
;}}}
sched_enqueue:			;{{{  SUB: add process in r25:r24 to the run-queue
	push	r0

	; load fptr into X
	ldi	ZH, hi(V_fptr)
	ldi	ZL, lo(V_fptr)
	ld	XH, Z+0
	ld	XL, Z+1

	mov	r0, XH
	or	r0, XL
	breq	sen_1

	; queue not empty
	nop

	rjmp	sen_2

sen_1:	; run-queue empty
	nop

sen_2:

	pop	r0
	ret
;}}}

reset:				;{{{  reset entry-point
	cli

	; first, setup stack pointer
	ldi	r16, hi(RAMEND)
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16

	sei			; enable interrupts

	call	sched_init


dieloop:
	sleep
	rjmp	dieloop
;}}}

