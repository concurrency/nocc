;	test_avr30.asm -- serial-port echo for the Arduino Uno (interrupt-driven)
;	Fred Barnes, November 2013

.mcu	"atmega328"

.include "atmega328.inc"

.text
.include "atmega328-imap.inc"

.data
.org	RAMSTART

; some flags for send and receive
RecvFlag:
	.space	1
SendFlag:
	.space	1

; and the data
RecvData:
	.space	1

.text


;
;	VEC_usart0rx: USART0 receive interrupt, triggered when a complete character is ready for reading.
;
VEC_usart0rx:
	push	r16			; save r16, r17, YH, YL, SREG
	push	r17
	push	YH
	push	YL
	in	r16, SREG
	push	r16

	ldi	YH:YL, UCSR0A
	ld	r17, Y			; read UCSR0A (before UDR0)

	ldi	YH:YL, UDR0
	ld	r16, Y			; read character from receive buffer
	ldi	YH:YL, RecvData
	st	Y, r16			; store in 'RecvData'

	andi	r17, 0x1c		; mask in FE0, DOR0 and UPE0 bits (receive errors)
	brne	0f			; if (error), skip flag setting
	ldi	YH:YL, RecvFlag
	ldi	r16, 1
	st	Y, r16			; RecvFlag = 1
.L0:

	pop	r16			; restore SREG, YL, YH, r17, r16
	out	SREG, r16
	pop	YL
	pop	YH
	pop	r17
	pop	r16
	reti


;
;	VEC_usart0tx: USART0 transmit interrupt, triggered when a character has been sent.
;
VEC_usart0tx:
	reti


;
;	init_usart0: initialises the USART at 38400-8N1.
;
init_usart0:
	push	r16
	push	YH
	push	YL

	; enable the USART0 module
	ldi	YH:YL, PRR		; power reduction register
	ld	r16, Y
	andi	r16, 0xfd		; bit 1 (PRUSART0) off
	st	Y, r16

	ldi	YH:YL, UCSR0B
	ldi	r16, 0x00		; RX and TX disable, no interrupts.
	st	Y, r16

	ldi	YH:YL, UCSR0A
	ldi	r16, 0x40		; write 1 to TXC0 to clear it, other flags disabled.
	st	Y, r16

	ldi	YH:YL, UBRR0H
	ldi	r16, 0			; 12-bit divisor is 25 at 16 MHz (page 199 in datasheet).
	st	Y, r16
	ldi	YH:YL, UBRR0L
	ldi	r16, 25
	st	Y, r16			; divisor set when UBRR0L is written.

	ldi	YH:YL, UCSR0C
	ldi	r16, 0x06		; asynchronous, no parity, 1 stop bit, 8 data bits.
	st	Y, r16

	; now we're setup, enable transmitter and receiver
	ldi	YH:YL, UCSR0B
	ldi	r16, 0xd8		; RX and TX enable, RX and TX interrupts
	st	Y, r16

	pop	YL
	pop	YH
	pop	r16
	ret

;
;	flip_led: changes the state of the on-board LED (useful for debugging).
;
flip_led:
	sbic	PORTB, 5		; skip if PB5 low
	rjmp	0f
	; if here, means LED was off, so turn on
	sbi	PORTB, 5
	rjmp	1f
.L0:
	; if here, means LED was on, so turn off
	cbi	PORTB, 5
.L1:
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

	rcall	init_usart0

	ldi	XH:XL, RecvFlag		; X = address of 'RecvFlag'
	ldi	YH:YL, SendFlag		; Y = address of 'SendFlag'
	ldi	ZH:ZL, RecvData		; Z = address of 'RecvData'

.L0:
	ld	r16, X
	cpi	r16, 1			; compare RecvFlag with 1
	breq	1f			; if (character received)
	sleep				; otherwise sleep
	rjmp	0b			; then try again
.L1:

	ld	r17, Z			; r17 = RecvData
	clr	r16
	st	X, r16			; RecvFlag = 0

	andi	r16, 0x80		; mask RXC0 bit (receive complete)
	breq	0b			; if zero (nothing received), loop

	; character received, collect
	ld	r16, Z			; read character out of receive buffer

	rcall	flip_led		; flip LED state to show we're doing something

	st	Z, r16			; put character back into transmit buffer
.L1:
	ld	r16, Y
	mov	r17, r16		; r17 = r16
	andi	r16, 0x40		; mask TXC0 bit (transmit complete)
	breq	1b			; loop until this bit becomes set

	; transmit complete, r17 has the contents of UCSR0A with TXC0 bit set.
	st	Y, r17			; write to TXC0 to clear it

	rjmp	0b

	
