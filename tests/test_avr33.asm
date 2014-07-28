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
.equ	PAL_CYC_VGATE_START	=195	; turn on here

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
.def	UDRE_ISR_TMP		=r2
.def	UDRE_ISR_COUNT		=r3

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
	breq	8f			; branch if V_scanline == DPY_STOP_RENDER  (and not rendering this line)

	; okis, rendering this one.

	; setup USART in SPI mode to blat data out
	ldi	r18, HRES
	mov	r3, r18

	; setup 'Y' (r29:r28) to be address of line data start
	lds	r29, V_roffs_h
	ldi	r18, 4			; XXX: must match MSB of framebuffer address
	add	r29, r18
	lds	r28, V_roffs_l

	ldi	r18, 0xc0		; MSPIM mode
	sts	UCSR1C, r18
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
	ldi	r18, 0x60		; TXC1, UDRE1
	sts	UCSR1A, r18		; clear any pending interrupts
	ldi	r18, 0x68		; UDRIE, TXCIE, TXEN1
	sts	UCSR1B, r18
	ldi	r19:r18, 2		; baud rate
	sts	UBRR1H, r19
	sts	UBRR1L, r18
	; transmitter up-and-running, UDRE interrupt will be ready to fire
	ldi	r18, 0x00
	;ld	r18, Y+
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	sts	UDR1, r18

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
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	dec	r18
	rjmp	94f
