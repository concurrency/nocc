;
;	test_avr33.asm -- testing for the ATMEGA2560: PAL display generation [for fun :)]
;	Copyright (C) 2014, Fred Barnes, University of Kent <frmb@kent.ac.uk>.
;

.mcu	"atmega2560"

.include "atmega1280.inc"

; GROT: this code is hardwired to a resolution of 128x96.
; Source: a lot of the initial how-to I got from the Arduino TVout library [Myles Metzer, 2010].
; Source: assorted inspiration / code observation from 'craft' [Linus Åkesson, 2008].

; MORE GROT: the first handful of X pixels don't get shown for whatever reason (maybe just my display though).

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
.equ	PAL_CYC_OUTPUT_START	=90
.equ	PAL_CYC_VGATE_START	=150	; turn on here (OC1B)

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
;{{{  other variables
V_scroll1:
V_scroll1_h:
	.space	1				; 16-bit organised as 13-bit:3-bit
V_scroll1_l:
	.space	1

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
	out	TIFR1, r16			; clear any pending OC1C interrupt				[22]
	;sts	TIFR1, r16

	rjmp	9f				;								[24]

.L2:	; leaving bottom-blanking into vsync region								-> [15]
	ldi	r16, PAL_VSYNC_LINES-1		;								[16]
	mov	r5, r16				;								[17]
	inc	r4				;								[18]
	; set OC1A for VSYNC
	ldi	r17:r16, CYC_V_SYNC		;								[19]
	sts	OCR1AH, r17			;								[21]
	sts	OCR1AL, r16			;								[23]

	rjmp	9f				;								[25]

.L3:	; leaving vsync region into top-blanking								-> [17]
	ldi	r16, PAL_TBLANK_LINES-1		;								[18]
	mov	r5, r16				;								[19]
	inc	r4				;								[20]
	; set OC1A timing for HSYNC
	ldi	r17:r16, CYC_H_SYNC		;								[21]
	sts	OCR1AH, r17			;								[23]
	sts	OCR1AL, r16			;								[25]

	rjmp	9f				;								[27]

.L5:						;								-> [10]
	tst	r4				;								[11]
	brne	9f				; branch if not in active region				[12+1]
	rjmp	7f				;								[14]

.L6:	; first active line [191]										-> [21]
	; turn on OC1C and its interrupt
	sbi	PORTB, 7	; DEBUG
	ldi	YH, hi(V_framebuffer)		;								[22]
	ldi	YL, lo(V_framebuffer)		;								[23]
	ldi	r16, TIFR1_BIT_OCF1C		;								[24]
	out	TIFR1, r16			; clear any past OC1C interrupt					[26]
	;sts	TIFR1, r16
	ldi	r16, (TIMSK1_BIT_OCIE1C | TIMSK1_BIT_TOIE1)	;						[27]
	sts	TIMSK1, r16			; enable OC1C and overflow interrupts				[29]
	cbi	PORTB, 7	; DEBUG


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
	ldi	r17:r16, 0						; baud rate
	sts	UBRR1H, r17
	sts	UBRR1L, r16

	ldi	r16, (UCSR1A_BIT_TXC1 | UCSR1A_BIT_UDRE1)
	sts	UCSR1A, r16						; clear these interrupt flags
	ldi	r16, (UCSR1B_BIT_UDRIE1 | UCSR1B_BIT_TXEN1)
	sts	UCSR1B, r16
	; transmitter up-and-running, UDRE interrupt will be ready to fire
	ldi	r17:r16, 2						; baud rate
	sts	UBRR1H, r17
	sts	UBRR1L, r16

	ldi	r16, HRES		; this many bytes more please
	mov	r3, r16

	;ld	r16, Y+			; first character to go out
	;sts	UDR1, r16		; some zeros first -- padding

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
	ldi	r16, UCSR1B_BIT_TXCIE1		; UDRE interrupt off, disable transmitter, TX-complete interrupt enable.
	sts	UCSR1B, r16

	mov	r16, r5			; r5 is active line [191 -> 0]
	andi	r16, 0x01
	breq	1f			; branch if bit 0 is clear (2nd, 4th, etc.)
	sbiw	YH:YL, HRES		; rewind please
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
	;sts	UBRR1H, r16		; set baud rate to zero
	;sts	UBRR1L, r16
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

	ldi	r16, (UCSR1C_BIT_UMSEL11 | UCSR1C_BIT_UMSEL10)		; MSPIM mode
	sts	UCSR1C, r16
	ldi	r16, 0							; all interrupts off, receiver and transmitter disabled
	sts	UCSR1B, r16

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

	tst	r16			; see if X is negative (or >= 128)
	brpl	1f			; nope, straightforward

	cpi	r16, 0xfc		; -4
	brne	0f
	rjmp	fb_writechar_offs4r
.L0:
	cpi	r16, 0xfd		; -3
	brne	0f
	rjmp	fb_writechar_offs5r
.L0:
	cpi	r16, 0xfe		; -2
	brne	0f
	rjmp	fb_writechar_offs6r
.L0:
	cpi	r16, 0xff		; -1
	brne	0f
	rjmp	fb_writechar_offs7r
.L0:
	; must be off-screen then!
	rjmp	fb_writechar_cont

.L1:
	cpi	r16, 124		; see if we're in the right-hand edge region (chars starting 124 -> 127)
	brlo	1f			; nope, straightforward

	; if we go anywhere from here, must be right-most byte, X+(HRES-1)
	ldi	r19, HRES-1
	clr	r20
	add	XL, r19
	adc	XH, r20

	cpi	r16, 124
	brne	0f
	rjmp	fb_writechar_offs4l
.L0:
	cpi	r16, 125
	brne	0f
	rjmp	fb_writechar_offs5l
.L0:
	cpi	r16, 126
	brne	0f
	rjmp	fb_writechar_offs6l
.L0:
	cpi	r16, 127
	brne	0f
	rjmp	fb_writechar_offs7l
.L0:
	; should not get here, ever!
	rjmp	fb_writechar_cont

.L1:
	mov	r19, r16		; copy of X
	lsr	r19			; divide by 8 to get byte portion
	lsr	r19
	lsr	r19
	clr	r20
	add	XL, r19			; add into X
	adc	XH, r20

	mov	r19, r16
	andi	r19, 0x07		; bit start within byte
	cpi	r19, 0
	brne	0f
	rjmp	fb_writechar_offs0
.L0:
	cpi	r19, 1
	brne	0f
	rjmp	fb_writechar_offs1
.L0:
	cpi	r19, 2
	brne	0f
	rjmp	fb_writechar_offs2
.L0:
	cpi	r19, 3
	brne	0f
	rjmp	fb_writechar_offs3	; simple case :)
.L0:

	; then the more complex split-byte cases
	cpi	r19, 4
	brne	0f
	rjmp	fb_writechar_offs4
.L0:
	cpi	r19, 5
	brne	0f
	rjmp	fb_writechar_offs5
.L0:
	cpi	r19, 6
	brne	0f
	rjmp	fb_writechar_offs6
.L0:
	rjmp	fb_writechar_offs7


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

	; helpers for particular offsets:
	;
	; XH:XL = address of byte in FB
	; ZH:ZL = address of first character byte (5 LSBs)
	; r16,r17 = pos; r18 = char;  r19-r21,r0-r1 = available.
fb_writechar_offs0:
	ldi	r21, 7			; this many bytes please
.L0:
	ld	r20, X
	lpm	r19, Z+
	lsl	r19			; shift left 3 places
	lsl	r19
	lsl	r19
	or	r20, r19		; OR character data into FB byte
	st	X, r20
	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs1:
	ldi	r21, 7			; this many bytes please
.L0:
	ld	r20, X
	lpm	r19, Z+
	lsl	r19			; shift left 2 places
	lsl	r19
	or	r20, r19		; OR character data into FB byte
	st	X, r20
	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs2:
	ldi	r21, 7			; this many bytes please
.L0:
	ld	r20, X
	lpm	r19, Z+
	lsl	r19			; shift left 1 place
	or	r20, r19		; OR character data into FB byte
	st	X, r20
	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs3:
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

fb_writechar_offs4:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	clr	r1
	clc
	ror	r19			; keep 4 MSB in r19
	ror	r1			; put 1 LSB into r1 MSB

	ld	r20, X
	or	r20, r19
	st	X+, r20			; write left-hand bit, move to next

	ld	r20, X
	or	r20, r1
	st	X, r20			; write right-hand bit

	adiw	XH:XL, HRES-1		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs4l:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	lsr	r19			; keep 4 MSB in r19

	ld	r20, X
	or	r20, r19
	st	X, r20			; write left-hand bit

	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs4r:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	ror	r19
	ror	r19			; put 1 LSB in r19 MSB
	andi	r19, 0x80		; cull the rest

	ld	r20, X
	or	r20, r19
	st	X, r20			; write right-hand bit

	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs5:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	clr	r1
	clc
	ror	r19			; keep 4 MSB in r19
	ror	r1			; put 1 LSB into r1 MSB

	ror	r19			; keep 3 MSB in r19
	ror	r1			; put 2 LSB into tr1 MSB

	ld	r20, X
	or	r20, r19
	st	X+, r20			; write left-hand bit, move to next

	ld	r20, X
	or	r20, r1
	st	X, r20			; write right-hand bit

	adiw	XH:XL, HRES-1		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs5l:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	lsr	r19			; keep 3 MSB in r19
	lsr	r19

	ld	r20, X
	or	r20, r19
	st	X, r20			; write left-hand bit

	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs5r:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	ror	r19
	ror	r19
	ror	r19			; put 2 LSB in r19 MSB
	andi	r19, 0xc0		; cull the rest

	ld	r20, X
	or	r20, r19
	st	X, r20			; write right-hand bit

	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs6:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	clr	r1
	clc
	ror	r19			; keep 4 MSB in r19
	ror	r1			; put 1 LSB into r1 MSB

	ror	r19			; keep 3 MSB in r19
	ror	r1			; put 2 LSB into r1 MSB

	ror	r19			; keep 2 MSB in r19
	ror	r1			; put 3 LSB into r1 MSB

	ld	r20, X
	or	r20, r19
	st	X+, r20			; write left-hand bit, move to next

	ld	r20, X
	or	r20, r1
	st	X, r20			; write right-hand bit

	adiw	XH:XL, HRES-1		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs6l:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	lsr	r19			; keep 2 MSB in r19
	lsr	r19
	lsr	r19

	ld	r20, X
	or	r20, r19
	st	X, r20			; write left-hand bit

	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs6r:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	ror	r19
	ror	r19
	ror	r19
	ror	r19			; put 3 LSB in r19 MSB
	andi	r19, 0xe0		; cull the rest

	ld	r20, X
	or	r20, r19
	st	X, r20			; write right-hand bit

	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs7:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	clr	r1
	clc
	ror	r19			; keep 4 MSB in r19
	ror	r1			; put 1 LSB into r1 MSB

	ror	r19			; keep 3 MSB in r19
	ror	r1			; put 2 LSB into r1 MSB

	ror	r19			; keep 2 MSB in r19
	ror	r1			; put 3 LSB into r1 MSB

	ror	r19			; keep 1 MSB in r19
	ror	r1			; put 4 LSB into r1 MSB

	ld	r20, X
	or	r20, r19
	st	X+, r20			; write left-hand bit, move to next

	ld	r20, X
	or	r20, r1
	st	X, r20			; write right-hand bit

	adiw	XH:XL, HRES-1		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs7l:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	lsr	r19			; keep 1 MSB in r19
	lsr	r19
	lsr	r19
	lsr	r19

	ld	r20, X
	or	r20, r19
	st	X, r20			; write left-hand bit

	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont

fb_writechar_offs7r:
	ldi	r21, 7			; this many bytes please
.L0:
	lpm	r19, Z+
	ror	r19
	ror	r19
	ror	r19
	ror	r19
	ror	r19			; put 4 LSB in r19 MSB
	andi	r19, 0xf0		; cull the rest

	ld	r20, X
	or	r20, r19
	st	X, r20			; write right-hand bit

	adiw	XH:XL, HRES		; next framebuffer line
	dec	r21
	brne	0b

	rjmp	fb_writechar_cont


;}}}
fb_writestring: ;{{{  writes the null-terminated ASCII string pointed at by Z to the framebuffer starting at position (r16, r17) using 5x7 font.
	push	r16
	push	r18
	push	r19
	push	ZL
	push	ZH

.L0:
	lpm	r18, Z+			; load character
	cpi	r18, 0
	breq	9f			; done

	rcall	fb_writechar		; write it at the appropriate location
	subi	r16, (256 - 6)		; add 6
	brmi	9f			; wrapped off RHS, stop here regardless
	rjmp	0b			; next character please

.L9:
	pop	ZH
	pop	ZL
	pop	r19
	pop	r18
	pop	r16
	ret

;}}}

