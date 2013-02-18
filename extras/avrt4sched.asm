;
;	avrt4sched.asm -- AVR T414+ scheduler (of sorts)
;	Copyright (C) 2013 Fred Barnes, University of Kent <frmb@kent.ac.uk>
;

.mcu "atmega328"
.include "atmega328.inc"
.include "atmega328-imap.inc"

.data

; This follows a fairly standard 16-bit Transputer memory layout:
;
;				|---------------|         |
;				|		|         |
;				| .. params ..  |         | (parent frame)
;				|		|         |
;				|---------------|         |
;		+ FS + 1	| Ret/Temp H	|         |
; call-in-Wptr	    ---------->	| Ret/Temp L	|   -.    |
;				|---------------|    |   -'
;				|		|    |
;				| .. locals ..  |    | (frame space for locals)
;				|		|    |
;				|---------------|    |
; 		+1		| Temp H	|    |
; Wptr (r29:r28 = Y) --------->	| Temp L	|   -'
;				|---------------|
;		-1		| Iptr H	|
;		-2		| Iptr L	|
;				|---------------|
;		-3		| Link H	|
;		-4		| Link L	|
;				|---------------|
;		-5		| Pointer H	|
;		-6		| Pointer L	|
;				|---------------|
;		-7		| Time H	|
;		-8		| Time L	|
;				|---------------|
;		-9		| TLink H	|
;		-10		| TLink L	|
;				|---------------|
;

V_t4_fptr:
	.space	2
V_t4_bptr:
	.space	2
V_t4_tptr:
	.space	2
V_t4_wptr:
	.space	2		; usually held in the 'Y' register


.text

.function t4_sched_init () ;{{{  Initialises the scheduler.
	push	XH
	push	XL
	push	r16

	ldi	XH:XL, V_t4_fptr
	clr	r16
	st	X+, r16
	st	X+, r16		; clear fptr
	st	X+, r16
	st	X+, r16		; clear bptr
	st	X+, r16
	st	X+, r16		; clear tptr
	st	X+, r16
	st	X, r16		; clear wptr

	pop	r16
	pop	XL
	pop	XH
.endfunction


;}}}
.function t4_sched_startp () ;{{{  Sets up a new process at X, code at address Z
	nop
.endfunction


;}}}
.function t4_sched_runp () ;{{{  Enqueues the process at X
	push	ZH
	push	ZL
	push	YH
	push	YL
	push	r16
	push	r17

	mov	YH, XH
	mov	YL, XL
	sbiw	Y, 4		; so Y is pointing at process's Link field
	ldi	ZH:ZL, V_t4_fptr	; Z points at fptr
	ld	r16, Z+1
	tst	r16
	breq	0f

	; fptr non-null, insert into bptr
	ldi	ZH:ZL, V_t4_bptr
	ld	r16, Z+0
	ld	r17, Z+1	; load bptr into r17:r16
	push	ZH
	push	ZL
	mov	ZH, r17
	mov	ZL, r16
	subiw	Z, 4		; so Z is pointing at bptr[Link]
	st	Z+0, XL
	st	Z+1, XH		; put process in bptr[Link]
	pop	ZL
	pop	ZH
	st	Z+0, XL
	st	Z+1, XH		; put process in bptr
	rjmp	9f

.L0:
	; fptr is null, whole queue please
	st	Z+, XL
	st	Z+, XH		; fptr = process
	st	Z+, XL
	st	Z+, XH		; bptr = process
	rjmp	9f

.L9:
	; y is still pointing at the process's Link field, clear it
	clr	r16
	st	Y+, r16
	st	Y, r16

	pop	r17
	pop	r16
	pop	YL
	pop	YH
	pop	ZL
	pop	ZH
.endfunction


;}}}


