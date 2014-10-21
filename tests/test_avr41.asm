;
;	test_avr41.asm -- Moppy replacement code for floppy-drive fun
;	Copyright (C) 2014 Fred Barnes, University of Kent <frmb@kent.ac.uk>
;

.mcu	"atmega2560"
.include "atmega1280.inc"

;{{{  hardware wiring notes (Arduino Mega 2560)
;
; On the 18x2 connector, even numbered pins connect to 'step', odd numbered pins to 'dir'.
; MIDI channel mapping is thus:
;	0  = 22, 23
;	1  = 24, 25
;	     ...
;	15 = 52, 53
;
;}}}
;{{{  register usage notes
;
;	r2, r3:		used for collecting up serial packets (in 3 byte pieces)
;	r5:		counter for serial packet data
;
;	r6, r7, r8, r20:	temporaries for interrupt handler(s)
;
;	r10:		active map for MIDI channels 0-7
;	r11:		active map for MIDI channels 8-15
;
;}}}

.data
.org	RAMSTART

;{{{  channel states and counters (as note periods)
D_chans:
	.space	32				; 2 bytes per channel for set period (0x0000 = off)
D_counters:
	.space	32				; 2 bytes per channel for current period (increments to D_chans value)

;}}}

.text
.include "atmega1280-imap.inc"


udelay: ;{{{  delay for r16 microseconds
	push	r16
.L0:
	cpi	r16, 0
	breq	1f
	nop
	nop

	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop

	nop
	dec	r16
	rjmp	0b
.L1:

	pop	r16
	ret


;}}}
mdelay: ;{{{  delay for r16 milliseconds
	push	r16
	push	r17

	mov	r17, r16
	ldi	r16, 250
.L0:
	cpi	r17, 0
	breq	1f

	rcall	udelay				; 250 us (approx)
	rcall	udelay				; 250 us (approx)
	rcall	udelay				; 250 us (approx)
	rcall	udelay				; 250 us (approx)

	dec	r17
	rjmp	0b
.L1:

	pop	r17
	pop	r16
	ret

;}}}


init_usart0: ;{{{  initialises USART0 for 9600-8N1 comms (rx only)
	push	r16
	push	YH
	push	YL

	; enable USART0
	lds	r16, PRR0
	andi	r16, 0xfd
	sts	PPR0, r16			; clear bit 1 (PSUSART0)

	clr	r16
	sts	UCSR0B, r16			; RX & TX disable, no interrupts

	ldi	r16, 0x40
	sts	UCSR0A, r16			; write 1 to TXC0 to clear any pending interrupt, other flags disabled

	ldi	r16, 0
	sts	UBRR0H, r16
	ldi	r16, 103
	sts	UBRR0L, r16			; baud divisor to 103 (9600 at 16 MHz, 0.2% error)

	ldi	r16, 0x06
	sts	UCSR0C, r16			; asynchronous, no parity, 1 stop, 8 data

	; setup, now enable receiver and interrupts
	ldi	r16, 0x90
	sts	UCSR0B, r16			; RX interrupts and RX enable

	pop	YL
	pop	YH
	pop	r16
	ret

;}}}


VEC_usart0rx: ;{{{  USART0 receive interrupt
	in	r6, SREG

	lds	r20, UCSR0A			; read status register first
	lds	r7, UDR0			; read data register

	andi	r20, 0x1c			; mask in FE0, DOR0 and UPE0 bits (receive errors)
	breq	0f				; if no error, skip next instrs

	; here means we had an error (oops), ~4ms will give the rest of the packet time to clear
	push	r16
	ldi	r16, 1
	rcall	mdelay				; XXX: this will stuff ongoing playing, but heyho
	lds	r7, UDR0			; empty data register (if anything)
	rcall	mdelay
	lds	r7, UDR0
	rcall	mdelay
	lds	r7, UDR0
	rcall	mdelay
	lds	r7, UDR0
	pop	r16

	clr	r5				; clear counter
	rjmp	9f				; get out
.L0:
	; here means a good character received, in r7

	ldi	r6, 1
	cp	r5, r6
	brlo	1f				; branch if first character
	breq	2f				; branch if second character
	; else 3rd character, pack away and handle on/off

	; r2 has channel (0-15), r3 has high-order bits, r7 low-order
	ldi	r20, 100			; magic reset value
	cp	r2, r20
	breq	15f

	push	XH				; save X register
	push	XL

	ldi	XH:XL, D_chans
	mov	r8, r2
	lsl	r8				; channel * 2
	add	XL, r8				; XXX: no need for XH fixup, since D_chans well-aligned

	st	X+, r3
	st	X, r7				; store 16-bit value

	ldi	XH:XL, D_counters
	add	XL, r8				; XXX: no need for XH fixup, since D_counters well-aligned

	clr	r5				; clear serial input counter
	st	X+, r5
	st	X, r5				; clear 16-bit value

	pop	XL				; restore X register
	pop	XH

	or	r3, r7				; mix up for 16-bit value
	breq	3f				; branch if zero (i.e. note off)

	; else note definitely on, ensure relevant bit set (channel in r2)
	ldi	r8, 8
	cp	r2, r8
	brlo	33f				; in channel 0-7 group
	; else channel 8-15
.L3:
	; note definitely off, ensure relevant bit clear (channel in r2)
	ldi	r8, 8
	cp	r2, r8

.L1:
	; first character coming in
	mov	r2, r7
	inc	r5
	rjmp	9f
.L15:
	; here means we're resetting! (interrupts already disabled)
	rjmp	VEC_reset

.L2:
	; second character
	mov	r3, r7
	inc	r5

	; fall through
.L9:
	out	SREG, r6
	reti

;}}}
VEC_reset: ;{{{  reset vector: program entry point (and also run-time reset)
	cli

	ldi	r16, hi(RAMEND)			; sort out SP
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16

	rcall	init_pstate			; initialise program state
	rcall	init_usart0			; initialise USART0
	rcall	init_timer0			; initialise TIMER0

	sei					; and we're off!

;}}}


