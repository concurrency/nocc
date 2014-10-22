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
;	0  = 22, 23			PA0, PA1	+--.
;	1  = 24, 25			PA2, PA3	   |  port state in r11
;	2  = 26, 27			PA4, PA5	   |
;	3  = 28, 29			PA6, PA7	+--'
;	4  = 30, 31			PC7, PC6	+--.
;	5  = 32, 33			PC5, PC4	   |  port state in r12
;	6  = 34, 35			PC3, PC2	   |
;	7  = 36, 37			PC1, PC0	+--'
;	8  = 38, 39			PD7, PG2	\     port state in r15 low
;	9  = 40, 41			PG1, PG0	/
;	10 = 42, 43			PL7, PL6	+--.
;	11 = 44, 45			PL5, PL4	   |  port state in r14
;	12 = 46, 47			PL3, PL2	   |
;	13 = 48, 49			PL1, PL0	+--'
;	14 = 50, 51			PB3, PB2	\     port state in r13 low
;	15 = 52, 53			PB1, PB0	/
;
;}}}
;{{{  register usage notes
;
;	r2, r3:		used for collecting up serial packets (in 3 byte pieces)
;	r5:		counter for serial packet data
;
;	r6, r7, r8, r20:	temporaries for interrupt handler(s)
;	YH (=r29), YL (=r28):	exclusive use for timer interrupt handler
;	ZH (=r31), ZL (=r30):	exclusive use for timer interrupt handler
;
;	r9:		active map for MIDI channels 0-7
;	r10:		active map for MIDI channels 8-15
;
;	r11:		PORTA current output state (channels 0-3): mask 0xff
;	r12:		PORTC current output state (channels 4-7): mask 0xff
;	r13:		PORTB current output state (low order bits, PB3-0, channels 8-9): mask 0x0f
;	r14:		PORTL current output state (channels 10-13): mask 0xff
;	r15:		PORTG/D current output state (low order bits, PD7, PG2-0, channels 14-15): mask 0x0f
;
;}}}

.data
.org	RAMSTART

