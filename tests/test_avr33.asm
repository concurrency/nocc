;
;	test_avr33.asm -- testing for the ATMEGA2560
;

.mcu	"atmega2560"

.include "atmega1280.inc"

;{{{  assorted constants
; own constants
.equ	DWIDTH	=128
.equ	DHEIGHT	=96

; video generation constants
.equ	CYC_V_SYNC	=940		; (940.6)
.equ	CYC_H_SYNC	=74		; (74.2)

.equ	PAL_TIME_SCANLINE	=64
.equ	PAL_TIME_OUTPUT_START	=11

.equ	PAL_LINE_FRAME		=312
.equ	PAL_LINE_START_VSYNC	=0
.equ	PAL_LINE_STOP_VSYNC	=7
.equ	PAL_LINE_DISPLAY	=260
.equ	PAL_LINE_MID		=156	; (((PAL_LINE_FRAME - PAL_LINE_DISPLAY) / 2) + (PAL_LINE_DISPLAY / 2))

.equ	PAL_CYC_SCANLINE	=1023
.equ	PAL_CYC_COLOUR_START	=85
.equ	PAL_CYC_OUTPUT_START	=178	; 175
.equ	PAL_CYC_VGATE_START	=190	; turn on here

; video connection
.equ	VID_PORT		=PORTD
.equ	VID_PIN			=3
.equ	VID_DDR			=DDRD

; SYNC is OC1A/PB5 (11)
.equ	SYNC_PORT		=PORTB
.equ	SYNC_PIN		=5
.equ	SYNC_DDR		=DDRB

; VGATE IS OC1B/PB6 (12)
.equ	VGATE_PORT		=PORTB
.equ	VGATE_PIN		=6
.equ	VGATE_DDR		=DDRB

; video stuff based on resolution
.equ	VRES			=DHEIGHT
.equ	HRES			=(DWIDTH >> 3)
.equ	VSCALE			=1
.equ	DPY_START_RENDER	=(PAL_LINE_MID - ((VRES * (VSCALE + 1)) / 2))
.equ	DPY_STOP_RENDER		=(DPY_START_RENDER + (VRES * (VSCALE + 1)))
.equ	DPY_VSYNC_END		=PAL_LINE_STOP_VSYNC
.equ	DPY_LINES_FRAME		=PAL_LINE_FRAME

; registers reserved for particular things:
.def	LINE_ADDR_H		=r29		; hijack 'Y' for line output stuff
.def	LINE_ADDR_L		=r28

;}}}

.data
.org	0x400
V_framebuffer: ;{{{  suitably sized b&w framebuffer
	.space	((DWIDTH >> 3) * DHEIGHT)

;}}}
;{{{  video variables
V_scanline:			; 16-bit current scanline
V_scanline_h:	.space 1
V_scanline_l:	.space 1

V_vscale:	.space 1	; 8-bit v-scale value (0..1)

V_roffs:			; 16-bit current render offset into framebuffer
V_roffs_h:	.space 1
V_roffs_l:	.space 1

V_xbleft:	.space 1	; 8-bit X bytes left.
V_xxmask:	.space 1	; mask.

A_vline:			; 16-bit address of scanline handler (NOTE: must be in first 64k)
A_vline_h:	.space 1
A_vline_l:	.space 1

;}}}

.text
.include "atmega1280-imap.inc"

