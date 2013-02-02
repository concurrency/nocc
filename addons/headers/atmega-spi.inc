;
;	atmega-spi.inc -- general definitions for the ATmega SPI
;
;	Hacked up by Fred Barnes, University of Kent  <frmb@kent.ac.uk>
;	most stuff taken from Atmel's AVR documentation
;

; for the typical ATmega SPI port assignments
.equ	SPI_PORT	=PORTB
.equ	SPI_DDR		=DDRB

.equ	SPI_BIT_SS	=2
.equ	SPI_BIT_MOSI	=3
.equ	SPI_BIT_MISO	=4
.equ	SPI_BIT_SCK	=5

; SPCR register bits
.equ	SPCR_BIT_SPIE	=7		; interrupt-enable (VEC_spistc)
.equ	SPCR_BIT_SPE	=6		; SPI enable
.equ	SPCR_BIT_DORD	=5		; 0 = MSB first, 1 = LSB first
.equ	SPCR_BIT_MSTR	=4		; master operation
.equ	SPCR_BIT_CPOL	=3		; clock polarity
.equ	SPCR_BIT_SPHA	=2		; clock phase
.equ	SPCR_DIV4	=0x00		; Fosc / 4
.equ	SPCR_DIV16	=0x01		; Fosc / 16
.equ	SPCR_DIV64	=0x02		; Fosc / 64
.equ	SPCR_DIV128	=0x03		; Fosc / 128

; SPSR register bits
.equ	SPSR_BIT_SPIF	=7		; SPI interrupt flag (cleared in vector handler)
.equ	SPSR_BIT_WCOL	=6		; write collision
.equ	SPSR_BIT_SPI2X	=0		; double-speed-mode


; local SPI mode register bits
.equ	SPIMODE_BIT_TX		=0	; transmitting
.equ	SPIMODE_BIT_TXDONE	=1	; done transmitting
.equ	SPIMODE_BIT_TXERR	=2	; error for some reason
.equ	SPIMODE_BIT_TXRX	=3	; if receive whilst transmitting
.equ	SPIMODE_BIT_TXRXCHK	=4	; if call check function with received data whilst transmitting

.data
V_spi_txbuf:
	.space	16			; 16-bytes of write buffer
V_spi_rxbuf:
	.space	16			; 16-bytes of read buffer

; note: order of these matters

V_spi_txcount:
	.space	1			; tx-count (in buffer)
V_spi_txptr:
	.space	1			; tx-ptr (in buffer)
V_spi_rxcount:
	.space	1			; rx-count (in buffer)
V_spi_mode:
	.space	1			; SPI mode
V_spi_txrxcheck:
	.space	2			; address of routine called during interrupt

.text

