;	test_avr29.asm -- serial-port echo for the Arduino Uno (polling)
;	Fred Barnes, November 2013

.mcu	"atmega328"

.include "atmega328.inc"

.text
.include "atmega328-imap.inc"


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
	ldi	r16, 0x18		; RX and TX enable, no interrupts.
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

	ldi	YH:YL, UCSR0A		; Y = address of UCSR0A (control/status register)
	ldi	ZH:ZL, UDR0		; Z = address of UDR0 (data register)
.L0:
	ld	r16, Y
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

	
