;
;	test_avr33.asm -- testing for the ATMEGA2560: PAL display generation [for fun :)]
;	Copyright (C) 2014, Fred Barnes, University of Kent <frmb@kent.ac.uk>.
;

.mcu	"atmega2560"

.include "atmega1280.inc"

; GROT: this code is hardwired to a resolution of 128x96.
; Source: a lot of the initial how-to I got from the Arduino TVout library [Myles Metzer, 2010].
; Source: assorted inspiration / code observation from 'craft' [Linus Åkesson, 2008].

;{{{  reserved registers (and calling convention)

.def	UDRE_ISR_TMP		=r2		; for the UDRE interrupt routine: this needs to fire every 48 cycles or so to keep up when rendering
.def	UDRE_ISR_COUNT		=r3

.def	LINE_REGION		=r4		; these determine the vertical position in rendering
.def	LINE_COUNT		=r5

.def	LINE_ADDR_L		=r28		; use 'Y' exclusively for framebuffer line address when rendering
.def	LINE_ADDR_H		=r29

;}}}
;{{{  assorted constants
; own constants
.equ	DWIDTH	=128
.equ	DHEIGHT	=96

.equ	VRES	=96
.equ	HRES	=16			; bytes worth of framebuffer

; video generation constants
.equ	CYC_V_SYNC	=940		; (940.6)
.equ	CYC_H_SYNC	=74		; (74.2)

.equ	PAL_CYC_SCANLINE	=1023	; 64us lines
.equ	PAL_CYC_OUTPUT_START	=80
.equ	PAL_CYC_VGATE_START	=195	; turn on here (OC1B)

; NOTE: r4 determines vertical region (0=active, 1=bblank, 2=vsync, 3=tblank),
;       r5 is the particular line in that region.
.equ	PAL_ACTIVE_LINES	=192
.equ	PAL_BBLANK_LINES	=60
.equ	PAL_VSYNC_LINES		=9
.equ	PAL_TBLANK_LINES	=51

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

;}}}

.data
.org	0x400
V_framebuffer: ;{{{  suitably sized b&w framebuffer
	.space	(HRES * VRES)

;}}}
;{{{  video variables
; Meh, all gone!

;}}}

.text
.include "atmega1280-imap.inc"

