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
.equ	SPIMODE_BIT_TX	=0		; transmitting
.equ	SPIMODE_BIT_TXDONE =1		; done transmitting
.equ	SPIMODE_BIT_TXRX =2		; if receive whilst transmitting

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
	push	r31
	in	r31, SREG
	push	r29
	push	r28
	push	r27
	push	r26
	push	r17
	push	r16

	ldi	r27:r26, V_spi_mode
	ld	r16, X			; r16 = V_spi_mode
	sbrs	r16, SPIMODE_BIT_TX
	rjmp	9f			; branch if TX flag not set (spurious interrupt)

	; transmitting, if tx-rx, retrieve returned bit
	sbrs	r16, SPIMODE_BIT_TXRX
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

.L1:
	; was this the last bit?
	ldi	r29:r28, V_spi_txcount
	ld	r16, Y+
	ld	r17, Y
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
	ld	r16, X
	andi	r16, ~(1 << SPIMODE_BIT_TX)
	ori	r16, (1 << SPIMODE_BIT_TXDONE)
	st	X, r16

.L9:
	pop	r16
	pop	r17
	pop	r26
	pop	r27
	pop	r28
	pop	r29
	out	SREG, r31
	pop	r31
	reti

;}}}

