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
.equ	PAL_CYC_OUTPUT_START	=175	; 175

; video connection
.equ	VID_PORT		=PORTB
.equ	VID_PIN			=4
.equ	VID_DDR			=DDRB

.equ	SYNC_PORT		=PORTB
.equ	SYNC_PIN		=5
.equ	SYNC_DDR		=DDRB

; video stuff based on resolution
.equ	VRES			=DHEIGHT
.equ	HRES			=(DWIDTH >> 3)
.equ	VSCALE			=1
.equ	DPY_START_RENDER	=(PAL_LINE_MID - ((VRES * (VSCALE + 1)) / 2))
.equ	DPY_STOP_RENDER		=(DPY_START_RENDER + (VRES * (VSCALE + 1)))
.equ	DPY_OUTPUT_DELAY	=PAL_CYC_OUTPUT_START
.equ	DPY_VSYNC_END		=PAL_LINE_STOP_VSYNC
.equ	DPY_LINES_FRAME		=PAL_LINE_FRAME



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

;}}}

.text
.include "atmega1280-imap.inc"

;{{{  interrupt for TIMER1 overflow
VEC_timer1ovf:
	push	r16
	push	r17
	push	r18
	push	r19
	push	r24
	push	r25
	push	XL
	push	XH
	in	r17, SREG
	push	r17

	lds	r17, V_scanline_h		; r17:r16 = V_scanline
	lds	r16, V_scanline_l

	ldi	r19:r18, DPY_VSYNC_END
	cp	r16, r18
	cpc	r17, r19
	brlo	1f				; branch if V_scanline < DPY_VSYNC_END
	brne	2f				; branch if V_scanline != DPY_VSYNC_END
	; assert: V_scanline == DPY_VSYNC_END
	ldi	r19:r18, CYC_H_SYNC
	sts	OCR1AH, r19
	sts	OCR1AL, r18
.L1:
	rjmp	9f
.L2:
	; assert: V_scanline > DPY_VSYNC_END
	ldi	r19:r18, DPY_START_RENDER
	cp	r16, r18
	cpc	r17, r19
	brlo	9f				; branch if V_scanline < DPY_START_RENDER
	brne	4f				; branch if V_scanline != DPY_START_RENDER
	; assert: V_scanline > DPY_START_RENDER
	ldi	r19:r18, DPY_STOP_RENDER
	cp	r16, r18
	cpc	r17, r19
	brlo	5f				; branch if V_scanline < DPY_STOP_RENDER
	; assert: V_scanline >= DPY_STOP_RENDER
	ldi	r19:r18, DPY_LINES_FRAME
	cp	r16, r18
	cpc	r17, r19
	brlo	6f				; branch if V_scanline <= DPY_LINES_FRAME
	breq	6f
	; assert: V_scanline > DPY_LINES_FRAME
	ldi	r19:r18, CYC_V_SYNC
	sts	OCR1AH, r19
	sts	OCR1AL, r18
	clr	r16
	clr	r17
	rjmp	9f


.L9:
	; V_scanline++ and store
	ldi	r19:r18, 1
	add	r16, r18
	adc	r17, r19
	sts	V_scanline_h, r17
	sts	V_scanline_l, r16

	; proceed to get out!

	pop	r17
	out	SREG, r17
	pop	XH
	pop	XL
	pop	r25
	pop	r24
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	reti

;FIXME: after here!


	;{{{  here means that V_scanline >= DPY_LINES_FRAME
	ldi	r19:r18, CYC_V_SYNC		; OCR1A = CYC_V_SYNC
	sts	OCR1AH, r19
	sts	OCR1AL, r18
	
	clr	r17				; V_scanline = 1
	ldi	r16, 1
	sts	V_scanline_h, r17
	sts	V_scanline_l, r16

	rjmp	9f
	;}}}
.L1:
	;{{{  here means that V_scanline < DPY_LINES_FRAME, see if we're in [0..DPY_VSYNC_END]
	ldi	r19:r18, DPY_VSYNC_END
	cp	r16, r18
	cpc	r17, r19

	brlo	2f				; branch if V_scanline < DPY_VSYNC_END
	breq	3f				; branch if V_scanline == DPY_VSYNC_END

	; else in the picture region or end-of-frame
	rjmp	4f
	;}}}
.L2:
	;{{{  here means that V_scanline < DPY_VSYNC_END  (also generic dest for V_scanline++ and out)
	ldi	r19:r18, 1			; V_scanline++
	add	r16, r18
	adc	r17, r19
	sts	V_scanline_h, r17
	sts	V_scanline_l, r16

	rjmp	9f
	;}}}
.L3:
	;{{{  here means that V_scanline == DPY_VSYNC_END
	ldi	r19:r18, CYC_H_SYNC		; OCR1A = CYC_H_SYNC
	sts	OCR1AH, r19
	sts	OCR1AL, r18

	; do V_scanline++ and out
	rjmp	2b

	;}}}
.L4:
	;{{{  here means in the picture region somewhere: r17:r16 = V_scanline
	ldi	r19:r18, DPY_START_RENDER
	cp	r16, r18
	cpc	r17, r19

	brlo	2b				; if V_scanline < DPY_START_RENDER, V_scanline++ and out
	breq	5f				; if V_scanline == DPY_START_RENDER, init for rendering

	ldi	r19:r18, DPY_STOP_RENDER
	cp	r16, r18
	cpc	r17, r19

	brlo	6f				; if V_scanline < DPY_STOP_RENDER, must be visible area
	; else we're past the image.
	rjmp	2b				; just V_scanline++ and out

	;}}}
.L5:
	;{{{  here means that V_scanline == DPY_START_RENDER
	clr	r18				; V_rline = 0
	sts	V_rline_h, r18
	sts	V_rline_l, r18
	ldi	r18, VSCALE			; V_vscale = VSCALE
	sts	V_vscale, r18

	rjmp	2b				; V_scanline++ and out
	;}}}
.L6:
	;{{{  here means that V_scanline is in (DPY_START_RENDER+1)..(DPY_STOP_RENDER-1)
	; wait for the appropriate output delay
	ldi	r18, DPY_OUTPUT_DELAY
	lds	r19, TCNT1L
	sub	r18, r19
	subi	r18, 10
.L60:
	subi	r18, 3
	brcc	60b
	subi	r18, 253
	breq	61f
	dec	r18
	breq	62f
	rjmp	62f
.L61:
	nop
.L62:

	; FIXME: render the line.
	sbi	VID_PORT, VID_PIN
	nop
	nop
	nop
	nop
	nop

	cbi	VID_PORT, VID_PIN

	; update v-scaling
	lds	r18, V_vscale
	cpi	r18, 0
	breq	63f				; branch if V_vscale == 0

	; here means V_vscale != 0
	dec	r18
	sts	V_vscale, r18
	rjmp	64f

.L63:
	; here means V_vscale == 0
	ldi	r18, VSCALE			; V_vscale = VSCALE
	sts	V_vscale, r18

	lds	r25, V_rline_h			; V_rline += HRES
	lds	r24, V_rline_l
	adiw	r25:r24, HRES
	sts	V_rline_h, r25
	sts	V_rline_l, r24
.L64:

	rjmp	2b				; V_scanline++ and out
	;}}}

;}}}