VEC_timer1ovf:	;{{{  interrupt for TIMER1 overflow

	push	r16
	push	r17
	push	r18
	push	r19
	push	ZL
	push	ZH
	in	r16, SREG
	push	r16

	lds	r17, V_scanline_h		; r17:r16 = V_scanline
	lds	r16, V_scanline_l

	lds	ZH, A_vline_h			; Z (r31:r30) = A_vline
	lds	ZL, A_vline_l
	ijmp					; jump to the appropriate part of the handler

;}}}
PR_vsync: ;{{{  rest-of-ISR for vsync lines, also initial case
	ldi	r19:r18, DPY_VSYNC_END
	cp	r16, r18
	cpc	r17, r19
	breq	1f			; branch if V_scanline == DPY_VSYNC_END
	ldi	r19:r18, DPY_LINES_FRAME
	cp	r16, r18
	cpc	r17, r19
	brlo	2f			; branch if V_scanline <= DPY_LINES_FRAME  (normal vsync line)
	breq	2f
	; assert: V_scanline > DPY_LINES_FRAME
	ldi	r19:r18, CYC_V_SYNC	; setup for vsync pulses
	sts	OCR1AH, r19
	sts	OCR1AL, r18
	clr	r16			; V_scanline = 0
	clr	r17

	; DEBUG updates for next frame
	lds	r18, V_xxmask
	inc	r18
	sts	V_xxmask, r18

	rjmp	2f
.L1:	; assert: V_scanline == DPY_VSYNC_END
	ldi	r19:r18, CYC_H_SYNC	; setup for hsync pulses
	sts	OCR1AH, r19
	sts	OCR1AL, r18
	; next-time, in blanking portion
	ldi	r19, hi(PR_tblank >> 1)
	ldi	r18, lo(PR_tblank >> 1)
	sts	A_vline_h, r19
	sts	A_vline_l, r18
.L2:
	rjmp	ENDVEC_timer1ovf


;}}}
PR_tblank: ;{{{  rest-of-ISR for top blanking lines
	ldi	r19:r18, DPY_START_RENDER-1
	cp	r16, r18
	cpc	r17, r19
	breq	1f			; branch if V_scanline == DPY_START_RENDER-1
	rjmp	9f
.L1:
	; prepare for active lines next
	ldi	r19, hi(PR_aline >> 1)
	ldi	r18, lo(PR_aline >> 1)
	sts	A_vline_h, r19
	sts	A_vline_l, r18

	ldi	r19:r18, 0		; set V_roffs = 0
	sts	V_roffs_h, r19
	sts	V_roffs_l, r18

	ldi	r18, VSCALE		; set V_vscale = VSCALE
	sts	V_vscale, r18
.L9:
	rjmp	ENDVEC_timer1ovf


;}}}
PR_aline: ;{{{  rest-of-ISR for active lines (from DPY_START_RENDER .. DPY_STOP_RENDER)
	ldi	r19:r18, DPY_STOP_RENDER
	cp	r16, r18
	cpc	r17, r19
	breq	7f			; branch if V_scanline == DPY_STOP_RENDER  (and not rendering this line)

	; okis, rendering this one.
	ldi	r19:r18, 157
	cp	r16, r18
	cpc	r17, r19

	brsh	1f
	rjmp	2f
.L7:
	rjmp	8f
.L1:
	;{{{  wait for PAL_CYC_OUTPUT_START cycles (based on TIMER1 value)
	ldi	r18, PAL_CYC_OUTPUT_START
	lds	r19, TCNT1L
	sub	r18, r19
	subi	r18, 3
.L10:
	subi	r18, 3
	brcc	10b
	subi	r18, 253
	breq	11f
	dec	r18
	breq	12f
	rjmp	12f
.L11:
	nop
.L12:
	;}}}
	;{{{  TEST

	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop

	nop	; 2 blank pixels
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

	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop

	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop

	nop	; back to /6
	nop

	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	nop

	nop	; back to /6
	nop
	nop
	nop

	sbi	VID_PORT, VID_PIN
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	nop

	sbi	VID_PORT, VID_PIN
	nop
	cbi	VID_PORT, VID_PIN
	nop
	sbi	VID_PORT, VID_PIN
	nop
	cbi	VID_PORT, VID_PIN
	nop
	sbi	VID_PORT, VID_PIN
	nop
	cbi	VID_PORT, VID_PIN
	nop
	sbi	VID_PORT, VID_PIN
	nop
	cbi	VID_PORT, VID_PIN
	nop

	nop	; back to /6
	nop

	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN

	nop	; back to /6
	nop

	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	nop
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	nop
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	nop
	sbi	VID_PORT, VID_PIN
	cbi	VID_PORT, VID_PIN
	nop

	sbi	VID_PORT, VID_PIN
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop
	sbi	VID_PORT, VID_PIN
	nop
	nop
	cbi	VID_PORT, VID_PIN
	nop

	nop	; back to /6
	nop

	;}}}
	rjmp	9f
.L2:
	; setup USART in SPI mode to blat data out
	ldi	r18, 8
	sts	V_xbleft, r18

	; setup 'Y' (r29:r28) to be address of line data start
	lds	r29, V_roffs_h
	ldi	r18, 4			; XXX: must match MSB of framebuffer address
	add	r29, r18
	lds	r28, V_roffs_l

	ldi	r18, 0xc0		; MSPIM mode
	sts	UCSR1C, r18
	ldi	r18, 0x28		; UDRIE, TXEN1
	sts	UCSR1B, r18
	ldi	r19:r18, 2		; baud rate
	sts	UBRR1H, r19
	sts	UBRR1L, r18
	; transmitter up-and-running, UDRE interrupt will be ready to fire

	rjmp	9f
.L8:
	; not rendering anymore, go into bottom blanking

	ldi	r19, hi(PR_bblank >> 1)
	ldi	r18, lo(PR_bblank >> 1)
	sts	A_vline_h, r19
	sts	A_vline_l, r18
	rjmp	99f
.L9:
	;{{{  update V_vscale and V_roffs
	lds	r18, V_vscale
	cpi	r18, 0
	breq	93f
	dec	r18
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	rjmp	94f
.L93:
	ldi	r18, VSCALE
	lds	r25, V_roffs_h
	lds	r24, V_roffs_l
	adiw	r25:r24, HRES		; V_roffs += HRES
	sts	V_roffs_h, r25
	sts	V_roffs_l, r24
.L94:
	sts	V_vscale, r18

	;}}}
.L99:
	rjmp	ENDVEC_timer1ovf


;}}}
PR_bblank: ;{{{  rest-of-ISR for bottom blanking lines
	ldi	r19:r18, DPY_LINES_FRAME
	cp	r16, r18
	cpc	r17, r19
	breq	1f
	rjmp	9f
.L1:
	; set to vsync for next.
	ldi	r19, hi(PR_vsync >> 1)
	ldi	r18, lo(PR_vsync >> 1)
	sts	A_vline_h, r19
	sts	A_vline_l, r18
.L9:
	rjmp	ENDVEC_timer1ovf


;}}}
ENDVEC_timer1ovf:  ;{{{  end-of-ISR code
	; V_scanline++ and store
	ldi	r19:r18, 1
	add	r16, r18
	adc	r17, r19
	sts	V_scanline_h, r17
	sts	V_scanline_l, r16

	pop	r16
	out	SREG, r16
	pop	ZH
	pop	ZL
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	reti

;}}}

