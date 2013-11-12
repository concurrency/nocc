;	test_avr28.asm -- blinkenlight for the Arduino Uno (interrupts)
;	Fred Barnes, November 2013

.mcu	"atmega328"

.include "atmega328.inc"

.text
.include "atmega328-imap.inc"

.equ	COUNTER_MAX = 15

.data
.org	RAMSTART

Counter:
	.space	1

.text

;
;	VEC_timer0ovf: timer overflow interrupt.
;
VEC_timer0ovf:
	push	r16			; save r16, YH, YL
	push	YH
	push	YL
	in	r16, SREG		; save SREG
	push	r16

	ldi	YH:YL, Counter
	ld	r16, Y			; load Counter value
	dec	r16			; Counter--
	breq	0f			; if (Counter == 0)
	st	Y, r16			; store modified value back
	rjmp	2f
.L0:
	ldi	r16, COUNTER_MAX
	st	Y, r16			; Counter = COUNTER_MAX

	; enough timeouts, toggle LED
	sbic	PORTB, 5		; skip if PB5 low
	rjmp	1f
	; if here, means LED was off, so turn on
	sbi	PORTB, 5
	rjmp	2f
.L1:
	; if here, means LED was on, so turn off
	cbi	PORTB, 5

.L2:
	pop	r16			; restore SREG
	out	SREG, r16
	pop	YL			; restore YL, YH, r16
	pop	YH
	pop	r16
	reti


;
;	init_timer0: initialises the 8-bit timer/counter, prescale at /1024.
;	the interrupt will trigger approximately every 16.4ms.
;
init_timer0:
	push	r16
	push	YH
	push	YL

	; enable the timer/counter0 module
	ldi	YH:YL, PRR		; power reduction register
	ld	r16, Y
	andi	r16, 0xdf		; bit 5 (PRTIM0) off
	st	Y, r16

	ldi	r16, 0x00		; normal operation, OC0A/OC0B disconnected.
	out	TCCR0A, r16

	ldi	r16, 0x00		; clock stopped
	out	TCCR0B, r16

	ldi	r16, 0x00		; initial counter value (zero)
	out	TCNT0, r16

	; enable TOIE0 interrupt
	ldi	YH:YL, TIMSK0
	ld	r16, Y			; read TIMSK0
	ori	r16, 0x01		; TOIE0
	st	Y, r16			; write back to TIMSK0

	ldi	r16, 0x05		; start timer, prescale /1024
	out	TCCR0B, r16

	pop	YL
	pop	YH
	pop	r16
	ret

;
;	VEC_reset: program entry point.
;
VEC_reset:
	; setup stack-pointer
	ldi	r16, hi(RAMEND)
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei				; enable interrupts

	sbi	DDRB, 5			; PB5 output
	cbi	PORTB, 5		; PB5 low (LED off)

	ldi	YH:YL, Counter
	ldi	r16, COUNTER_MAX
	st	Y, r16			; Counter = COUNTER_MAX

	rcall	init_timer0

	; and we're off!
.L0:
	sleep
	rjmp	0b

	
