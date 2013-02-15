;
;	dfrobot-arduino-ether.inc -- code to interface with the DFRobot ethernet shield (Wiznet W5100)
;	Copyright (C) 2013, Fred Barnes, University of Kent  <frmb@kent.ac.uk>
;
;	constants mostly extracted from W5100 datasheet
;

.include "atmega-spi.inc"

.equ	W5100_MR	=0x0000
.equ	W5100_MR_BIT_RST	=7		; software reset, cleared after
.equ	W5100_MR_BIT_PB		=4		; ping block
.equ	W5100_MR_BIT_PPPOE	=3		; PPPoE mode
.equ	W5100_MR_BIT_AI		=1		; address auto-increment in indirect bus i/f
.equ	W5100_MR_BIT_IND	=0		; indirect bus i/f mode
.equ	W5100_GAR0	=0x0001			; gateway IP address
.equ	W5100_GAR1	=0x0002
.equ	W5100_GAR2	=0x0003
.equ	W5100_GAR3	=0x0004
.equ	W5100_SUBR0	=0x0005			; subnet mask
.equ	W5100_SUBR1	=0x0006
.equ	W5100_SUBR2	=0x0007
.equ	W5100_SUBR3	=0x0008
.equ	W5100_SHAR0	=0x0009			; source hardware address
.equ	W5100_SHAR1	=0x000a
.equ	W5100_SHAR2	=0x000b
.equ	W5100_SHAR3	=0x000c
.equ	W5100_SHAR4	=0x000d
.equ	W5100_SHAR5	=0x000e
.equ	W5100_SIPR0	=0x000f			; source IP address
.equ	W5100_SIPR1	=0x0010
.equ	W5100_SIPR2	=0x0011
.equ	W5100_SIPR3	=0x0012
.equ	W5100_IR	=0x0015
.equ	W5100_IR_BIT_CONFLICT	=7		; IP conflict (cleared by writing 1)
.equ	W5100_IR_BIT_UNREACH	=6		; destination unreachable (cleared by writing 1)
.equ	W5100_IR_BIT_PPPOE	=5		; PPPoE connection close (cleared by writing 1)
.equ	W5100_IR_BIT_S3_INT	=3		; socket 3 interrupt (cleared by writing S3_IR)
.equ	W5100_IR_BIT_S3_INT	=2		; socket 3 interrupt (cleared by writing S2_IR)
.equ	W5100_IR_BIT_S3_INT	=1		; socket 3 interrupt (cleared by writing S1_IR)
.equ	W5100_IR_BIT_S3_INT	=0		; socket 3 interrupt (cleared by writing S0_IR)
.equ	W5100_IMR	=0x0016
.equ	W5100_IMR_BIT_IM_IR7	=7		; IP conflict enable
.equ	W5100_IMR_BIT_IM_IR6	=6		; destination unreachable enable
.equ	W5100_IMR_BIT_IM_IR5	=5		; PPPoE close enable
.equ	W5100_IMR_BIT_IM_IR3	=3		; socket 3 interrupt enable
.equ	W5100_IMR_BIT_IM_IR2	=2		; socket 3 interrupt enable
.equ	W5100_IMR_BIT_IM_IR1	=1		; socket 3 interrupt enable
.equ	W5100_IMR_BIT_IM_IR0	=0		; socket 3 interrupt enable
.equ	W5100_RTR0	=0x0017			; retry time-value (in 100us units), MSB first
.equ	W5100_RTR1	=0x0018
.equ	W5100_RCR	=0x0019			; retry count (before timeout interrupt)
.equ	W5100_RMSR	=0x001a			; RX memory size, formed as 4*2 bits for each socket
.equ	W5100_RMSR_BIT_1KB	=0x00		; 1 KB receive buffer
.equ	W5100_RMSR_BIT_2KB	=0x01		; 2 KB receive buffer (default)
.equ	W5100_RMSR_BIT_4KB	=0x02		; 4 KB receive buffer
.equ	W5100_RMSR_BIT_8KB	=0x03		; 8 KB receive buffer (all of it)
.equ	W5100_TMSR	=0x001b			; TX memory size, formed as 4*2 bits for each socket
.equ	W5100_TMSR_BIT_1KB	=0x00		; 1 KB transmit buffer
.equ	W5100_TMSR_BIT_2KB	=0x01		; 2 KB transmit buffer (default)
.equ	W5100_TMSR_BIT_4KB	=0x02		; 4 KB transmit buffer
.equ	W5100_TMSR_BIT_8KB	=0x03		; 8 KB transmit buffer (all of it)
.equ	W5100_PATR0	=0x001c			; PPPoE authentication mode, MSB first
.equ	W5100_PATR1	=0x001d
.equ	W5100_PATR_NONE		=0x0000
.equ	W5100_PATR_PAP		=0xc023
.equ	W5100_PATR_CHAP		=0xc223
.equ	W5100_PTIMER	=0x0028			; PPP LCP request timer (in ~25ms units)
.equ	W5100_PMAGIC	=0x0029			; PPP LCP magic number
.equ	W5100_UIPR0	=0x002a			; unreachable IP address (set by ICMP response)
.equ	W5100_UIPR1	=0x002b
.equ	W5100_UIPR2	=0x002c
.equ	W5100_UIPR3	=0x002d
.equ	W5100_UPORT0	=0x002e			; unreachable IP port (set by ICMP response)
.equ	W5100_UPORT1	=0x002f