.macro	spi_setSS ()			;{{{  sets SS (low)
	cbi	SPI_PORT, SPI_BIT_SS
.endmacro


;}}}
.macro	spi_clearSS ()			;{{{  sets SS (high)
	sbi	SPI_PORT, SPI_BIT_SS
.endmacro


;}}}
.function spi_init_master ()		;{{{  initialises the SPI in master mode
	push	r16
	push	r27
	push	r26

	; initialise local vars
	ldi	r27:r26, V_spi_txcount
	clr	r16
	st	X+, r16
	st	X+, r16
	st	X+, r16
	st	X, r16

	; make MOSI, SCK and SS output, MISO input
	in	r16, DDRB
	ori	r16, (1 << SPI_BIT_MOSI) | (1 << SPI_BIT_SCK) | (1 << SPI_BIT_SS)
	andi	r16, ~(1 << SPI_BIT_MISO)
	out	DDRB, r16

	spi_clearSS			; clear SS (set high)

	; setup for SPI-enable, interrupts, master, div/16
	ldi	r16, (1 << SPCR_BIT_SPIE) | (1 << SPCR_BIT_SPE) | (1 << SPCR_BIT_MSTR) | SPCR_DIV16
	out	SPCR, r16

	pop	r26
	pop	r27
	pop	r16
.endfunction


;}}}
VEC_spistc:				;{{{  interrupt handler for SPI/STC
	in	INTREG0, SREG
	push	r31
	push	r30
	push	r29
	push	r28
	push	r27
	push	r26
	push	r18
	push	r17
	push	r16

	in	r16, SPSR		; throw-away

	ldi	r27:r26, V_spi_mode
	ld	r18, X			; r16 = V_spi_mode
	sbrs	r18, SPIMODE_BIT_TX
	rjmp	9f			; branch if TX flag not set (spurious interrupt)

	; transmitting, if tx-rx, retrieve returned bit
	sbrs	r18, SPIMODE_BIT_TXRX
	rjmp	1f

	ldi	r29:r28, V_spi_rxcount
	ld	r17, Y
	ldi	r27:r26, V_spi_rxbuf
	clr	r16
	add	r26, r17
	adc	r27, r16		; X = V_spi_rxbuf + V_spi_rxcount(r17)

	in	r16, SPDR
	st	X, r16			; V_spi_rxbuf[V_spi_rxcount] = SPDR
	inc	r17
	st	Y, r17			; V_spi_rxcount++

	sbrs	r18, SPIMODE_BIT_TXRXCHK
	rjmp	3f

	; here we call the indirectly specified routine to check what we just read
	; setup with r16 = received value, r17 = txptr (one just gone)

	ldi	r29:r28, V_spi_txrxcheck
	ld	r31, Y+
	ld	r30, Y
	ldi	r29:r28, V_spi_txptr
	ld	r17, Y+			; leave Y pointing at rxcount
	icall
	cpi	r16, 1
	breq	4f			; abort
	ld	r17, Y			; fall through with r17 = rxcount

.L3:
	; if we have filled the RX buffer, stop (sanity)
	cpi	r17, 16
	breq	4f			; abort

.L1:
	; was this the last bit?
	ldi	r29:r28, V_spi_txcount
	ld	r16, Y+			; r16 = txcount
	ld	r17, Y			; r17 = txptr
	inc	r17			; because we've transmitted this one
	st	Y, r17
	cp	r16, r17
	breq	2f			; yes, all gone, branch

	; still more to transmit (r17 = offset-of-next)
	ldi	r27:r26, V_spi_txbuf
	clr	r16
	add	r26, r17
	adc	r27, r16

	ld	r16, X			; load next byte to transmit
	out	SPDR, r16

	rjmp	9f

.L2:
	; last byte gone, mark as done
	ldi	r27:r26, V_spi_mode
	ld	r18, X
	andi	r18, ~(1 << SPIMODE_BIT_TX)
	ori	r18, (1 << SPIMODE_BIT_TXDONE)
	st	X, r18

	; clear SS
	spi_clearSS
	rjmp	9f

.L4:
	; abort way out
	ldi	r27:r26, V_spi_mode
	ld	r18, X
	andi	r18, ~(1 << SPIMODE_BIT_TX)
	ori	r18, (1 << SPIMODE_BIT_TXERR)
	st	X, r18

	; clear SS
	spi_clearSS

.L9:

	pop	r16
	pop	r17
	pop	r18
	pop	r26
	pop	r27
	pop	r28
	pop	r29
	pop	r30
	pop	r31
	out	SREG, INTREG0
	reti

;}}}
.function spi_reset_txbuf ()		;{{{  clears the TX buffer
	push	r16
	push	r26
	push	r27

	ldi	r27:r26, V_spi_txcount
	clr	r16
	st	X+, r16			; clear txcount
	st	X, r16			; clear txptr

	pop	r27
	pop	r26
	pop	r16
.endfunction


;}}}
.function spi_reset_rxbuf ()		;{{{  clears the RX buffer
	push	r16
	push	r26
	push	r27

	ldi	r27:r26, V_spi_rxcount
	clr	r16
	st	X, r16			; clear rxcount

	pop	r27
	pop	r26
	pop	r16
.endfunction


;}}}
.function spi_setmode ()		;{{{  sets local SPI mode (in r16, SPIMODE_...)
	push	r26
	push	r27

	ldi	r27:r26, V_spi_mode
	st	X, r16

	pop	r27
	pop	r26
.endfunction


;}}}
.function spi_settxcheck ()		;{{{  sets local TX check function pointer (in X = r27:r26)
	push	r28
	push	r29

	ldi	r29:r28, V_spi_txrxcheck
	st	Y+, r27
	st	Y, r26

	pop	r29
	pop	r28
.endfunction


;}}}
.function spi_add_txbuf ()		;{{{  adds byte in r16 to TX buffer
	push	r17
	push	r18
	push	r26
	push	r27
	ldi	r27:r26, V_spi_txcount
	ld	r17, X
	ldi	r27:r26, V_spi_txbuf
	clr	r18
	add	r26, r17
	adc	r27, r18		; X = txbuf + txcount
	st	X, r16			; txbuf[txcount] = r16

	inc	r17
	ldi	r27:r26, V_spi_txcount
	st	X, r17			; txcount++

	pop	r27
	pop	r26
	pop	r18
	pop	r17
.endfunction


;}}}
.function spi_start_tx ()		;{{{  start SPI transmit
	push	r16
	push	r17
	push	r26
	push	r27

	ldi	r27:r26, V_spi_mode
	ld	r16, X
	ori	r16, (1 << SPIMODE_BIT_TX)
	andi	r16, ~((1 << SPIMODE_BIT_TXDONE) | (1 << SPIMODE_BIT_TXERR))
	st	X, r16

	; set SS
	spi_setSS

	ldi	r27:r26, V_spi_txptr
	ld	r16, X
	ldi	r27:r26, V_spi_txbuf
	clr	r17
	add	r26, r16
	adc	r27, r17		; r27:r26 = txbuf + txptr

	ld	r16, X			; load first/next byte to transmit
	out	SPDR, r16

	pop	r27
	pop	r26
	pop	r17
	pop	r16
.endfunction


;}}}
.function spi_lastrx ()			;{{{  extract last received character
	push	r27
	push	r26
	push	r18
	push	r17

	ldi	r27:r26, V_spi_rxcount
	ld	r17, X
	tst	r17
	breq	0f			; branch if nothing in RX buffer

	dec	r17
	ldi	r27:r26, V_spi_rxbuf
	clr	r18
	add	r26, r17
	adc	r27, r18
	ld	r16, X			; r16 = rxbuf[rxcount-1]
	rjmp	1f
.L0:
	ldi	r16, 0x00

.L1:
	pop	r17
	pop	r18
	pop	r26
	pop	r27
.endfunction


;}}}
.function spi_restart_txrx ()		;{{{  resets TX and RX counters for retry
	push	r16
	push	r26
	push	r27

	ldi	r27:r26, V_spi_txptr
	clr	r16
	st	X+, r16			; txptr = 0
	st	X, r16			; rxcount = 0

	pop	r27
	pop	r26
	pop	r16
.endfunction


;}}}

.function spi_waittx ()			;{{{  wait for transmit to complete, returns r16=1 if abort, r16=0 if completed normally
	push	r27
	push	r26

.L0:
	ldi	r27:r26, V_spi_mode
	ld	r16, X
	sbrc	r16, SPIMODE_BIT_TXDONE
	rjmp	3f
	sbrc	r16, SPIMODE_BIT_TXERR
	rjmp	2f

	sleep
	rjmp	0b
.L3:
	; successful transmit
	clr	r16
	rjmp	1f

.L2:
	; transmit error..
	ldi	r16, 1
.L1:
	pop	r26
	pop	r27
.endfunction


;}}}



