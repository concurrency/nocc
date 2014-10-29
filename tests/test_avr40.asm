;	test_avr40.asm -- testing external memory interface
;	Fred Barnes, 2014.
;

.mcu	"atmega2560"

.include "atmega1280.inc"

.data
.org	0x2200
D_extmem:	.space	0xde00			; occupy entire 64k external (55.5k in reality)

.text
.include "atmega1280-imap.inc"

;{{{  string constants

S_hello:	.const	"Hello, world!\r\n", 0x00
S_numstr:	.const	"0123456789", 0x00
S_hexstr:	.const	"0123456789ABCDEF", 0x00

S_failaddr:	.const	" failed read/write pattern\r\n", 0x00
S_okaddr:	.const	" OK\r\n", 0x00
S_doneaddr:	.const	"Test complete!\r\n", 0x00

;}}}

VEC_reset:				; START HERE
	cli
	ldi	r16, hi(RAMEND)		; init stack-pointer to end of internal SRAM
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16

	call	usart_init		; init communications with the world
	call	xmem_init		; enable external memory interface

	sei				; interrupts on
	rjmp	prg_main


udelay:		;{{{  suspend for r16 microsecconds
	push	r16
.L0:
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
	nop
	nop
	nop
	dec	r16
	brne	0b

	pop	r16
	ret

;}}}
mdelay:		;{{{  suspend for r16 milliseconds
	push	r16
	push	r17

	mov	r17, r16
.L0:
	ldi	r16, 249
	rcall	udelay
	rcall	udelay
	rcall	udelay
	rcall	udelay
	dec	r17
	brne	0b

	pop	r17
	pop	r16
	ret

;}}}
sdelay:		;{{{  suspend for r16 seconds (ish!)
	push	r16
	push	r17

	mov	r17, r16
.L0:
	ldi	r16, 249
	rcall	mdelay
	rcall	mdelay
	rcall	mdelay
	rcall	mdelay
	dec	r17
	brne	0b

	pop	r17
	pop	r16
	ret

;}}}

