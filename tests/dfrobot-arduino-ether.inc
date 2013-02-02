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


.equ	DFRMODE_BIT_WRITE	=0
.equ	DFRMODE_BIT_READ	=1

.data
V_dfr_mode:
	.space	1

.text


.function dfr_ether_init ()
	push	r27
	push	r26
	push	r16

	ldi	r27:r26, V_dfr_mode
	clr	r16
	st	X, r16			; V_dfr_mode = 0

	call	spi_init_master
	ldi	r27, hi(lab_dfr_ether_txcheck >> 1)
	ldi	r26, lo(lab_dfr_ether_txcheck >> 1)
	call	spi_settxcheck
	ldi	r16, (1 << SPIMODE_BIT_TXRX) | (1 << SPIMODE_BIT_TXRXCHK)
	call	spi_setmode

	pop	r16
	pop	r26
	pop	r27
.endfunction


.function dfr_ether_txcheck ()
lab_dfr_ether_txcheck:
	push	r27
	push	r26
	push	r18

	; NOTE: this is called from an interrupt handler (indirectly)
	; Inputs:
	;	r16 = received byte
	;	r17 = txptr (counter for transmit/receive)
	; Output:
	;	r16 = {0 = fine, next-or-end, 1 = abort}
	;

	cpi	r17, 0x03
	brlo	5f

	; 4th byte, if read, response, else should be 0x03
	ldi	r27:r26, V_dfr_mode
	ld	r18, X
	sbrc	r18, DFRMODE_BIT_WRITE
	rjmp	5f				; write bit set, so check equality
	sbrs	r18, DFRMODE_BIT_READ
	rjmp	7f				; read bit not set, so fail

	; reading, so accept anything
	rjmp	8f

.L5:
	; byte 0-2, so response should match
	cp	r16, r17
	breq	8f

	; no match, so fail
.L7:
	ldi	r16, 1
	rjmp	9f
.L8:
	ldi	r16, 0
.L9:

	pop	r18
	pop	r26
	pop	r27
.endfunction

.function dfr_ether_read ()
	; r27:r26 (X) = 16-bit address => r16
	push	r29
	push	r28
	push	r17

	ldi	r29:r28, V_dfr_mode
	ldi	r17, (1 << DFRMODE_BIT_READ)
	st	Y, r17

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

.L0:
	call	spi_start_tx
	call	spi_waittx
	cpi	r16, 0
	breq	3f

	; failed, retry
	call	spi_restart_txrx
	rjmp	0b

.L3:
	call	spi_lastrx

	ldi	r29:r28, V_dfr_mode
	clr	r17
	st	Y, r17

	pop	r17
	pop	r28
	pop	r29
.endfunction

.function dfr_ether_write ()
	; r27:r26 (X) = 16-bit address, r16 = value
	push	r29
	push	r28
	push	r17

	ldi	r29:r28, V_dfr_mode
	ldi	r17, (1 << DFRMODE_BIT_WRITE)
	st	Y, r17

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

.L0:
	call	spi_start_tx
	call	spi_waittx
	cpi	r16, 0
	breq	3f

	; failed, retry
	call	spi_restart_txrx
	rjmp	0b

.L3:
	ldi	r29:r28, V_dfr_mode
	clr	r17
	st	Y, r17

	pop	r17
	pop	r28
	pop	r29
.endfunction

.function dfr_ether_reset ()
	push	r27
	push	r26
	push	r16

	ldi	r27:r26, W5100_MR
	ldi	r16, 0x80
	call	dfr_ether_write

	; now, read the mode register until the reset bit is cleared
.L0:
	ldi	r27:r26, W5100_MR
	call	dfr_ether_read
	sbrc	r16, 7				; skip if reset bit (0x80) is clear
	rjmp	0b

	pop	r16
	pop	r26
	pop	r27
.endfunction