;{{{  channel states and counters (as note periods)
D_chans:
	.space	16*8				; 8 bytes per MIDI channel:
						;   0-1 = running counter
						;   2-3 = set period in microseconds (0x0000 == off)
						;     4 = position counter (0-79)
						;     5 = direction
						;  [6-7 = unused]

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


init_pstate: ;{{{  initialises program state
	push	r16
	push	r17
	push	XL
	push	XH

	ldi	r16, 16
	ldi	XH:XL, D_chans			; guaranteed well-aligned :)

.L0:
	clr	r17
	st	X+, r17				; clear running counter value
	st	X+, r17

	st	X+, r17				; clear period value
	st	X+, r17

	st	X+, r17				; clear position counter
	st	X+, r17				; clear direction

	ser	r17
	st	X+, r17				; set unused bytex to 0xff (will likely fail if we accidently pick these up)
	st	X+, r17

	dec	r16
	brne	0b				; and round again :)

	clr	r5				; clear serial counter
	clr	r9				; clear channel 0-7 map
	clr	r10				; clear channel 8-15 map

	pop	XH
	pop	XL
	pop	r17
	pop	r16
	ret

;}}}
init_hwstate: ;{{{  initialises hardware port state (all outputs)
	push	r16
	push	r17

	ser	r16
	clr	r17
	sts	DDRL, r16			; port L all output
	sts	PORTL, r17			; all outputs low

	out	DDRA, r16			; port A all output
	out	PORTA, r17			; all outputs low

	out	DDRC, r16			; port C all output
	out	PORTC, r17			; all outputs low

	; then the pieces G0-2, B0-3, D7
	in	r16, DDRG
	ori	r16, 0x07
	out	DDRG, r16			; port G 0-2 output

	in	r17, PORTG
	andi	r17, 0xf8
	out	PORTG, r17			; outputs 0-2 low

	in	r16, DDRB
	ori	r16, 0x0f
	out	DDRB, r16			; port B 0-3 output

	in	r17, PORTB
	andi	r17, 0xf0
	out	PORTB, r17			; outputs 0-3 low

	in	r16, DDRD
	ori	r16, 0x80
	out	DDRD, r16			; port D 7 output

	in	r17, PORTD
	andi	r17, 0x7f
	out	PORTD, r17			; output 7 low

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
	sts	PRR0, r16			; clear bit 1 (PSUSART0)

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
init_timer0: ;{{{  initialises TIMER0 for 20us interrupts
	push	r16

	ldi	r16, 0x02
	out	TCCR0A, r16			; setup for CTC mode (counts from 0 to OCR0A)
	ldi	r16, 0x00
	out	TCCR0B, r16			; ensure TIMER0 is *off*

	ldi	r16, 40				; 20us in .5us bits
	out	OCR0A, r16
	clr	r16
	out	TCNT0, r16			; clear current timer value (if any)

	ldi	r16, 0x02			; OCIE0A bit
	out	TIFR0, r16			; clear this interrupt if set
	sts	TIMSK0, r16			; XXX: this is not in/out addressable!

	ldi	r16, 0x02
	out	TCCR0B, r16			; prescale /8 (0.5us at 16 MHz)

	pop	r16
	ret
;}}}

reset_drives: ;{{{  called to ensure all [physical] drives are at track 0
	push	r16
	push	r17
	push	r18

	ldi	r16, 0xaa
	mov	r11, r16
	out	PORTA, r11			; direction pins high for port A

	ldi	r16, 0x55
	mov	r12, r16
	out	PORTC, r12			; direction pins high for port C

	ldi	r16, 0x05
	mov	r15, r16			; redundant :D
	sbi	PORTG, 2			; direction pins high for PG2,0
	sbi	PORTG, 0

	ldi	r16, 0x55
	mov	r14, r16
	sts	PORTL, r14			; direction pins high for port L

	ldi	r16, 0x55
	mov	r13, r16			; redundant :D
	sbi	PORTB, 2			; direction pins high for PB2,0
	sbi	PORTB, 0

	; now pulse all signal pins 79 times
	ldi	r17, 79
	ldi	r16, 0xff
.L0:
	out	PORTA, r16
	out	PORTC, r16
	sts	PORTL, r16
	sbi	PORTD, 7
	sbi	PORTG, 1
	sbi	PORTB, 3
	sbi	PORTB, 1

	rcall	udelay				; wait for ~250us

	out	PORTA, r11
	out	PORTC, r12
	sts	PORTL, r14
	cbi	PORTD, 7
	cbi	PORTG, 1
	cbi	PORTB, 3
	cbi	PORTB, 1

	rcall	udelay				; wait for ~250us
	rcall	udelay				; wait for ~250us

	dec	r17
	brne	0b

	; done, clear all.
	ldi	r16, 0x00
	out	PORTA, r16
	out	PORTC, r16
	sts	PORTL, r16
	cbi	PORTG, 2
	cbi	PORTG, 0
	cbi	PORTB, 2
	cbi	PORTB, 0

	clr	r11
	clr	r12
	clr	r13
	clr	r14
	clr	r15

	pop	r18
	pop	r17
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

	clr	r6
	inc	r6				; r6 == 1

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
	lsl	r8				; channel * 8
	lsl	r8
	lsl	r8
	add	XL, r8				; XXX: no need for XH fixup, since D_chans well-aligned

	; clear out counter, set period
	clr	r5				; clear serial input counter
	st	X+, r5				; counter = 0
	st	X+, r5

	st	X+, r3
	st	X, r7				; store 16-bit value

	pop	XL				; restore X register
	pop	XH

	; put relevant channel bit into r20
	ldi	r20, 0x01
	sbrc	r2, 0
	lsl	r20
	sbrc	r2, 1
	lsl	r20
	sbrc	r2, 1
	lsl	r20
	sbrc	r2, 2
	swap	r20

	or	r3, r7				; mix up for 16-bit value
	breq	3f				; branch if zero (i.e. note off)

	; else note definitely on, ensure relevant bit set (channel in r2)
	sbrs	r2, 3
	or	r9, r20				; low-group, set bit in r9
	sbrc	r2, 3
	or	r10, r20			; high-group, set bit in r10

	rjmp	9f				; all done here, get out!
.L3:
	; note definitely off, ensure relevant bit clear (channel in r2)
	com	r20				; flip bits in r20 (mask out)
	sbrs	r2, 3
	and	r9, r20				; low-group, clear bit in r9
	sbrc	r2, 3
	and	r10, r20			; high-group, clear bit in r10

	rjmp	9f				; all done, get out!

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
VEC_timer0compa: ;{{{  TIMER0 comparator A interrupt (20us hits)
	in	r6, SREG

	ldi	YH:YL, D_chans

	sbrs	r9, 0
	rjmp	1f
	; channel 0 active
	ldd	ZH, Y+0				; load current counter
	ldd	ZL, Y+1
	adiw	ZH:ZL, 20			; 20 microseconds have elapsed
	ldd	r7, Y+2
	ldd	r6, Y+3
	cp	ZL, r6
	cpc	ZH, r7

	std	Y+0, ZH
	std	Y+1, ZL

.L1:
	; FIXME: do some stuff here!

	out	SREG, r6
	reti
;}}}
VEC_reset: ;{{{  reset vector: program entry point (and also run-time reset)
	cli

	ldi	r16, hi(RAMEND)			; sort out SP
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16

	rcall	init_hwstate			; initialise hardware state
	rcall	init_pstate			; initialise program state
	rcall	init_usart0			; initialise USART0

	rcall	reset_drives			; make sure all drives are at the starting position

	rcall	init_timer0			; initialise TIMER0

	sei					; and we're off!

loop:
	nop
	nop
	nop
	nop
	nop
	nop
	rjmp	loop

;}}}


