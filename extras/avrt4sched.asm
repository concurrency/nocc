;
;	avrt4sched.asm -- AVR T414+ scheduler (of sorts)
;	Copyright (C) 2013 Fred Barnes, University of Kent <frmb@kent.ac.uk>
;

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

.data

V_t4_fptr:
	.space	2
V_t4_bptr:
	.space	2
V_t4_tptr:
	.space	2
V_t4_wptr:
	.space	2		; usually held in the 'Y' register


.text

;{{{  constants in text
MSG_deadlocked:
	.const	8, "deadlock", 0x00

MC_table_alpha:
	.const	(2 << 5) | 01b,		; A = .-
		(4 << 5) | 1000b	; B = -...
	.const	(4 << 5) | 1010b,	; C = -.-.
		(3 << 5) | 100b		; D = _..
	.const	(1 << 5) | 00b,		; E = .
		(4 << 5) | 0010b	; F = ..-.
	.const	(3 << 5) | 110b,	; G = --.
		(4 << 5) | 0000b	; H = ....
	.const	(2 << 5) | 00b,		; I = ..
		(4 << 5) | 0111b	; J = .---
	.const	(3 << 5) | 101b,	; K = -.-
		(4 << 5) | 0100b	; L = .-..
	.const	(2 << 5) | 11b,		; M = --
		(2 << 5) | 10b		; N = -.
	.const	(3 << 5) | 111b,	; O = ---
		(4 << 5) | 0110b	; P = .--.
	.const	(4 << 5) | 1101b,	; Q = --.-
		(3 << 5) | 010b		; R = .-.
	.const	(3 << 5) | 000b,	; S = ...
		(1 << 5) | 01b		; T = -
	.const	(3 << 5) | 001b,	; U = ..-
		(4 << 5) | 0001b	; V = ...-
	.const	(3 << 5) | 011b,	; W = .--
		(4 << 5) | 1001b	; X = -..-
	.const	(4 << 5) | 1011b,	; Y = -.--
		(4 << 5) | 1100b	; Z = --..

MC_table_num:
	.const	(5 << 5) | 11111b,	; 0 = -----
		(5 << 5) | 01111b	; 1 = .----
	.const	(5 << 5) | 00111b,	; 2 = ..---
		(5 << 5) | 00011b	; 3 = ...--
	.const	(5 << 5) | 00001b,	; 4 = ....-
		(5 << 5) | 00000b	; 5 = .....
	.const	(5 << 5) | 10000b,	; 6 = -....
		(5 << 5) | 11000b	; 7 = --...
	.const	(5 << 5) | 11100b,	; 8 = ---..
		(5 << 5) | 11110b	; 9 = ----.

;}}}


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
	sbiw	XH:XL, 2	; point X at proc[Iptr]
	st	X+, ZL
	st	X+, ZH		; proc[Iptr] = addr;  restore X
	jmp	t4_runp_direct	; add to run-queue
.endfunction


;}}}
.function t4_sched_runp () ;{{{  Enqueues the process at X
t4_runp_direct:			; jump here to enqueue process in X (Y must be a valid Wptr)
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
	sbiw	Z, 4		; so Z is pointing at bptr[Link]
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

.function t4_out () ;{{{  output on channel: Y=wptr, Z=chan-addr, r17:r16=data-addr, r18=count
	; inspect channel high-word
	ld	XH, Z+1
	tst	XH
	breq	8f		; channel empty, jump
	; something in the channel, see if it's ALTy
	ld	XL, Z+0

	push	ZH
	push	ZL
	mov	ZH, XH
	mov	ZL, XL
	sbiw	ZH:ZL, 6	; point Z at wproc[Pointer]
	ld	r20, Z+0
	ld	r21, Z+1	; r21:r20 = wproc[Pointer]

	pop	ZL
	pop	ZH


.L8:	; here means the channel is empty, save ourselves and reschedule
	sbiw	YH:YL, 6	; point Wptr at Wptr[Pointer]
	st	Y+0, r16
	st	Y+1, r17	; Wptr[Pointer] = data-addr
	clr	r20
	st	Y+2, r20
	st	Y+3, r20	; Wptr[Link] = null
	pop	XH
	pop	XL		; remove call return-address from stack
	st	Y+4, XL
	st	Y+5, XH		; Wptr[Iptr] = return-addr
	adiw	YH:YL, 6	; restore Wptr
	st	Z+0, YL
	st	Z+1, YH		; store Wptr in channel
	jmp	t4_scheduler	; reschedule

.endfunction

;}}}

.function t4_scheduler_saveiptr () ;{{{  scheduler entry-point (both of them, former saves state in Y[Iptr])
	; Note: no guarantee about preserving state here
t4_scheduler_callin:
	; entering here assumes that the process has been saved somewhere
	pop	XH		; remove call return-address from stack
	pop	XL

	sbiw	YH:YL, 2	; point Y at wptr[Iptr]
	st	Y+0, XL
	st	Y+1, XH		; wptr[Iptr] = return-addr
t4_scheduler:
	; pick a new process and schedule it
	ldi	YH:YL, V_t4_fptr
	ld	ZL, Y+0
	ld	ZH, Y+1		; load fptr into ZH:ZL
	tst	ZH
	breq	0f		; no processes on run-queue
	ld	XL, Y+2
	ld	XH, Y+3		; load bptr into XH:XL
	cp	XL, ZL
	brne	1f
	cp	XH, ZH
	brne	1f

	; here means that fptr == bptr (singleton process, now in ZH:ZL)
	clr	r16
	st	Y+0, r16
	st	Y+1, r16
	st	Y+2, r16
	st	Y+3, r16	; fptr = bptr = null

	mov	YH, ZH
	mov	YL, ZL
	rjmp	9f

.L1:	; here means that fptr != bptr (more than one process on the queue)
	; ZH:ZL has the contents of fptr
	sbiw	ZH:ZL, 4	; Z now pointing at fptr[Link]
	ld	XL, Z+0
	ld	XH, Z+1
	st	Y+0, XL
	st	Y+1, XH		; fptr = fptr[Link]
	adiw	ZH:ZL, 4	; Z now back to fptr[Temp]
	mov	YH, ZH
	mov	YL, ZL		; wptr = saved-fptr
	rjmp	9f

.L0:	; here means that fptr is null: no process
	ldi	YH:YL, V_t4_tptr
	ld	r16, Y+1
	tst	r16		; high-byte of tptr null?
	brne	2f

	; FIXME: anything else outstanding..?
	rjmp	3f

.L2:	; here means that we have something outstanding, so wait for it, then loop around
	sleep
	rjmp	t4_scheduler

.L3:	; here means that we're deadlocked
	nop
	nop
	nop
	nop

.L9:	; here means we're ready to go, process in YH:YL
	sbiw	YH:YL, 2	; point Y at wptr[Iptr]
	ld	ZL, Y+0
	ld	ZH, Y+1
	ijmp


.endfunction


;}}}

