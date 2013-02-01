;
;	dfrobot-arduino-ether.inc -- code to interface with the DFRobot ethernet shield (Wiznet W5100)
;

.include "atmega-spi.inc"

.equ	W5100_MR	=0x0000
.equ	W5100_GAR0	=0x0001
.equ	W5100_GAR1	=0x0002
.equ	W5100_GAR2	=0x0003
.equ	W5100_GAR3	=0x0004
.equ	W5100_SUBR0	=0x0005
.equ	W5100_SUBR1	=0x0006
.equ	W5100_SUBR2	=0x0007
.equ	W5100_SUBR3	=0x0008
.equ	W5100_SHAR0	=0x0009
.equ	W5100_SHAR1	=0x000a
.equ	W5100_SHAR2	=0x000b
.equ	W5100_SHAR3	=0x000c
.equ	W5100_SHAR4	=0x000d
.equ	W5100_SHAR5	=0x000e
.equ	W5100_SIPR0	=0x000f
.equ	W5100_SIPR1	=0x0010
.equ	W5100_SIPR2	=0x0011
.equ	W5100_SIPR3	=0x0012
.equ	W5100_IR	=0x0015
.equ	W5100_IMR	=0x0016
.equ	W5100_RTR0	=0x0017
.equ	W5100_RTR1	=0x0018
.equ	W5100_RCR	=0x0019
.equ	W5100_RMSR	=0x001a
.equ	W5100_TMSR	=0x001b
.equ	W5100_PATR0	=0x001c
.equ	W5100_PATR1	=0x001d
.equ	W5100_PTIMER	=0x0028
.equ	W5100_PMAGIC	=0x0029
.equ	W5100_UIPR0	=0x002a
.equ	W5100_UIPR1	=0x002b
.equ	W5100_UIPR2	=0x002c
.equ	W5100_UIPR3	=0x002d
.equ	W5100_UPORT0	=0x002e
.equ	W5100_UPORT1	=0x002f


.function dfr_ether_init ()
	call	spi_init_master
	ldi	r16, (1 << SPIMODE_BIT_TXRX)
	call	spi_setmode
.endfunction

.function dfr_ether_read ()
	; r27:r26 (X) = 16-bit address => r16
	call	spi_reset_txbuf
	call	spi_reset_rxbuf
	ldi	r16, 0x0f
	call	spi_add_txbuf
	mov	r16, r27
	call	spi_add_txbuf
	mov	r16, r26
	call	spi_add_txbuf
	ldi	r16, 0x00
	call	spi_add_txbuf

	call	spi_start_tx
	call	spi_waittx

	call	spi_lastrx
.endfunction

.function dfr_ether_write ()
	; r27:r26 (X) = 16-bit address, r16 = value
	push	r17
	mov	r17, r16

	call	spi_reset_txbuf
	call	spi_reset_rxbuf
	ldi	r16, 0xf0
	call	spi_add_txbuf
	mov	r16, r27
	call	spi_add_txbuf
	mov	r16, r26
	call	spi_add_txbuf
	mov	r16, r17
	call	spi_add_txbuf

	call	spi_start_tx
	call	spi_waittx

	pop	r17
.endfunction