.equ	W5100_SOCK0_BASE	=0x0400		; socket 0 register base
.equ	W5100_SOCK1_BASE	=0x0500		; socket 1 register base
.equ	W5100_SOCK2_BASE	=0x0600		; socket 2 register base
.equ	W5100_SOCK3_BASE	=0x0700		; socket 3 register base

.equ	W5100_TXMEM_BASE	=0x4000		; TX memory base
.equ	W5100_RXMEM_BASE	=0x6000		; RX memory base

.equ	W5100_S_MR	=0x0000		; mode register
.equ	W5100_S_CR	=0x0001		; command register
.equ	W5100_S_IR	=0x0002		; interrupt register
.equ	W5100_S_SR	=0x0003		; status register
.equ	W5100_S_PORT0	=0x0004		; source port, MSB first
.equ	W5100_S_PORT1	=0x0005
.equ	W5100_S_DHAR0	=0x0006		; destination hardware address
.equ	W5100_S_DHAR1	=0x0007
.equ	W5100_S_DHAR2	=0x0008
.equ	W5100_S_DHAR3	=0x0009
.equ	W5100_S_DHAR4	=0x000a
.equ	W5100_S_DHAR5	=0x000b
.equ	W5100_S_DIPR0	=0x000c		; destination IP address
.equ	W5100_S_DIPR1	=0x000d
.equ	W5100_S_DIPR2	=0x000e
.equ	W5100_S_DIPR3	=0x000f
.equ	W5100_S_DPORT0	=0x0010		; destination port
.equ	W5100_S_DPORT1	=0x0011
.equ	W5100_S_MSSR0	=0x0012		; maximum segment size
.equ	W5100_S_MSSR1	=0x0013
.equ	W5100_S_PROTO	=0x0014		; protocol in IP raw mode
.equ	W5100_S_TOS	=0x0015		; IP TOS
.equ	W5100_S_TTL	=0x0016		; IP TTL
.equ	W5100_S_TX_FSR0	=0x0020		; TX free size, MSB first
.equ	W5100_S_TX_FSR1	=0x0021
.equ	W5100_S_TX_RD0	=0x0022		; TX read pointer
.equ	W5100_S_TX_RD1	=0x0023
.equ	W5100_S_TX_WR0	=0x0024		; TX write pointer
.equ	W5100_S_TX_WR1	=0x0025
.equ	W5100_S_RX_RSR0	=0x0026		; RX received size
.equ	W5100_S_RX_RSR1	=0x0027
.equ	W5100_S_RX_RD0	=0x0028		; RX read pointer
.equ	W5100_S_RX_RD1	=0x0029


.equ	DFRMODE_BIT_WRITE	=0
.equ	DFRMODE_BIT_READ	=1

.data
V_dfr_mode:
	.space	1

.text


.function dfr_ether_init ()		;{{{  initialise SPI for W5100 comms
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


;}}}
.function dfr_ether_txcheck ()		;{{{  called from inside an interrupt handler to check data I/O
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


;}}}
.function dfr_ether_read ()		;{{{  read W5100 register r27:r26 => r16
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


;}}}
.function dfr_ether_write ()		;{{{  write W5100 register r27:r26 <= r16
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


;}}}
.function dfr_ether_reset ()		;{{{  reset W5100
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


;}}}

.function dfr_ether_write4code ()	;{{{  write 4 W5100 registers from r27:r26 from program-memory r31:r30
	; r27:r26 (X) = 16-bit address
	; r31:r30 (Z) = flash address where data starts
	push	r31
	push	r30
	push	r27
	push	r26
	push	r16

	lpm	r16, Z+
	call	dfr_ether_write

	adiw	r26, 1
	lpm	r16, Z+
	call	dfr_ether_write

	adiw	r26, 1
	lpm	r16, Z+
	call	dfr_ether_write

	adiw	r26, 1
	lpm	r16, Z+
	call	dfr_ether_write

	pop	r16
	pop	r26
	pop	r27
	pop	r30
	pop	r31
.endfunction


;}}}
.function dfr_ether_write6code ()	;{{{  write 6 W5100 registers from r27:r26 from program-memory r31:r30
	; r27:r26 (X) = 16-bit address
	; r31:r30 (Z) = flash address where data starts
	push	r31
	push	r30
	push	r27
	push	r26
	push	r16

	lpm	r16, Z+
	call	dfr_ether_write

	adiw	r26, 1
	lpm	r16, Z+
	call	dfr_ether_write

	adiw	r26, 1
	lpm	r16, Z+
	call	dfr_ether_write

	adiw	r26, 1
	lpm	r16, Z+
	call	dfr_ether_write

	adiw	r26, 1
	lpm	r16, Z+
	call	dfr_ether_write

	adiw	r26, 1
	lpm	r16, Z+
	call	dfr_ether_write

	pop	r16
	pop	r26
	pop	r27
	pop	r30
	pop	r31
.endfunction


;}}}