.L93:
	ldi	r18, VSCALE
	lds	r25, V_roffs_h
	lds	r24, V_roffs_l
	adiw	r25:r24, HRES		; V_roffs += HRES
	sts	V_roffs_h, r25
	sts	V_roffs_l, r24
	nop
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

	dec	r3
	brne	1f

	ldi	r16, 0x40		; UDRE interrupt off, disable transmitter (TX-complete interrupt still enabled).
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
	nop
	nop
	nop
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
	mov	r3, r16
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
fb_pattern: ;{{{  fills the framebuffer with a pattern
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
	add	r16, XL
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
fb_setpixel: ;{{{  sets a pixel at r16,17 to r18 (0 or 1)
	push	r0
	push	r1
	push	r16
	push	r17
	push	r18
	push	r19
	push	r20
	push	XL
	push	XH

	ldi	r19, HRES
	ldi	r20, hi(V_framebuffer)
	mul	r17, r19		; result left in r1:r0
	mov	r19, r16
	lsr	r19
	lsr	r19
	lsr	r19			; r19 = X / 8
	add	r0, r19
	adc	r1, r20			; r1:r0 = framebuffer offset
	mov	r19, r16
	andi	r19, 0x07		; r19 = X % 8
	inc	r19
	ldi	r20, 0x80
.L1:
	dec	r19
	breq	2f
	lsr	r20
	rjmp	1b
.L2:
	; r20 has the appropriate bit
	mov	XH, r1
	mov	XL, r0
	ld	r19, X
	com	r20
	and	r19, r20
	com	r20
	sbrc	r18, 0
	or	r19, r20

	st	X, r19

	pop	XH
	pop	XL
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	pop	r1
	pop	r0
	ret
;}}}
fb_xsetpixel: ;{{{  sets a pixel at r16,r17 (always sets)
	push	r0
	push	r1
	push	r18
	push	r19
	push	r20
	push	XL
	push	XH

	ldi	r19, HRES
	ldi	r20, hi(V_framebuffer)
	mul	r17, r19		; result of Y*HRES left in r1:r0

	mov	r19, r16
	lsr	r19
	lsr	r19
	lsr	r19			; r19 = X / 8
	add	r0, r19
	adc	r1, r20			; r1:r0 = framebuffer offset

	mov	r19, r16
	andi	r19, 0x07		; r19 = X % 8
	inc	r19
	ldi	r20, 0x80
.L1:
	dec	r19
	breq	2f
	lsr	r20
	rjmp	1b
.L2:
	; r20 has the appropriate bit
	mov	XH, r1
	mov	XL, r0
	ld	r19, X
	or	r19, r20
	st	X, r19

	pop	XH
	pop	XL
	pop	r20
	pop	r19
	pop	r18
	pop	r1
	pop	r0
	ret

;}}}
fb_hline: ;{{{  draw horizontal line from (r16,r17) for r18
	push	r16
	push	r18

.L0:
	rcall	fb_xsetpixel
	inc	r16
	dec	r18
	brne	0b

	pop	r18
	pop	r16
	ret


;}}}
fb_vline: ;{{{  draw vertical line from (r16,r17) for r18
	push	r17
	push	r18

.L0:
	rcall	fb_xsetpixel
	inc	r17
	dec	r18
	brne	0b

	pop	r18
	pop	r17
	ret


;}}}
fb_setbyte: ;{{{  write the 8-bits in r18 to the framebuffer at position (r16*8, r17)
	push	r0
	push	r1
	push	r16
	push	r17
	push	r18
	push	r19
	push	r20
	push	XL
	push	XH

	ldi	r19, HRES
	ldi	r20, hi(V_framebuffer)
	mul	r17, r19		; result of Y*HRES left in r1:r0

	add	r0, r16			; r1:r0 = framebuffer offset
	adc	r1, r20

	mov	XH, r1
	mov	XL, r0
	st	X, r18

	pop	XH
	pop	XL
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	pop	r1
	pop	r0
	ret


;}}}
fb_writechar_8x8: ;{{{  writes the ASCII char in r18 to the framebuffer at position (r16*8, r17) using 8x8 font;  updates r16
	push	ZL
	push	ZH


	pop	ZH
	pop	ZL
	ret


;}}}

vid_waitbot: ;{{{  waits for the render to reach bottom of visible area (so we can start updating things)
	push	r16
	push	r17
	push	r18
	push	r19

	ldi	r19:r18, DPY_STOP_RENDER
.L0:
	lds	r17, V_scanline_h
	lds	r16, V_scanline_l
	cp	r16, r18
	cpc	r17, r19
	breq	1f
	rjmp	0b
.L1:

	pop	r19
	pop	r18
	pop	r17
	pop	r16
	ret
;}}}
vid_waitbot1: ;{{{  waits for the render to reach just after bottom of visible area (forces delay with the above)
	push	r16
	push	r17
	push	r18
	push	r19

	ldi	r19:r18, DPY_STOP_RENDER+1
.L0:
	lds	r17, V_scanline_h
	lds	r16, V_scanline_l
	cp	r16, r18
	cpc	r17, r19
	breq	1f
	rjmp	0b
.L1:

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

	rcall	fb_pattern
	rcall	vidhw_setup

	sei				; enable interrupts

prg_code:
	ldi	r16, 199		; 4 seconds
.L0:
	rcall	vid_waitbot1
	rcall	vid_waitbot
	dec	r16
	brne	0b

	ldi	r20, 1
	ldi	r21, 1
.L1:
	rcall	fb_clear
	ldi	r16, 0
	ldi	r17, 0
	ldi	r18, 128
	rcall	fb_hline
	ldi	r18, 96
	rcall	fb_vline
	ldi	r17, 95
	ldi	r18, 128
	rcall	fb_hline
	ldi	r16, 127
	ldi	r17, 0
	ldi	r18, 96
	rcall	fb_vline

	ldi	r16, 2
	ldi	r17, 0
	ldi	r18, 96
	rcall	fb_vline
	ldi	r16, 4
	rcall	fb_vline
	ldi	r16, 7
	rcall	fb_vline
	ldi	r16, 11
	rcall	fb_vline

	; moving portions
	ldi	r16, 0
	mov	r17, r21
	ldi	r18, 128
	rcall	fb_hline

	mov	r16, r20
	ldi	r17, 0
	ldi	r18, 96
	rcall	fb_vline

	inc	r21
	cpi	r21, 96
	brlo	2f
	ldi	r21, 1
.L2:

	inc	r20
	cpi	r20, 128
	brlo	3f
	ldi	r20, 1
.L3:

	rcall	vid_waitbot
	rjmp	1b

loop:
	;sleep
	nop
	nop
	nop
	nop
	rjmp	loop