VEC_usart1udre:	;{{{  interrupt for USART1 data register empty
	push	r16
	in	r16, SREG
	push	r16

	; more to go
	ld	r16, Y+
	sts	UDR1, r16

	lds	r16, V_xbleft
	dec	r16
	sts	V_xbleft, r16
	brne	1f

	ldi	r16, 0x48		; UDRE interrupt off, transmitter and TX-complete interrupts still enabled.
	sts	UCSR1B, r16
.L1:

	pop	r16
	out	SREG, r16
	pop	r16
	reti

;}}}
VEC_usart1tx:	;{{{  interrupt for USART1 TX complete
	push	r16
	in	r16, SREG
	push	r16
	push	r17

	ldi	r16, 0x00		; interrupt and transmit disable
	sts	UCSR1B, r16
	sts	UBRR1H, r16		; set baud rate to zero
	sts	UBRR1L, r16
	cbi	PORTD, 3		; PD3 (TXD1) low

	pop	r17
	pop	r16
	out	SREG, r16
	pop	r16
	reti
;}}}


vidhw_setup: ;{{{  sets up video generation stuff
	push	r16
	push	r17

	; set VGATE, VID, SYNC pins output
	sbi	VGATE_DDR, VGATE_PIN
	sbi	VID_DDR, VID_PIN
	sbi	SYNC_DDR, SYNC_PIN
	; VGATE, VID, SYNC low
	cbi	VGATE_PORT, VGATE_PIN
	cbi	VID_PORT, VID_PIN
	cbi	SYNC_PORT, SYNC_PIN

	; DEBUG:
	sbi	DDRB, 7			; PB7 (13) == DEBUG
	cbi	PORTB, 7

	; enable TIMER1 in PRR0
	lds	r16, PRR0
	andi	r16, ~PRR0_BIT_PRTIM1	; clear bit 3 (PRTIM1)
	sts	PRR0, r16

	; enable USART1 in PRR1
	lds	r16, PRR1
	andi	r16, ~PRR1_BIT_PRUSART1	; clear bit 0 (PRUSART1)
	sts	PRR1, r16

	; setup USART1-SPI hardware
	;sbi	DDRD, 3			; PD3/TXD1 (18) as output
	cbi	DDRD, 2			; PD2/RXD1 (19) as input  [unused]
	sbi	DDRD, 5			; PD5/XCK1 as output  [not routed]

	;cbi	PORTD, 3		; TXD1 low

	clr	r16
	sts	V_xbleft, r16
	sts	V_xxmask, r16

	ldi	r17:r16, 0		; baud
	sts	UBRR1H, r17
	sts	UBRR1L, r16
	; NOTE: USART transmitter setup and enabled at line start point.

	; ICR1 = PAL_CYC_SCANLINE
	ldi	r17:r16, PAL_CYC_SCANLINE
	sts	ICR1H, r17
	sts	ICR1L, r16

	; OCR1A = CYC_H_SYNC
	ldi	r17:r16, CYC_H_SYNC
	sts	OCR1AH, r17
	sts	OCR1AL, r16

	; OCR1B = PAL_CYC_VGATE_START
	ldi	r17:r16, PAL_CYC_VGATE_START
	sts	OCR1BH, r17
	sts	OCR1BL, r16

	; clear timer1 value
	clr	r16
	sts	TCNT1H, r16
	sts	TCNT1L, r16

	; set scanline and vline-address
	ldi	r17:r16, (PAL_LINE_FRAME + 1)
	sts	V_scanline_h, r17
	sts	V_scanline_l, r16

	ldi	r17, hi(PR_vsync >> 1)
	ldi	r16, lo(PR_vsync >> 1)
	sts	A_vline_h, r17
	sts	A_vline_l, r16

	; setup TIMER1 for fast PWM mode, TOP=ICR1(=PAL_CYC_SCANLINE); OCR1A update at BOTTOM; overflow interrupt at TOP.
	; attach OC1A inverted mode, OC1B inverted mode
	ldi	r16, (TCCR1A_BIT_COM1A1 | TCCR1A_BIT_COM1A0 | TCCR1A_BIT_COM1B1 | TCCR1A_BIT_COM1B0 | TCCR1A_BIT_WGM11)
	sts	TCCR1A, r16
	ldi	r16, (TCCR1B_BIT_WGM13 | TCCR1B_BIT_WGM12 | TCCR1B_BIT_CS10)
	sts	TCCR1B, r16

	; at this point the timer is running

	; enable timer interrupts: overflow and compare-match-C
	ldi	r16, TIMSK1_BIT_TOIE1 | TIMSK1_BIT_OCIE1C
	sts	TIMSK1, r16

	pop	r17
	pop	r16
	ret
;}}}
fb_clear: ;{{{  clears the framebuffer
	push	r16
	push	r17
	push	r18
	push	r19
	push	r24
	push	r25
	push	XL
	push	XH

	ldi	XH:XL, V_framebuffer
	ldi	r25:r24, (HRES * VRES)
	clr	r16
.L1:
	st	X+, r16
	sbiw	r25:r24, 1
	brne	1b

	pop	XH
	pop	XL
	pop	r25
	pop	r24
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	ret
;}}}


VEC_reset:
	cli				; disable interrupts
	ldi	r16, hi(RAMEND)		; stack-pointer SPH:SPL = RAMEND
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16

	rcall	fb_clear
	rcall	vidhw_setup

	sei				; enable interrupts

loop:
	sleep
	rjmp	loop