VEC_timer1ovf:	;{{{  interrupt for TIMER1 overflow
	; Note: this is triggered every 64us or so.  Exclusive so can use 'r2' (UDRE temporary).
	in	r2, SREG			; save SREG							[1]
	push	r16				;								[3]
	push	r17				;								[5]

	dec	r5				; next line please						[6]
	mov	r16, r5				;								[7]
	cpi	r16, 0xff			; see if we rolled over						[8]
	brne	5f				; straightforward: nothing special here				[9+1]
	; rolled over, entering new region
	mov	r16, r4				; current V region						[10]
	cpi	r16, 0				;								[11]
	breq	1f				;								[12+1]
	cpi	r16, 1				;								[13]
	breq	2f				;								[14+1]
	cpi	r16, 2				;								[15]
	breq	3f				;								[16+1]
	; else must be 3 (top-blanking region), moving into active region
	ldi	r16, PAL_ACTIVE_LINES-1		;								[17]
	mov	r5, r16				;								[18]
	clr	r4				;								[19]
	rjmp	6f				;								[21]

.L1:	; leaving active region into bottom-blanking								-> [13]
	ldi	r16, PAL_BBLANK_LINES-1		;								[14]
	mov	r5, r16				;								[15]
	inc	r4				;								[16]
	; better make sure we turn off OC1C here
	ldi	r16, TIMSK1_BIT_TOIE1		;								[17]
	sts	TIMSK1, r16			; turn off OC1C interrupt					[19]
	ldi	r16, TIFR1_BIT_OCF1C		;								[20]
	sts	TIFR1, r16			; clear any pending OC1C interrupt				[22]

	rjmp	9f				;								[24]

.L2:	; leaving bottom-blanking into vsync region								-> [15]
	ldi	r16, PAL_VSYNC_LINES-1		;								[16]
	mov	r5, r16				;								[17]
	inc	r4				;								[18]
	; set OC1A for VSYNC
	ldi	r17:r16, CYC_V_SYNC		;								[19]
	sts	OCR1AH, r17			;								[21]
	sts	OCR1AL, r16			;								[23]
	sbi	PORTB, 7	; DEBUG

	rjmp	9f				;								[25]

.L3:	; leaving vsync region into top-blanking								-> [17]
	ldi	r16, PAL_TBLANK_LINES-1		;								[18]
	mov	r5, r16				;								[19]
	inc	r4				;								[20]
	; set OC1A timing for HSYNC
	ldi	r17:r16, CYC_H_SYNC		;								[21]
	sts	OCR1AH, r17			;								[23]
	sts	OCR1AL, r16			;								[25]
	cbi	PORTB, 7	; DEBUG

	rjmp	9f				;								[27]

.L5:						;								-> [10]
	tst	r4				;								[11]
	brne	9f				; branch if not in active region				[12+1]
	rjmp	7f				;								[14]

.L6:	; first active line [191]										-> [21]
	; turn on OC1C and its interrupt
	ldi	YH, hi(V_framebuffer)		;								[22]
	ldi	YL, lo(V_framebuffer)		;								[23]
	ldi	r16, TIFR1_BIT_OCF1C		;								[24]
	sts	TIFR1, r16			; clear any past OC1C interrupt					[26]
	ldi	r16, (TIMSK1_BIT_OCIE1C | TIMSK1_BIT_TOIE1)	;						[27]
	sts	TIMSK1, r16			; enable OC1C and overflow interrupts				[29]


.L7:	; in active line [191-0]										-> [14, 29]

.L9:						;								-> [24, 25, 27, 13, 14, 29]
	; here we are in the new V region (r4).
	;								-> [24] if just moved into bottom-blanking (and OC1C now all clear)
	;								-> [25] if just moved into vsync (and now generating vsync pulses)
	;								-> [27] if just moved into top-blanking (and now generating hsync pulses)
	;								-> [13] if in the middle of some non-active region, possibly last time
	;								-> [14] if in the middle of the active region
	;								-> [29] if just moved into active region (and OC1C now enabled, YH:YL set)

	pop	r17				;								[<= 30 cycles to here].
	pop	r16
	out	SREG, r2			; restore SREG
	reti

;}}}
VEC_timer1compc: ;{{{  interrupt for TIMER1 comparator C
	; can use r2 as temporary
	in	r2, SREG
	push	r16
	push	r17

	; this is triggered shortly after the timer interrupt on active lines only to kick-off the USART
	ldi	r16, 0xc0		; MSPIM mode
	sts	UCSR1C, r16
	ldi	r16, 0x60		; TXC1, UDRE1
	sts	UCSR1A, r16
	ldi	r16, 0x68		; UDRIE, TXCIE, TXEN1
	sts	UCSR1B, r16
	ldi	r17:r16, 2		; baud rate
	sts	UBRR1H, r17
	sts	UBRR1L, r16
	; transmitter up-and-running, UDRE interrupt will be ready to fire

	ldi	r16, HRES		; this many bytes please
	mov	r3, r16

	pop	r17
	pop	r16
	out	SREG, r2
	reti


;}}}
VEC_usart1udre:	;{{{  interrupt for USART1 data register empty
	; can use r2 as temporary
	in	r2, SREG
	push	r16

	; more to go
	ld	r16, Y+
	sts	UDR1, r16

	dec	r3
	brne	1f

	; here means we've just dispatched what is the last thing into the mix, so fixup Y for next line of data (rather, rewind for same data)
	; also means we've got plenty of time
	ldi	r16, 0x40		; UDRE interrupt off, disable transmitter (TX-complete interrupt still enabled).
	sts	UCSR1B, r16

	mov	r16, r5			; r5 is active line [191 -> 0]
	andi	r16, 0x01
	breq	1f			; branch if bit 0 is clear
	sbiw	Y, HRES			; rewind please
.L1:

	pop	r16
	out	SREG, r2
	reti

;}}}
VEC_usart1tx:	;{{{  interrupt for USART1 TX complete
	; Note: this gets called once at the end of each active line, just as the last byte has been shifted out (to the display)

	; can use r2 as temporary
	in	r2, SREG
	push	r16


	ldi	r16, 0x00		; interrupt and transmit disable
	sts	UCSR1B, r16
	sts	UBRR1H, r16		; set baud rate to zero
	sts	UBRR1L, r16
	cbi	PORTD, 3		; PD3 (TXD1) low

	pop	r16
	out	SREG, r2
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
	;sbi	DDRD, 3			; PD3/TXD1 (18) as output	: XXX: set as above (VID_DDR)
	cbi	DDRD, 2			; PD2/RXD1 (19) as input  [unused]
	sbi	DDRD, 5			; PD5/XCK1 as output  [not routed]

	;cbi	PORTD, 3		; TXD1 low

	clr	r16
	mov	r3, r16

	ldi	r17:r16, 0		; baud
	sts	UBRR1H, r17
	sts	UBRR1L, r16
	; NOTE: USART transmitter setup and enabled at line start point (in response to OCR1C).

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

	; OCR1C = PAL_CYC_OUTPUT_START
	ldi	r17:r16, PAL_CYC_OUTPUT_START
	sts	OCR1CH, r17
	sts	OCR1CL, r16

	; clear timer1 value
	clr	r16
	sts	TCNT1H, r16
	sts	TCNT1L, r16

	; setup location so next interrupt starts a vertical sync
	ldi	r16, 3
	mov	r4, r16
	ldi	r16, 0
	mov	r5, r16

	; setup TIMER1 for fast PWM mode, TOP=ICR1(=PAL_CYC_SCANLINE); OCR1A update at BOTTOM; overflow interrupt at TOP.
	; attach OC1A inverted mode, OC1B inverted mode
	ldi	r16, (TCCR1A_BIT_COM1A1 | TCCR1A_BIT_COM1A0 | TCCR1A_BIT_COM1B1 | TCCR1A_BIT_COM1B0 | TCCR1A_BIT_WGM11)
	; ldi	r16, (TCCR1A_BIT_COM1A1 | TCCR1A_BIT_COM1A0 | TCCR1A_BIT_WGM11)
	sts	TCCR1A, r16
	ldi	r16, (TCCR1B_BIT_WGM13 | TCCR1B_BIT_WGM12 | TCCR1B_BIT_CS10)
	sts	TCCR1B, r16

	; at this point the timer is running

	; enable timer interrupts: overflow
	ldi	r16, TIMSK1_BIT_TOIE1
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
fb_writechar: ;{{{  writes the ASCII char in r18 to the framebuffer at position (r16, r17) using 5x7 font.

	; check for valid character (obviously :))
	cpi	r18, 32
	brsh	0f
	rjmp	fb_writechar_out
.L0:
	cpi	r18, 128
	brlo	0f
	rjmp	fb_writechar_out
.L0:
	push	r0
	push	r1
	push	r18			; save regs from here
	push	r19
	push	r20
	push	r21
	push	XL
	push	XH
	push	ZL
	push	ZH

	subi	r18, 32			; normalise character to [0-95]

	; point Z at start of the correct character
	ldi	ZH, hi(font_table_5x7)
	ldi	ZL, lo(font_table_5x7)
	ldi	r19, 8			; bytes per character
	mul	r18, r19		; result left in r1:r0
	add	ZL, r0
	adc	ZH, r1

	; point X at the first byte in the framebuffer of interest
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	ldi	r19, HRES		; bytes per line
	mul	r17, r19		; Y*HRES in r1:r0
	add	XL, r0
	adc	XH, r1
	mov	r19, r16		; copy of X
	lsr	r19			; divide by 8 to get byte portion
	lsr	r19
	lsr	r19
	clr	r20
	add	XL, r19			; add into X
	adc	XH, r20

	mov	r19, r16
	andi	r19, 0x07		; bit start within byte
	cpi	r19, 3
	brne	0f
	rjmp	fb_writechar_offs3	; simple case :)
.L0:

	; FIXME: incomplete!

fb_writechar_cont:

	pop	ZH
	pop	ZL
	pop	XH
	pop	XL
	pop	r21
	pop	r20
	pop	r19
	pop	r18
	pop	r1
	pop	r0
fb_writechar_out:
	ret

fb_writechar_offs3:
	; XH:XL = address of byte in FB
	; ZH:ZL = address of first character byte (5 LSBs)
	; r16,r17 = pos; r18 = char;  r19-r21 = available.
	ldi	r21, 7			; this many bytes please
.L0:
	ld	r20, X
	lpm	r19, Z+
	or	r20, r19		; OR character data into FB byte
	st	X, r20
	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

;}}}

vid_waitbot: ;{{{  waits for the render to reach bottom of visible area (so we can start updating things)
	push	r16

.L0:
	mov	r16, r4			; basically wait for (r4 == 1) and (r5 == (PAL_BBLANK_LINES-1)).
	cpi	r16, 1
	brne	0b			; nope, wait some more
	mov	r16, r5
	cpi	r16, PAL_BBLANK_LINES-1
	brne	0b			; nope, wait some more (will be waiting a long time here..)

	; yep, at start of bottom blanking region.
	; this gives us basically 120 PAL lines worth of time before the next frame is drawn <= 7.68ms ~= 122880 cycles at 16 MHz --- MANY LOTS :-)
	pop	r16
	ret
;}}}
vid_waitbot1: ;{{{  waits for the render to reach just after bottom of visible area (forces delay with the above)
	push	r16

.L0:
	mov	r16, r4			; basically wait for (r4 == 1) and (r5 == (PAL_BBLANK_LINES-2)).
	cpi	r16, 1
	brne	0b			; nope, wait some more
	mov	r16, r5
	cpi	r16, PAL_BBLANK_LINES-2
	brne	0b			; nope, wait some more (will be waiting a long time here..)

	; yep, in 2nd line of bottom blanking region.
	; this gives us basically 119 PAL lines worth of time before the next frame is drawn <= 7.616ms ~= 121856 cycles at 16 MHz --- MANY LOTS :-)
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

	;ldi	r16, 2
	;ldi	r17, 0
	;ldi	r18, 96
	;rcall	fb_vline
	;ldi	r16, 4
	;rcall	fb_vline
	;ldi	r16, 7
	;rcall	fb_vline
	;ldi	r16, 11
	;rcall	fb_vline

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

	; slap in some text
	ldi	r16, 11
	ldi	r17, 3
	ldi	r18, 'f'
	rcall	fb_writechar

	ldi	r16, 19
	ldi	r17, 5
	ldi	r18, 'b'
	rcall	fb_writechar

	rcall	vid_waitbot
	rjmp	1b

loop:
	;sleep
	nop
	nop
	nop
	nop
	rjmp	loop


;------------------------------------------------------------------------------
; fonts and suchlike
;------------------------------------------------------------------------------

.include "fb-font5x7.inc"