usart_init:	;{{{  initialise USART0 (hooked up to USB)
	push	r16
	push	YH
	push	YL

	; enable USART0 module
	ldi	YH:YL, PRR0
	ld	r16, Y
	andi	r16, 0xfd		; clear bit 1 (PSUSART0)
	st	Y, r16

	ldi	YH:YL, UCSR0B
	ldi	r16, 0x00		; RX and TX disable, no interrupts.
	st	Y, r16

	ldi	YH:YL, UCSR0A
	ldi	r16, 0x40		; write 1 to TXC0 to clear it, other flags disabled
	st	Y, r16

	ldi	YH:YL, UBRR0H
	ldi	r16, 0			; 12-bit divisor is 25 at 16 MHz
	st	Y, r16
	ldi	YH:YL, UBRR0L
	ldi	r16, 25
	st	Y, r16			; divisor latched when UBRR0L written

	ldi	YH:YL, UCSR0C
	ldi	r16, 0x06		; asynch, no parity, 1 stop, 8 data
	st	Y, r16

	; now we're setup, enable transmitter and receiver
	ldi	YH:YL, UCSR0B
	ldi	r16, 0x18		; RX and TX enable, no interrupts.
	st	Y, r16

	pop	YL
	pop	YH
	pop	r16
	ret
;}}}
usart_txbyte: ;{{{  transmits the byte in r16
	push	r17

.L0:
	lds	r17, UCSR0A
	sbrs	r17, 5
	rjmp	0b			; wait for bit 5 to be set (UDRE)

	sts	UDR0, r16

	pop	r17
	ret
;}}}
usart_txpstr: ;{{{  transmits the string pointed at by Z in program memory to USART0
	push	ZL
	push	ZH
	push	r16

.L0:
	lpm	r16, Z+
	cpi	r16, 0x00
	breq	1f			; end-of-string
	rcall	usart_txbyte
	rjmp	0b
.L1:

	pop	r16
	pop	ZH
	pop	ZL
	ret
;}}}
usart_tx16dec:	;{{{  blarts out a 16-bit value in r17:r16 to the USART in base 10
	push	ZH
	push	ZL
	push	r16
	push	r17
	push	r18
	push	r19
	push	r20			; collect digits in r20-r24
	push	r21
	push	r22
	push	r23
	push	r24

	clr	r20
	clr	r21
	clr	r22
	clr	r23

	; NOTE: max 5 digits here!
	ldi	r19:r18, 10000
.L0:
	cp	r16, r18
	cpc	r17, r19
	brlo	1f			; branch if < 10000
	; r17:r16 is 10000 or higher
	inc	r20
	sub	r16, r18
	sbc	r17, r19
	rjmp	0b
.L1:

	ldi	r19:r18, 1000
.L0:
	cp	r16, r18
	cpc	r17, r19
	brlo	1f			; branch if < 1000
	; r17:r16 is 1000-9999
	inc	r21
	sub	r16, r18
	sbc	r17, r19
	rjmp	0b
.L1:

	clr	r19
	ldi	r18, 100
.L0:
	cp	r16, r18
	cpc	r17, r19
	brlo	1f			; branch if < 100
	; r17:r16 is 100-999
	inc	r22
	sub	r16, r18
	sbc	r17, r19
	rjmp	0b
.L1:

	ldi	r18, 10
.L0:
	cp	r16, r18
	brlo	1f			; branch if < 10
	; r16 is 10-99
	inc	r23
	sub	r16, r18
	rjmp	0b
.L1:
	; r16 has leftovers
	mov	r24, r16

	; next, bang out.
	ldi	r16, '0'
	add	r16, r20
	rcall	usart_txbyte
	ldi	r16, '0'
	add	r16, r21
	rcall	usart_txbyte
	ldi	r16, '0'
	add	r16, r22
	rcall	usart_txbyte
	ldi	r16, '0'
	add	r16, r23
	rcall	usart_txbyte
	ldi	r16, '0'
	add	r16, r24
	rcall	usart_txbyte

	pop	r24
	pop	r23
	pop	r22
	pop	r21
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	pop	ZL
	pop	ZH
	ret

;}}}

xmem_init:	;{{{  initialise external SRAM interface
	push	r16
	ldi	r16, 0x00
	out	DDRA, r16
	ldi	r16, 0x00
	out	PORTA, r16		; pullups on^W OFF

	ldi	r16, 0x80		; SRE, whole region, ... wait states
	sts	XMCRA, r16
	ldi	r16, 0x00
	sts	XMCRB, r16

	pop	r16
	ret
;}}}

prg_main:
	ldi	r16, 5			; 5 seconds to attach RS232!
	rcall	sdelay

	ldi	ZH:ZL, S_hello
	rcall	usart_txpstr

prg_loop:
	ldi	XH:XL, D_extmem		; address of external SRAM (0x2200 upwards)
	ldi	r19, 0
.L0:
	ldi	r20, 0x00
.L1:
	st	X, r20
	nop
	nop
	ld	r21, X
	nop
	nop

	cp	r20, r21
	breq	2f
	; if here, means we failed somehow!
	mov	r17, XH
	mov	r16, r19
	rcall	usart_tx16dec
	ldi	ZH:ZL, S_failaddr
	rcall	usart_txpstr
.L2:
	inc	r20			; pattern increment
	brne	1b
	adiw	XH:XL, 1		; address increment
	inc	r19
	brne	0b

	mov	r17, XH
	dec	r17
	mov	r16, XL
	rcall	usart_tx16dec
	ldi	ZH:ZL, S_okaddr
	rcall	usart_txpstr

	; if XH:XL wrapper to 0, means we're done!
	mov	r21, XH
	or	r21, XL
	cpi	r21, 0x00
	breq	3f
	rjmp	0b
.L3:
	; all done!
	ldi	ZH:ZL, S_doneaddr
	rcall	usart_txpstr

	ldi	r16, 2
	rcall	sdelay

	nop
	nop
	nop
	nop
	rjmp	prg_loop