vidhw_setup: ;{{{  sets up video generation stuff
	push	r16
	push	r17

	; set VID and SYNC pins output
	sbi	VID_DDR, VID_PIN
	sbi	SYNC_DDR, SYNC_PIN
	; VID low, SYNC high
	cbi	VID_PORT, VID_PIN
	sbi	SYNC_PORT, SYNC_PIN

	; enable TIMER1 in PPR0
	lds	r16, PRR0
	andi	r16, 0xf7		; clear bit 3 (PRTIM1)
	sts	PRR0, r16

	; ICR1 = PAL_CYC_SCANLINE
	ldi	r17:r16, PAL_CYC_SCANLINE
	sts	ICR1H, r17
	sts	ICR1L, r16

	; OCR1A = CYC_H_SYNC
	ldi	r17:r16, CYC_H_SYNC
	sts	OCR1AH, r17
	sts	OCR1AL, r16

	; clear timer1 value
	clr	r16
	sts	TCNT1H, r16
	sts	TCNT1L, r16

	; setup TIMER1 for fast PWM mode, TOP=ICR1(=PAL_CYC_SCANLINE); OCR1A update at BOTTOM; overflow interrupt at TOP.
	ldi	r16, 0xc2		; COM1A1 | COM1A0 | WGM11
	sts	TCCR1A, r16
	ldi	r16, 0x19		; WGM13 | WGM12 | CS10
	sts	TCCR1B, r16

	; at this point the timer is running
	ldi	r17:r16, (PAL_LINE_FRAME + 1)
	sts	V_scanline_h, r17
	sts	V_scanline_l, r16

	; enable timer interrupt
	ldi	r16, 0x01		; TOIE1
	sts	TIMSK1, r16

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

	rcall	vidhw_setup

	sei				; enable interrupts

loop:
	sleep
	rjmp	loop