fb_pbyteblit: ;{{{  does byte-chunk blitting from program memory to the framebuffer (at r16*8,r17) from Z, width r18*8, height r19
	push	r0
	push	r1
	push	r16
	push	r17
	push	r18
	push	r19
	push	r20
	push	r21
	push	XL
	push	XH
	push	ZL
	push	ZH

	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	ldi	r20, HRES
	mul	r17, r20			; r1:r0 = Y * HRES
	add	XL, r0
	adc	XH, r1
	clr	r20
	add	XL, r16				; add target X (*8)
	adc	XH, r20

	; X now points at the start in the framebuffer, can re-use r16/r17

	; for each line..
.L0:
	mov	r16, r18			; byte count

	mov	r21, XH				; save X (for this line)
	mov	r20, XL
.L1:
	lpm	r17, Z+				; next byte from program memory
	st	X+, r17				; store in framebuffer
	dec	r16
	brne	1b				; more to go

	mov	XH, r21				; restore X
	mov	XL, r20
	adiw	XH:XL, HRES			; next line

	dec	r19				; height--
	brne	0b				; next line please

.L9:

	pop	ZH
	pop	ZL
	pop	XH
	pop	XL
	pop	r21
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	pop	r1
	pop	r0
	ret
;}}}

fb_drawline: ;{{{  draws a line starting at (r16,r17) [unsigned] for (r18,r19) [signed]
	push	r0
	push	r1
	push	r16
	push	r17
	push	r18
	push	r19
	push	r20
	push	r21
	push	r24
	push	r25
	push	XL
	push	XH

	cpi	r17, 96
	brlo	0f			; in range (unsigned)
	rjmp	9f			; outside raster
.L0:
	tst	r16
	brpl	0f			; is positive
	rjmp	9f			; outside raster maybe
.L0:
	cpi	r18, 0
	brne	0f

	; FIXME: incomplete!
	nop

.L0:

.L9:
	pop	XH
	pop	XL
	pop	r25
	pop	r24
	pop	r21
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	pop	r1
	pop	r0
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

lvar_init: ;{{{  initialises local stuffs
	push	r16
	push	r17
	push	XL
	push	XH

	ldi	r17:r16, 0
	sts	V_scroll1_h, r17
	sts	V_scroll1_l, r16

	pop	XH
	pop	XL
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
	rcall	lvar_init

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
	ldi	r16, 5
	rcall	fb_vline
	ldi	r16, 0
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

	ldi	r16, 4			; X = 32
	ldi	r17, 9
	ldi	r18, 11			; W = 88
	ldi	r19, 86
	ldi	ZH, hi(I_ada_image)
	ldi	ZL, lo(I_ada_image)
	rcall	fb_pbyteblit


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
	lds	r25, V_scroll1_h
	lds	r24, V_scroll1_l

	; upper 13-bits are message offset, lower 3 are character-offset
	clr	r16
	clc
	ror	r25
	ror	r24
	ror	r16
	ror	r25
	ror	r24
	ror	r16
	ror	r25			; third bit
	ror	r24
	ror	r16

	; r25:r24 how has the 13-bit message offset, r16 has 3 MSBs with the offset
	lsr	r16
	swap	r16			; r16 = 0-5

	ldi	ZH, hi(S_message)
	ldi	ZL, lo(S_message)
	add	ZL, r24
	adc	ZH, r25			; adjust for message offset

	mov	r17, r16
	ldi	r16, 5
	sub	r16, r17		; r16 = 5-0, X position
	ldi	r17, 2			; Y position

	rcall	fb_writestring

	cpi	r16, 0
	breq	4f
	; moving within a character, add 1 to low-order bits
	lds	r25, V_scroll1_h
	lds	r24, V_scroll1_l
	adiw	r25:r24, 1
	sts	V_scroll1_h, r25
	sts	V_scroll1_l, r24
	rjmp	5f
.L4:
	; end of this character, onto next
	lds	r25, V_scroll1_h
	lds	r24, V_scroll1_l
	adiw	r25:r24, 8
	andi	r24, 0xf8		; clear out low-order bits
	sts	V_scroll1_h, r25
	sts	V_scroll1_l, r24

	; check to see if we're at the end: Z still pointing at what we're about to display, if the null-character, reset
	lpm	r16, Z
	tst	r16			; null-terminator?
	brne	5f			; nope

	clr	r16
	sts	V_scroll1_h, r16
	sts	V_scroll1_l, r16
.L5:


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

S_message:
	.const	"                         Welcome to the AVR/PAL thing!  :-)  :-)  :-)  ",
		"This is currently re-rendering the text for each frame, and everything else, ",
		"as well as clearing the framebuffer, just to get an idea of how much work we ",
		"can do outside the interrupt handlers..", 0x00

.include "fb-ada.inc"

