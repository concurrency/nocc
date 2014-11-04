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

;{{{  hardware notes (wiring)
; Video:				[Arduino pin]
;	vsync/hsync:	OC1A / PB5	[11]
;	vgate:		OC1B / PB6	[12]
;	video-signal:	TXD1 / PD3	[18]
; Sound:
;	channel 1 hi:	OC3A / PE3	[5]
;	channel 1 lo:	OC3B / PE4	[2]
;	channel 2 hi:	OC4A / PH3	[6]
;	channel 2 lo:	OC4B / PH4	[7]
;	channel 3 hi:	OC5A / PL3	[46]
;	channel 3 lo:	OC5B / PL4	[45]
;
;}}}
;{{{  reserved registers (and calling convention)

; registers r0 and r1 available, generally MUL/FMUL target

.def	UDRE_ISR_TMP		=r2		; for the UDRE interrupt routine: this needs to fire every 48 cycles or so to keep up when rendering
.def	UDRE_ISR_COUNT		=r3

.def	LINE_REGION		=r4		; bottom two bits determine the vertical position in rendering:
						;	0 = active region
						;	1 = bottom blanking
						;	2 = vsync
						;	3 = top blanking
						; other bits:
.def	LINE_COUNT		=r5

.def	SND_FRAME		=r6		; current sound frame, updated when this hits zero
.def	SND_C1VAL		=r7		; specific settings for channel 1
.def	SND_C2VAL		=r8		; specific settings for channel 2
.def	SND_C3VAL		=r9		; specific settings for channel 3

.def	LINE_ADDR_L		=r28		; use 'Y' exclusively for framebuffer line address when rendering
.def	LINE_ADDR_H		=r29

; calling convention:
;
;  callee saves all modified;
;  params passed in r16, r17, ...;  results in r16, r17, ...
;

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
;{{{  music note constants
.equ	NOTE_NOCHANGE	=0x00
.equ	NOTE_OFF	=0x01

.equ	NOTE_DRUM3	=0x02
.equ	NOTE_DRUM2	=0x03
.equ	NOTE_DRUM1	=0x04
.equ	NOTE_DRUMLAST	=0x05

.equ	NOTE_C2		=0x10	; 65.406
.equ	NOTE_C2S	=0x11
.equ	NOTE_D2		=0x12
.equ	NOTE_D2S	=0x13
.equ	NOTE_E2		=0x14
.equ	NOTE_F2		=0x15
.equ	NOTE_F2S	=0x16
.equ	NOTE_G2		=0x17
.equ	NOTE_G2S	=0x18
.equ	NOTE_A2		=0x19	; 110.00
.equ	NOTE_A2S	=0x1a
.equ	NOTE_B2		=0x1b	; 123.47
.equ	NOTE_C3		=0x1c	; 130.81
.equ	NOTE_C3S	=0x1d
.equ	NOTE_D3		=0x1e
.equ	NOTE_D3S	=0x1f
.equ	NOTE_E3		=0x20
.equ	NOTE_F3		=0x21
.equ	NOTE_F3S	=0x22
.equ	NOTE_G3		=0x23
.equ	NOTE_G3S	=0x24
.equ	NOTE_A3		=0x25	; 220.00
.equ	NOTE_A3S	=0x26
.equ	NOTE_B3		=0x27	; 246.94
.equ	NOTE_C4		=0x28	; 261.62
.equ	NOTE_C4S	=0x29
.equ	NOTE_D4		=0x2a
.equ	NOTE_D4S	=0x2b
.equ	NOTE_E4		=0x2c
.equ	NOTE_F4		=0x2d
.equ	NOTE_F4S	=0x2e
.equ	NOTE_G4		=0x2f
.equ	NOTE_G4S	=0x30
.equ	NOTE_A4		=0x31	; 440.00
.equ	NOTE_A4S	=0x32
.equ	NOTE_B4		=0x33	; 493.88
.equ	NOTE_C5		=0x34	; 523.25
.equ	NOTE_C5S	=0x35
.equ	NOTE_D5		=0x36
.equ	NOTE_D5S	=0x37
.equ	NOTE_E5		=0x38
.equ	NOTE_F5		=0x39
.equ	NOTE_F5S	=0x3a
.equ	NOTE_G5		=0x3b
.equ	NOTE_G5S	=0x3c
.equ	NOTE_A5		=0x3d
.equ	NOTE_A5S	=0x3e
.equ	NOTE_B5		=0x3f	; 987.77
.equ	NOTE_C6		=0x40	; 1046.5
.equ	NOTE_C6S	=0x41
.equ	NOTE_D6		=0x42
.equ	NOTE_D6S	=0x43
.equ	NOTE_E6		=0x44
.equ	NOTE_F6		=0x45
.equ	NOTE_F6S	=0x46
.equ	NOTE_G6		=0x47
.equ	NOTE_G6S	=0x48
.equ	NOTE_A6		=0x49
.equ	NOTE_A6S	=0x4a
.equ	NOTE_B6		=0x4b	; 1975.5

.equ	NOTE_END	=0xff
;}}}
;{{{  misc sound constants
.equ	SOUND_SPEED		=12

;}}}

.data
.org	0x400
V_framebuffer1: ;{{{  suitably sized b&w framebuffers  [3k of space]
	.space	(HRES * VRES)

V_framebuffer2:				; starts at 0xa00
	.space	(HRES * VRES)

V_fbtailslack:
	.space	(HRES * 2)		; some slack

;}}}
;{{{  other variables
V_scroll1:
V_scroll1_h:	.space 1			; 16-bit organised as 13-bit:3-bit
V_scroll1_l:	.space 1

V_sndpos:
V_sndpos_h:	.space 1			; 16-bit sound position
V_sndpos_l:	.space 1

V_bfreq_c1:
V_bfreq_c1_h:	.space 1			; 16-bit channel 1 base frequency
V_bfreq_c1_l:	.space 1

V_afreq_c1:
V_afreq_c1_h:	.space 1			; 16-bit channel 1 alternate frequency
V_afreq_c1_l:	.space 1

V_bfreq_c2:
V_bfreq_c2_h:	.space 1			; 16-bit channel 2 base frequency
V_bfreq_c2_l:	.space 1

V_afreq_c2:
V_afreq_c2_h:	.space 1			; 16-bit channel 2 alternate frequency
V_afreq_c2_l:	.space 1

V_bfreq_c3:
V_bfreq_c3_h:	.space 1			; 16-bit channel 3 base frequency
V_bfreq_c3_l:	.space 1

V_afreq_c3:
V_afreq_c3_h:	.space 1			; 16-bit channel 3 alternate frequency
V_afreq_c3_l:	.space 1

V_sfld_pos:	.space 1

V_sfld_l1:	.space (3 * 16)			; 16 (x,y,type) points
V_sfld_l2:	.space (3 * 16)			; ditto for 2nd layer
V_sfld_l3:	.space (3 * 16)			; ditto for 3rd layer

V_sctext:	.space 2			; 16-bits for flash address of the string
		.space 2			; 16-bits of bitwise scrolly offset
		.space 1			; defines how this particular one works (scroll-in-R, hold, scroll-out-L)
		.space 1			; Y offset

V_svtext:	.space 2			; 16-bits for flash address of the string
		.space 1			; 8 bits for scroll/Y offset
		.space 1			; mode (scroll-in-top, hold, scroll-out-bot)
		.space 1			; X offset
		.space 1			; target Y for scroll-down

V_svrtext:	.space 2			; 16-bits for flash address of the string
		.space 1			; 8 bits for scroll/Y offset
		.space 1			; mode (scroll-in-bottom, hold, scroll-out-top)
		.space 1			; X offset
		.space 1			; target Y for scroll-up

V_rdrpnts:	.space (4 * 8)			; FIXME: for now ...

;}}}

.text
.include "atmega1280-imap.inc"

VEC_timer1ovf:	;{{{  interrupt for TIMER1 overflow
	; Note: this is triggered every 64us or so.  Exclusive so can use 'r2' (UDRE temporary).
	in	r2, SREG			; save SREG							[1]
	push	r16				;								[3]
	push	r17				;								[5]

	dec	LINE_COUNT			; next line please						[6]
	mov	r16, LINE_COUNT			;								[7]
	cpi	r16, 0xff			; see if we rolled over						[8]
	brne	5f				; straightforward: nothing special here				[9+1]
	; rolled over, entering new region
	mov	r16, LINE_REGION		; current V region						[10]
	andi	r16, 0x03			; only interested in low-order bits				[11]
	breq	1f				;								[12+1]
	cpi	r16, 1				;								[13]
	breq	2f				;								[14+1]
	cpi	r16, 2				;								[15]
	breq	3f				;								[16+1]
	; else must be 3 (top-blanking region), moving into active region
	ldi	r16, PAL_ACTIVE_LINES-1		;								[17]
	mov	LINE_COUNT, r16			;								[18]
	ldi	r16, 0xfc			; mask to clear							[19]
	and	LINE_REGION, r16		;								[20]
	rjmp	6f				;								[22]

.L1:	; leaving active region into bottom-blanking								-> [13]
	ldi	r16, PAL_BBLANK_LINES-1		;								[14]
	mov	LINE_COUNT, r16			;								[15]
	inc	LINE_REGION			;								[16]
	; better make sure we turn off OC1C here
	ldi	r16, TIMSK1_BIT_TOIE1		;								[17]
	sts	TIMSK1, r16			; turn off OC1C interrupt					[19]
	ldi	r16, TIFR1_BIT_OCF1C		;								[20]
	out	TIFR1, r16			; clear any pending OC1C interrupt				[22]
	;sts	TIFR1, r16

	rjmp	9f				;								[24]

.L2:	; leaving bottom-blanking into vsync region								-> [15]
	ldi	r16, PAL_VSYNC_LINES-1		;								[16]
	mov	LINE_COUNT, r16			;								[17]
	inc	LINE_REGION			;								[18]
	; set OC1A for VSYNC
	ldi	r17:r16, CYC_V_SYNC		;								[19]
	sts	OCR1AH, r17			;								[21]
	sts	OCR1AL, r16			;								[23]

	rjmp	8f				;								[25]

.L3:	; leaving vsync region into top-blanking								-> [17]
	ldi	r16, PAL_TBLANK_LINES-1		;								[18]
	mov	LINE_COUNT, r16			;								[19]
	inc	LINE_REGION			;								[20]
	; set OC1A timing for HSYNC
	ldi	r17:r16, CYC_H_SYNC		;								[21]
	sts	OCR1AH, r17			;								[23]
	sts	OCR1AL, r16			;								[25]

	rjmp	9f				;								[27]

.L5:						;								-> [10]
	tst	LINE_REGION			;								[11]
	brne	9f				; branch if not in active region				[12+1]
	rjmp	7f				;								[14]

.L8:						;								-> [25]
	; here when in first vsync line, used to update sound stuff
	dec	SND_FRAME			;								[26]
	brne	1f				;								[27+1]
	ldi	r16, SOUND_SPEED		;								[28]
	mov	SND_FRAME, r16			;								[29]
	rcall	next_sound_frame_isr		;								[33]
						;								-> [..]
	rjmp	9f

.L1:	; land here in first vsync, but not new sound frame							-> [28]
	rcall	stepx_sound_frame_isr		;								[32]
						;								-> [..]
	ldi	r16, SOUND_SPEED - 2		; in 3rd 1/50th part-frame?
	cp	SND_FRAME, r16			;
	brne	9f				;
	rcall	step2_sound_frame_isr		;
						;
	rjmp	9f

.L6:	; first active line [191]										-> [22]
	; turn on OC1C and its interrupt
	;sbi	PORTB, 7	; DEBUG
	sbrc	LINE_REGION, 7			;								[23]
	ldi	YH, hi(V_framebuffer2)		;								[24]
	sbrs	LINE_REGION, 7			;								[25]
	ldi	YH, hi(V_framebuffer1)		;								[26]

	clr	YL				;								[27]
	ldi	r16, TIFR1_BIT_OCF1C		;								[28]
	out	TIFR1, r16			; clear any past OC1C interrupt					[30]
	;sts	TIFR1, r16
	ldi	r16, (TIMSK1_BIT_OCIE1C | TIMSK1_BIT_TOIE1)	;						[31]
	sts	TIMSK1, r16			; enable OC1C and overflow interrupts				[33]
	;cbi	PORTB, 7	; DEBUG


.L7:	; in active line [191-0]										-> [14, 33]

.L9:						;								-> [24, 28, 27, 13, 14, 33]
	; here we are in the new V region (r4).
	;								-> [24] if just moved into bottom-blanking (and OC1C now all clear)
	;								-> [28] if just moved into vsync (and now generating vsync pulses) and not a
	;									new sound frame (SND_FRAME register > 0).
	;								-> [27] if just moved into top-blanking (and now generating hsync pulses)
	;								-> [13] if in the middle of some non-active region, possibly last time
	;								-> [14] if in the middle of the active region
	;								-> [29] if just moved into active region (and OC1C now enabled, YH:YL set)

	pop	r17				;								[<= 30 cycles to here in most cases].
	pop	r16
	out	SREG, r2			; restore SREG
	reti

;}}}
next_sound_frame_isr: ;{{{  called from the video interrupt handler at the start of a vsync section to signal next sound frame
	; Note: r16 and r17 are available
	push	ZH
	push	ZL
	push	r25
	push	r24

	lds	r25, V_sndpos_h	
	lds	r24, V_sndpos_l	
	adiw	r25:r24, 6
	sts	V_sndpos_h, r25
	sts	V_sndpos_l, r24			; V_sndpos += 6

	ldi	ZH, hi(D_soundtrk)
	ldi	ZL, lo(D_soundtrk)

	; Note: each set of notes/values is 
	add	ZL, r24
	adc	ZH, r25				; Z = address in D_soundtrk

	; fetch-and-play for channel 1
	lpm	r16, Z+				; load channel 1 note
	lpm	r17, Z+				; load setting

	cpi	r16, NOTE_END
	breq	0f				; special case: end of tune

	cpi	r16, NOTE_NOCHANGE
	breq	2f				; keep doing what we were already doing (channel 1)
	rcall	snd_playnote_c1			; start playing!
.L2:

	; fetch-and-play for channel 2
	lpm	r16, Z+				; load channel 2 note
	lpm	r17, Z+				; load setting

	cpi	r16, NOTE_NOCHANGE
	breq	2f				; keep doing what we were already doing (channel 2)
	rcall	snd_playnote_c2			; start playing!
.L2:

	; fetch-and-play for channel 3
	lpm	r16, Z+				; load channel 3 note
	lpm	r17, Z+				; load setting

	cpi	r16, NOTE_NOCHANGE
	breq	2f				; keep doing what we were already doing (channel 2)
	rcall	snd_playnote_c3			; start playing!
.L2:

	rjmp	1f
.L0:
	; end of tune, redo from start
	clr	r16
	sts	V_sndpos_h, r16
	sts	V_sndpos_l, r16			; V_sndpos = 0
.L1:

	pop	r24
	pop	r25
	pop	ZL
	pop	ZH
	ret							;						[+5]
;}}}
stepx_sound_frame_isr: ;{{{  called from the video interrupt handler at the start of a vsync section to signal non-start part of sound frame (tremelo)
	; Note: r16 and r17 are available
	;{{{  channel 1 tremelo handling
	mov	r16, SND_C1VAL
	andi	r16, 0x40			; bit 6 (tremelo) set?
	breq	1f

	; yes, twiddle appropriately
	push	r19
	;{{{  ensure timer/counter3 is stopped
	lds	r19, TCCR3B
	andi	r19, ~(TCCR3B_BIT_CS32 | TCCR3B_BIT_CS31 | TCCR3B_BIT_CS30)
	sts	TCCR3B, r19		; ensure stopped

	;}}}
	;{{{  load alternate or base frequency
	mov	r16, SND_FRAME
	andi	r16, 0x01		; low-bit set?
	breq	2f			; nope, go for base frequency
	; yes, load alternate frequency
	lds	r17, V_afreq_c1_h
	lds	r16, V_afreq_c1_l
	rjmp	3f
.L2:
	; no, load base frequency
	lds	r17, V_bfreq_c1_h
	lds	r16, V_bfreq_c1_l
.L3:
	sts	ICR3H, r17
	sts	ICR3H, r16
	rcall	snd_setc1output

	;}}}
	; Note: keep r19 with TCCR3B bits in
	;{{{  restart timer/counter3
	ori	r19, TCCR3B_BIT_CS31	; prescale /8
	sts	TCCR3B, r19

	;}}}
	pop	r19
.L1:
	;}}}
	;{{{  channel 2 tremelo handling
	mov	r16, SND_C2VAL
	andi	r16, 0x40			; bit 6 (tremelo) set?
	breq	1f

	; yes, twiddle appropriately
	push	r19
	;{{{  ensure timer/counter4 is stopped
	lds	r19, TCCR4B
	andi	r19, ~(TCCR4B_BIT_CS42 | TCCR4B_BIT_CS41 | TCCR4B_BIT_CS40)
	sts	TCCR4B, r19		; ensure stopped

	;}}}
	;{{{  load alternate or base frequency
	mov	r16, SND_FRAME
	andi	r16, 0x01		; low-bit set?
	breq	2f			; nope, go for base frequency
	; yes, load alternate frequency
	lds	r17, V_afreq_c2_h
	lds	r16, V_afreq_c2_l
	rjmp	3f
.L2:
	; no, load base frequency
	lds	r17, V_bfreq_c2_h
	lds	r16, V_bfreq_c2_l
.L3:
	sts	ICR4H, r17
	sts	ICR4H, r16
	rcall	snd_setc2output

	;}}}
	; Note: keep r19 with TCCR3B bits in
	;{{{  restart timer/counter4
	ori	r19, TCCR4B_BIT_CS41	; prescale /8
	sts	TCCR4B, r19

	;}}}
	pop	r19
.L1:
	;}}}
	;{{{  channel 3 tremelo handling
	mov	r16, SND_C3VAL
	andi	r16, 0x40			; bit 6 (tremelo) set?
	breq	1f

	; yes, twiddle appropriately
	push	r19
	;{{{  ensure timer/counter5 is stopped
	lds	r19, TCCR5B
	andi	r19, ~(TCCR5B_BIT_CS52 | TCCR5B_BIT_CS51 | TCCR5B_BIT_CS50)
	sts	TCCR5B, r19		; ensure stopped

	;}}}
	;{{{  load alternate or base frequency
	mov	r16, SND_FRAME
	andi	r16, 0x01		; low-bit set?
	breq	2f			; nope, go for base frequency
	; yes, load alternate frequency
	lds	r17, V_afreq_c3_h
	lds	r16, V_afreq_c3_l
	rjmp	3f
.L2:
	; no, load base frequency
	lds	r17, V_bfreq_c3_h
	lds	r16, V_bfreq_c3_l
.L3:
	sts	ICR5H, r17
	sts	ICR5H, r16
	rcall	snd_setc3output

	;}}}
	; Note: keep r19 with TCCR5B bits in
	;{{{  restart timer/counter5
	ori	r19, TCCR5B_BIT_CS51	; prescale /8
	sts	TCCR5B, r19

	;}}}
	pop	r19
.L1:
	;}}}
	ret
;}}}
step2_sound_frame_isr: ;{{{  called from the video interrupt handler at the start of a vsync section to signal 2nd step in a sound frame
	; Note: r16 and r17 are available
	;{{{  channel 1 staccato handling
	mov	r16, SND_C1VAL
	andi	r16, 0x80			; high-bit (staccato) set?
	breq	1f

	; high-bit set, stop TIMER3 (channel 1)
	lds	r16, TCCR3B
	andi	r16, ~(TCCR3B_BIT_CS32 | TCCR3B_BIT_CS31 | TCCR3B_BIT_CS30)
	sts	TCCR3B, r16		; ensure stopped

	lds	r16, TCCR3A
	andi	r16, ~(TCCR3A_BIT_COM3A1 | TCCR3A_BIT_COM3A0 | TCCR3A_BIT_COM3B1 | TCCR3A_BIT_COM3B0)
	sts	TCCR3A, r16		; disconnect OC3A and OC3B

	clr	SND_C1VAL
.L1:
	;}}}
	;{{{  channel 2 staccato handling
	mov	r16, SND_C2VAL
	andi	r16, 0x80			; high-bit (staccato) set?
	breq	1f

	; high-bit set, stop TIMER4 (channel 2)
	lds	r16, TCCR4B
	andi	r16, ~(TCCR4B_BIT_CS42 | TCCR4B_BIT_CS41 | TCCR4B_BIT_CS40)
	sts	TCCR4B, r16		; ensure stopped

	lds	r16, TCCR4A
	andi	r16, ~(TCCR4A_BIT_COM4A1 | TCCR4A_BIT_COM4A0 | TCCR4A_BIT_COM4B1 | TCCR4A_BIT_COM4B0)
	sts	TCCR4A, r16		; disconnect OC4A and OC4B

	clr	SND_C2VAL
.L1:
	;}}}
	ret
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

	mov	r16, LINE_COUNT			; r5 is active line [191 -> 0]
	andi	r16, 0x01
	breq	1f				; branch if bit 0 is clear (2nd, 4th, etc.)
	sbiw	YH:YL, HRES			; rewind please
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

; NOTE: sound stuff up here to make jumps from interrupt handler less consuming.
snd_hwsetup: ;{{{  setup hardware for sound
	push	r16
	push	r17

	; enable TIMER3, TIMER4 and TIMER5 in PRR1
	lds	r16, PRR1
	andi	r16, ~(PRR1_BIT_PRTIM3 | PRR1_BIT_PRTIM4 | PRR1_BIT_PRTIM5)	; clear bits 3-5 (PRTIM3, PRTIM4, PRTIM5)
	sts	PRR1, r16

	; enable port outputs for PE3 (OC3A) and PE4 (OC3B)
	sbi	DDRE, 3
	sbi	DDRE, 4
	cbi	PORTE, 3
	cbi	PORTE, 4

	; and PH3 (OC4A), PH4 (OC4B), PL3 (OC5A) and PL4 (OC5B)
	lds	r16, DDRH
	ori	r16, 0x18			; PH3 and PH4 output
	sts	DDRH, r16
	lds	r16, DDRL
	ori	r16, 0x18			; PL3 and PL4 output
	sts	DDRL, r16
	lds	r16, PORTH
	andi	r16, ~0x18			; clear bits 3 and 4
	sts	PORTH, r16
	lds	r16, PORTL
	andi	r16, ~0x18			; clear bits 3 and 4
	sts	PORTL, r16

	; ICR3, OCR3A and OCR3B used for specific note generation (channel 1)
	; ICR4, OCR4A and OCR4B (channel 2)
	; ICR5, OCR5A and OCR5B (channel 3)

	; setup timer/counters 3, 4, 5 for phase+frequency correct PWM, TOP=ICR3 (WGM bits 1000b)
	; keep OC3A, OC3B, OC4A, OC4B, OC5A, OC5B detached to start with (enabled per note)
	ldi	r16, 0x00		;	(TCCR3A_BIT_COM3A1 | TCCR3A_BIT_COM3A0 | TCCR3A_BIT_COM3B1 | TCCR3A_BIT_COM3B0)
	sts	TCCR3A, r16
	ldi	r16, (TCCR3B_BIT_WGM33)
	sts	TCCR3B, r16

	ldi	r16, 0x00		;	(TCCR4A_BIT_COM4A1 | TCCR4A_IT_COM4A0 | TCCR4A_BIT_COM4B1 | TCCR3A_BIT_COM4B0)
	sts	TCCR4A, r16
	ldi	r16, (TCCR4B_BIT_WGM43)
	sts	TCCR4B, r16

	ldi	r16, 0x00		;	(TCCR5A_BIT_COM5A1 | TCCR5A_IT_COM5A0 | TCCR5A_BIT_COM5B1 | TCCR5A_BIT_COM5B0)
	sts	TCCR5A, r16
	ldi	r16, (TCCR5B_BIT_WGM53)
	sts	TCCR5B, r16

	; Note: keep TIMER3,4,5 stopped for now

	ldi	r16, SOUND_SPEED
	mov	SND_FRAME, r16
	clr	SND_C1VAL
	clr	SND_C2VAL
	clr	SND_C3VAL

	pop	r17
	pop	r16
	ret


;}}}

snd_setc1output: ;{{{  sets OCR3A and OCR3B as output/clear as specified in SND_C1VAL (r7) low nibble, r17:r16 has ICR3 value
	push	ZH
	push	ZL
	push	r19

	;{{{  sort out OCR3A based on SND_C1VAL bits 2-3
	mov	r19, SND_C1VAL
	andi	r19, 0x0c		; OCR3A setting (0-3)
	breq	0f
	
	; 25, 50 or 75% duty cycle
	movw	ZL, r16			; copy ICR3 into Z
	clc
	ror	ZH
	ror	ZL			; Z = ICR3 / 2
	cpi	r19, 0x08		; 50% duty?
	breq	2f
	cpi	r19, 0x04		; 25% duty?
	breq	3f
	; else 75% duty, so 1/4 value
	clc
	ror	ZH
	ror	ZL			; Z = ICR3 / 4
	rjmp	2f
.L3:
	; quarter-duty, so 3/4 value
	push	r24
	push	r25
	movw	r24, ZL			; r25:r24 = ICR3 / 2
	clc
	ror	r25
	ror	r24			; r25:r24 = ICR3 / 4
	add	ZL, r24
	adc	ZH, r25			; Z = ICR3 * 3/4
	pop	r25
	pop	r24
.L2:
	; turn on OCR3A (inverted mode)
	lds	r19, TCCR3A
	ori	r19, (TCCR3A_BIT_COM3A1 | TCCR3A_BIT_COM3A0)
	sts	TCCR3A, r19

	; write Z into OCR3A
	sts	OCR3AH, ZH
	sts	OCR3AL, ZL
	rjmp	1f

.L0:
	; turn off OCR3A (and disconnect)
	lds	r19, TCCR3A
	andi	r19, ~(TCCR3A_BIT_COM3A1 | TCCR3A_BIT_COM3A0)
	sts	TCCR3A, r19
	; write daft value into OCR3A to be sure
	ser	r19
	sts	OCR3AH, r19
	sts	OCR3AL, r19

.L1:
	;}}}
	;{{{  sort out OCR3B based on SND_C1VAL bits 0-1
	mov	r19, SND_C1VAL
	andi	r19, 0x03		; OCR3B setting (0-3)
	breq	0f
	
	; 25, 50 or 75% duty cycle
	movw	ZL, r16			; copy ICR3 into Z
	clc
	ror	ZH
	ror	ZL			; Z = ICR3 / 2
	cpi	r19, 0x02		; 50% duty?
	breq	2f
	cpi	r19, 0x01		; 25% duty?
	breq	3f
	; else 75% duty, so 1/4 value
	clc
	ror	ZH
	ror	ZL			; Z = ICR3 / 4
	rjmp	2f
.L3:
	; quarter-duty, so 3/4 value
	push	r24
	push	r25
	movw	r24, ZL			; r25:r24 = ICR3 / 2
	clc
	ror	r25
	ror	r24			; r25:r24 = ICR3 / 4
	add	ZL, r24
	adc	ZH, r25			; Z = ICR3 * 3/4
	pop	r25
	pop	r24
.L2:
	; turn on OCR3B (inverted mode)
	lds	r19, TCCR3A
	ori	r19, (TCCR3A_BIT_COM3B1 | TCCR3A_BIT_COM3B0)
	sts	TCCR3A, r19

	; write Z into OCR3B
	sts	OCR3BH, ZH
	sts	OCR3BL, ZL
	rjmp	1f

.L0:
	; turn off OCR3B (and disconnect)
	lds	r19, TCCR3A
	andi	r19, ~(TCCR3A_BIT_COM3B1 | TCCR3A_BIT_COM3B0)
	sts	TCCR3A, r19
	; write daft value into OCR3B to be sure
	ser	r19
	sts	OCR3BH, r19
	sts	OCR3BL, r19

.L1:
	;}}}

	pop	r19
	pop	ZL
	pop	ZH
	ret
;}}}
snd_playc1: ;{{{  play a note on channel 1, expects SND_C1VAL and V_bfreq/afreq_c1 to be set appropriately
	push	r16
	push	r17
	push	r19

	; setup ICR3 for specific note frequency
	lds	r17, V_bfreq_c1_h
	lds	r16, V_bfreq_c1_l
	sts	ICR3H, r17
	sts	ICR3L, r16

	; setup OC3A and OC3B, frequency given in r17:r16
	rcall	snd_setc1output

	pop	r19
	pop	r17
	pop	r16
	ret
;}}}
snd_playnote_c1: ;{{{  sets playing a specific note on channel 1 in r16 (NOTE_...), mod-bits in r17
	push	r16
	push	r17
	push	r18
	push	r20
	push	ZL
	push	ZH

	ldi	ZH:ZL, D_notetab
	cpi	r16, NOTE_OFF
	brne	0f
	rjmp	8f
.L0:
	cpi	r16, NOTE_C2
	brsh	0f
	rjmp	7f
.L0:
	cpi	r16, NOTE_B6+1
	brlo	0f
	rjmp	9f
.L0:
	; Note: no conditional branches past this (so can be big)

	;{{{  move mod-bits from r17 to SND_C1VAL (r7)
	mov	SND_C1VAL, r17

	;}}}
	;{{{  load regular note (0x10 - 0x4b) frequency from D_notetab into r17:r16, store in V_bfreq_c1 also
	; regular note (0x10 - 0x4b)
	subi	r16, 0x10
	lsl	r16			; r16 = offset in D_notetab
	clr	r17
	add	ZL, r16
	adc	ZH, r17

	lpm	r16, Z+			; load frequency into r17:r16
	lpm	r17, Z+

	sts	V_bfreq_c1_h, r17
	sts	V_bfreq_c1_l, r16

	;}}}
	;{{{  if tremelo bit set, set V_afreq_c1 to be next semitone up
	mov	r20, SND_C1VAL
	andi	r20, 0x40			; tremelo?
	breq	0f
	; yes, set alternate frequency to the next semitone up

	lpm	r20, Z+
	sts	V_afreq_c1_l, r20
	lpm	r20, Z+
	sts	V_afreq_c1_h, r20
.L0:
	;}}}
	;{{{  ensure timer/counter3 is stopped
	lds	r20, TCCR3B
	andi	r20, ~(TCCR3B_BIT_CS32 | TCCR3B_BIT_CS31 | TCCR3B_BIT_CS30)
	sts	TCCR3B, r20		; ensure stopped

	;}}}
	; Note: keep r20 with TCCR3B bits in
	rcall	snd_playc1

	; r20 still has TCCR3B bits in
	;{{{  re-enable timer/counter 3, prescale /8
	ori	r20, TCCR3B_BIT_CS31	; prescale /8
	sts	TCCR3B, r20

	;}}}
	rjmp	9f

	;------------------------------------------------------------------

.L7:
	; FIXME: drum handling..
	rjmp	8f

.L8:
	; turn note off
	clr	SND_C1VAL

	lds	r20, TCCR3B
	andi	r20, ~(TCCR3B_BIT_CS32 | TCCR3B_BIT_CS31 | TCCR3B_BIT_CS30)
	sts	TCCR3B, r20		; ensure stopped

	lds	r20, TCCR3A
	andi	r20, ~(TCCR3A_BIT_COM3A1 | TCCR3A_BIT_COM3A0 | TCCR3A_BIT_COM3B1 | TCCR3A_BIT_COM3B0)
	sts	TCCR3A, r20		; disconnect OC3A and OC3B


.L9:
	pop	ZH
	pop	ZL
	pop	r20
	pop	r18
	pop	r17
	pop	r16
	ret


;}}}

snd_setc2output: ;{{{  sets OCR4A and OCR4B as output/clear as specified in SND_C2VAL (r8) low nibble, r17:r16 has ICR4 value
	push	ZH
	push	ZL
	push	r19

	;{{{  sort out OCR4A based on SND_C2VAL bits 2-3
	mov	r19, SND_C2VAL
	andi	r19, 0x0c		; OCR3A setting (0-3)
	breq	0f
	
	; 25, 50 or 75% duty cycle
	movw	ZL, r16			; copy ICR4 into Z
	clc
	ror	ZH
	ror	ZL			; Z = ICR4 / 2
	cpi	r19, 0x08		; 50% duty?
	breq	2f
	cpi	r19, 0x04		; 25% duty?
	breq	3f
	; else 75% duty, so 1/4 value
	clc
	ror	ZH
	ror	ZL			; Z = ICR4 / 4
	rjmp	2f
.L3:
	; quarter-duty, so 3/4 value
	push	r24
	push	r25
	movw	r24, ZL			; r25:r24 = ICR4 / 2
	clc
	ror	r25
	ror	r24			; r25:r24 = ICR4 / 4
	add	ZL, r24
	adc	ZH, r25			; Z = ICR4 * 3/4
	pop	r25
	pop	r24
.L2:
	; turn on OCR4A (inverted mode)
	lds	r19, TCCR4A
	ori	r19, (TCCR4A_BIT_COM4A1 | TCCR4A_BIT_COM4A0)
	sts	TCCR4A, r19

	; write Z into OCR4A
	sts	OCR4AH, ZH
	sts	OCR4AL, ZL
	rjmp	1f

.L0:
	; turn off OCR4A (and disconnect)
	lds	r19, TCCR4A
	andi	r19, ~(TCCR4A_BIT_COM4A1 | TCCR4A_BIT_COM4A0)
	sts	TCCR4A, r19
	; write daft value into OCR4A to be sure
	ser	r19
	sts	OCR4AH, r19
	sts	OCR4AL, r19

.L1:
	;}}}
	;{{{  sort out OCR4B based on SND_C2VAL bits 0-1
	mov	r19, SND_C2VAL
	andi	r19, 0x03		; OCR4B setting (0-3)
	breq	0f
	
	; 25, 50 or 75% duty cycle
	movw	ZL, r16			; copy ICR4 into Z
	clc
	ror	ZH
	ror	ZL			; Z = ICR4 / 2
	cpi	r19, 0x02		; 50% duty?
	breq	2f
	cpi	r19, 0x01		; 25% duty?
	breq	3f
	; else 75% duty, so 1/4 value
	clc
	ror	ZH
	ror	ZL			; Z = ICR4 / 4
	rjmp	2f
.L3:
	; quarter-duty, so 3/4 value
	push	r24
	push	r25
	movw	r24, ZL			; r25:r24 = ICR4 / 2
	clc
	ror	r25
	ror	r24			; r25:r24 = ICR4 / 4
	add	ZL, r24
	adc	ZH, r25			; Z = ICR4 * 3/4
	pop	r25
	pop	r24
.L2:
	; turn on OCR4B (inverted mode)
	lds	r19, TCCR4A
	ori	r19, (TCCR4A_BIT_COM4B1 | TCCR4A_BIT_COM4B0)
	sts	TCCR4A, r19

	; write Z into OCR4B
	sts	OCR4BH, ZH
	sts	OCR4BL, ZL
	rjmp	1f

.L0:
	; turn off OCR4B (and disconnect)
	lds	r19, TCCR4A
	andi	r19, ~(TCCR4A_BIT_COM4B1 | TCCR4A_BIT_COM4B0)
	sts	TCCR4A, r19
	; write daft value into OCR4B to be sure
	ser	r19
	sts	OCR4BH, r19
	sts	OCR4BL, r19

.L1:
	;}}}

	pop	r19
	pop	ZL
	pop	ZH
	ret
;}}}
snd_playc2: ;{{{  play a note on channel 2, expects SND_C2VAL and V_bfreq/afreq_c2 to be set appropriately
	push	r16
	push	r17
	push	r19

	; setup ICR4 for specific note frequency
	lds	r17, V_bfreq_c2_h
	lds	r16, V_bfreq_c2_l
	sts	ICR4H, r17
	sts	ICR4L, r16

	; setup OC4A and OC4B, frequency given in r17:r16
	rcall	snd_setc2output

	pop	r19
	pop	r17
	pop	r16
	ret
;}}}
snd_playnote_c2: ;{{{  sets playing a specific note on channel 2 in r16 (NOTE_...), mod-bits in r17
	push	r16
	push	r17
	push	r18
	push	r20
	push	ZL
	push	ZH

	ldi	ZH:ZL, D_notetab
	cpi	r16, NOTE_OFF
	brne	0f
	rjmp	8f
.L0:
	cpi	r16, NOTE_C2
	brsh	0f
	rjmp	7f
.L0:
	cpi	r16, NOTE_B6+1
	brlo	0f
	rjmp	9f
.L0:
	; Note: no conditional branches past this (so can be big)

	;{{{  move mod-bits from r17 to SND_C2VAL (r8)
	mov	SND_C2VAL, r17

	;}}}
	;{{{  load regular note (0x10 - 0x4b) frequency from D_notetab into r17:r16, store in V_bfreq_c2 also
	; regular note (0x10 - 0x4b)
	subi	r16, 0x10
	lsl	r16			; r16 = offset in D_notetab
	clr	r17
	add	ZL, r16
	adc	ZH, r17

	lpm	r16, Z+			; load frequency into r17:r16
	lpm	r17, Z+

	sts	V_bfreq_c2_h, r17
	sts	V_bfreq_c2_l, r16

	;}}}
	;{{{  if tremelo bit set, set V_afreq_c2 to be next semitone up
	mov	r20, SND_C2VAL
	andi	r20, 0x40			; tremelo?
	breq	0f
	; yes, set alternate frequency to the next semitone up

	lpm	r20, Z+
	sts	V_afreq_c2_l, r20
	lpm	r20, Z+
	sts	V_afreq_c2_h, r20
.L0:
	;}}}
	;{{{  ensure timer/counter4 is stopped
	lds	r20, TCCR4B
	andi	r20, ~(TCCR4B_BIT_CS42 | TCCR4B_BIT_CS41 | TCCR4B_BIT_CS40)
	sts	TCCR4B, r20		; ensure stopped

	;}}}
	; Note: keep r20 with TCCR4B bits in
	rcall	snd_playc2

	; r20 still has TCCR4B bits in
	;{{{  re-enable timer/counter 4, prescale /8
	ori	r20, TCCR4B_BIT_CS41	; prescale /8
	sts	TCCR4B, r20

	;}}}
	rjmp	9f

	;------------------------------------------------------------------

.L7:
	; FIXME: drum handling..
	rjmp	8f

.L8:
	; turn note off
	clr	SND_C2VAL

	lds	r20, TCCR4B
	andi	r20, ~(TCCR4B_BIT_CS42 | TCCR4B_BIT_CS41 | TCCR4B_BIT_CS40)
	sts	TCCR4B, r20		; ensure stopped

	lds	r20, TCCR4A
	andi	r20, ~(TCCR4A_BIT_COM4A1 | TCCR4A_BIT_COM4A0 | TCCR4A_BIT_COM4B1 | TCCR4A_BIT_COM4B0)
	sts	TCCR4A, r20		; disconnect OC4A and OC4B


.L9:
	pop	ZH
	pop	ZL
	pop	r20
	pop	r18
	pop	r17
	pop	r16
	ret


;}}}

snd_setc3output: ;{{{  sets OCR5A and OCR5B as output/clear as specified in SND_C3VAL (r9) low nibble, r17:r16 has ICR5 value
	push	ZH
	push	ZL
	push	r19

	;{{{  sort out OCR5A based on SND_C3VAL bits 2-3
	mov	r19, SND_C3VAL
	andi	r19, 0x0c		; OCR3A setting (0-3)
	breq	0f
	
	; 25, 50 or 75% duty cycle
	movw	ZL, r16			; copy ICR5 into Z
	clc
	ror	ZH
	ror	ZL			; Z = ICR5 / 2
	cpi	r19, 0x08		; 50% duty?
	breq	2f
	cpi	r19, 0x04		; 25% duty?
	breq	3f
	; else 75% duty, so 1/4 value
	clc
	ror	ZH
	ror	ZL			; Z = ICR5 / 4
	rjmp	2f
.L3:
	; quarter-duty, so 3/4 value
	push	r24
	push	r25
	movw	r24, ZL			; r25:r24 = ICR5 / 2
	clc
	ror	r25
	ror	r24			; r25:r24 = ICR5 / 4
	add	ZL, r24
	adc	ZH, r25			; Z = ICR5 * 3/4
	pop	r25
	pop	r24
.L2:
	; turn on OCR5A (inverted mode)
	lds	r19, TCCR5A
	ori	r19, (TCCR5A_BIT_COM5A1 | TCCR5A_BIT_COM5A0)
	sts	TCCR5A, r19

	; write Z into OCR5A
	sts	OCR5AH, ZH
	sts	OCR5AL, ZL
	rjmp	1f

.L0:
	; turn off OCR5A (and disconnect)
	lds	r19, TCCR5A
	andi	r19, ~(TCCR5A_BIT_COM5A1 | TCCR5A_BIT_COM5A0)
	sts	TCCR5A, r19
	; write daft value into OCR5A to be sure
	ser	r19
	sts	OCR5AH, r19
	sts	OCR5AL, r19

.L1:
	;}}}
	;{{{  sort out OCR5B based on SND_C3VAL bits 0-1
	mov	r19, SND_C3VAL
	andi	r19, 0x03		; OCR5B setting (0-3)
	breq	0f
	
	; 25, 50 or 75% duty cycle
	movw	ZL, r16			; copy ICR5 into Z
	clc
	ror	ZH
	ror	ZL			; Z = ICR5 / 2
	cpi	r19, 0x02		; 50% duty?
	breq	2f
	cpi	r19, 0x01		; 25% duty?
	breq	3f
	; else 75% duty, so 1/4 value
	clc
	ror	ZH
	ror	ZL			; Z = ICR5 / 4
	rjmp	2f
.L3:
	; quarter-duty, so 3/4 value
	push	r24
	push	r25
	movw	r24, ZL			; r25:r24 = ICR5 / 2
	clc
	ror	r25
	ror	r24			; r25:r24 = ICR5 / 4
	add	ZL, r24
	adc	ZH, r25			; Z = ICR5 * 3/4
	pop	r25
	pop	r24
.L2:
	; turn on OCR5B (inverted mode)
	lds	r19, TCCR5A
	ori	r19, (TCCR5A_BIT_COM5B1 | TCCR5A_BIT_COM5B0)
	sts	TCCR5A, r19

	; write Z into OCR5B
	sts	OCR5BH, ZH
	sts	OCR5BL, ZL
	rjmp	1f

.L0:
	; turn off OCR5B (and disconnect)
	lds	r19, TCCR5A
	andi	r19, ~(TCCR5A_BIT_COM5B1 | TCCR5A_BIT_COM5B0)
	sts	TCCR5A, r19
	; write daft value into OCR5B to be sure
	ser	r19
	sts	OCR5BH, r19
	sts	OCR5BL, r19

.L1:
	;}}}

	pop	r19
	pop	ZL
	pop	ZH
	ret
;}}}
snd_playc3: ;{{{  play a note on channel 2, expects SND_C3VAL and V_bfreq/afreq_c3 to be set appropriately
	push	r16
	push	r17
	push	r19

	; setup ICR5 for specific note frequency
	lds	r17, V_bfreq_c3_h
	lds	r16, V_bfreq_c3_l
	sts	ICR5H, r17
	sts	ICR5L, r16

	; setup OC5A and OC5B, frequency given in r17:r16
	rcall	snd_setc3output

	pop	r19
	pop	r17
	pop	r16
	ret
;}}}
snd_playnote_c3: ;{{{  sets playing a specific note on channel 3 in r16 (NOTE_...), mod-bits in r17
	push	r16
	push	r17
	push	r18
	push	r20
	push	ZL
	push	ZH

	ldi	ZH:ZL, D_notetab
	cpi	r16, NOTE_OFF
	brne	0f
	rjmp	8f
.L0:
	cpi	r16, NOTE_C2
	brsh	0f
	rjmp	7f
.L0:
	cpi	r16, NOTE_B6+1
	brlo	0f
	rjmp	9f
.L0:
	; Note: no conditional branches past this (so can be big)

	;{{{  move mod-bits from r17 to SND_C3VAL (r9)
	mov	SND_C3VAL, r17

	;}}}
	;{{{  load regular note (0x10 - 0x4b) frequency from D_notetab into r17:r16, store in V_bfreq_c2 also
	; regular note (0x10 - 0x4b)
	subi	r16, 0x10
	lsl	r16			; r16 = offset in D_notetab
	clr	r17
	add	ZL, r16
	adc	ZH, r17

	lpm	r16, Z+			; load frequency into r17:r16
	lpm	r17, Z+

	sts	V_bfreq_c3_h, r17
	sts	V_bfreq_c3_l, r16

	;}}}
	;{{{  if tremelo bit set, set V_afreq_c2 to be next semitone up
	mov	r20, SND_C3VAL
	andi	r20, 0x40			; tremelo?
	breq	0f
	; yes, set alternate frequency to the next semitone up

	lpm	r20, Z+
	sts	V_afreq_c3_l, r20
	lpm	r20, Z+
	sts	V_afreq_c3_h, r20
.L0:
	;}}}
	;{{{  ensure timer/counter4 is stopped
	lds	r20, TCCR5B
	andi	r20, ~(TCCR5B_BIT_CS52 | TCCR5B_BIT_CS51 | TCCR5B_BIT_CS50)
	sts	TCCR5B, r20		; ensure stopped

	;}}}
	; Note: keep r20 with TCCR5B bits in
	rcall	snd_playc3

	; r20 still has TCCR5B bits in
	;{{{  re-enable timer/counter 5, prescale /8
	ori	r20, TCCR5B_BIT_CS51	; prescale /8
	sts	TCCR5B, r20

	;}}}
	rjmp	9f

	;------------------------------------------------------------------

.L7:
	; FIXME: drum handling..
	rjmp	8f

.L8:
	; turn note off
	clr	SND_C3VAL

	lds	r20, TCCR5B
	andi	r20, ~(TCCR5B_BIT_CS52 | TCCR5B_BIT_CS51 | TCCR5B_BIT_CS50)
	sts	TCCR5B, r20		; ensure stopped

	lds	r20, TCCR5A
	andi	r20, ~(TCCR5A_BIT_COM5A1 | TCCR5A_BIT_COM5A0 | TCCR5A_BIT_COM5B1 | TCCR5A_BIT_COM5B0)
	sts	TCCR5A, r20		; disconnect OC4A and OC4B


.L9:
	pop	ZH
	pop	ZL
	pop	r20
	pop	r18
	pop	r17
	pop	r16
	ret


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
	mov	LINE_REGION, r16
	ldi	r16, 0
	mov	LINE_COUNT, r16

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
fb_clearall: ;{{{  clears the framebuffer (both of them)
	push	r16
	push	r24
	push	r25
	push	XL
	push	XH

	ldi	XH:XL, V_framebuffer1
	ldi	r25:r24, (HRES * VRES * 2)
	clr	r16
.L1:
	st	X+, r16
	sbiw	r25:r24, 1
	brne	1b

	pop	XH
	pop	XL
	pop	r25
	pop	r24
	pop	r16
	ret
;}}}
fb_clear: ;{{{  clears the framebuffer
	push	r16
	push	r24
	push	r25
	push	XL
	push	XH

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	clr	XL			; framebuffers start at known addresses

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

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	clr	XL			; framebuffers start at known addresses

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
fb_xxsetpixel: ;{{{  sets a pixel at r16,r17 (always sets): hardcoded to know about HRES=16
	push	r18
	push	r19
	push	XL
	push	XH

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	mov	XL, r17
	swap	XL			; low-order bits * 16
	mov	r18, XL
	andi	r18, 0x0f		; save high-order bits of Y
	andi	XL, 0xf0		; cull these in X
	add	XH, r18			; add these in

	ldi	r19, 0x80		; bit we'll add in
	mov	r18, r16
	sbrc	r18, 0			; shift r19 right appropriately
	lsr	r19
	sbrc	r18, 1
	lsr	r19
	sbrc	r18, 1
	lsr	r19
	sbrc	r18, 2
	swap	r19			; swap rather than shift by 4

	lsr	r18
	lsr	r18
	lsr	r18			; r18 = X/8  (now 0-15)
	or	XL, r18			; pick right address

	ld	r18, X			; set bit in framebuffer
	or	r18, r19
	st	X, r18

	pop	XH
	pop	XL
	pop	r19
	pop	r18
	ret
;}}}
fb_xhline: ;{{{  draw horizontal line from (r16,r17) for r18: hardcoded to know about HRES=16
	push	r16
	push	r18
	push	r19
	push	r20
	push	XL
	push	XH

	; setup X register for framebuffer, correct line to start
	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	mov	XL, r17
	swap	XL			; low-order bits * 16
	mov	r19, XL
	andi	r19, 0x0f		; save high-order bits of Ypos
	andi	XL, 0xf0		; cull these in XL
	add	XH, r19			; add these in

	; assert: XH:XL points at the start of the relevant line
	mov	r19, r16
	lsr	r19
	lsr	r19
	lsr	r19			; r19 = X / 8, starting byte in line
	or	XL, r19			; set X to relevant start byte

	mov	r19, r16		; 
	andi	r19, 0x07		; r19 = Xpos % 8
	add	r19, r18		;       + Length
	andi	r19, 0xf8		; look at high-order bits
	brne	0f			; branch if more than 1 byte of framebuffer
	; assert: relevant line fits in a single framebuffer byte
	rjmp	5f
.L0:
	mov	r19, r16
	andi	r19, 0x07		; starting at multiple of 8?
	breq	2f			; yes, so skip partial filling

	; assert: multiple bytes needed, so first and last may be partially filled
	ldi	r20, 0xff		; fill all pixels by default

	sbrc	r16, 0			; shift r20 right appropriately
	lsr	r20
	sbrc	r16, 1
	lsr	r20
	sbrc	r16, 1
	lsr	r20
	sbrc	r16, 2
	swap	r20
	sbrc	r16, 2
	andi	r20, 0x0f		; r20 now has the correct starting-bit pattern

	; fill in first byte
	ld	r19, X
	or	r19, r20
	st	X+, r19

	; adjust length appropriately
	mov	r19, r16
	dec	r19
	ldi	r20, 0x07
	eor	r19, r20
	andi	r19, 0x07		; r19 is now the number of pixels we just filled in
	sub	r18, r19		; and remove from length

.L2:
	; for each block of 8 pixels, just fill in the byte at X
	ldi	r20, 0xff
.L0:
	cpi	r18, 8
	brlo	1f			; if less than 8 left, special handling
	st	X+, r20			; fill in this one
	subi	r18, 8
	rjmp	0b			; and round again
.L1:
	; some bits left over maybe
	cpi	r18, 0
	breq	2f			; do nothing if none left :)
	dec	r18
	ldi	r20, 0x80		; starting bit
	sec
	sbrc	r18, 0
	ror	r20			; filled bit
	sec
	sbrc	r18, 1
	ror	r20			; filled bit
	sec
	sbrc	r18, 1
	ror	r20			; filled bit
	sbrc	r18, 2			; if bit 2 is set, paint upper nibble
	swap	r20
	sbrc	r18, 2
	ori	r20, 0xf0

	ld	r19, X
	or	r19, r20
	st	X, r19			; put last byte in place
.L2:
	; and we're done :)

	pop	XH
	pop	XL
	pop	r20
	pop	r19
	pop	r18
	pop	r16
	ret

	; Note: put this code down here to stay clear of [short] conditional branches
.L5:					; horizontal line fits in byte
	; r18 has the number of bits we need to set (1-7), XH:XL points at the relevant framebuffer byte
	dec	r18
	ldi	r20, 0x80		; starting bit
	sec
	sbrc	r18, 0
	ror	r20			; filled bit
	sec
	sbrc	r18, 1
	ror	r20			; filled bit
	sec
	sbrc	r18, 1
	ror	r20			; filled bit
	sbrc	r18, 2			; if bit 2 set, paint upper nibble
	swap	r20
	sbrc	r18, 2
	ori	r20, 0xf0

	andi	r16, 0x07
.L0:
	breq	1f			; branch of nowt left
	lsr	r20			; shift bits right (others will fall off end)
	dec	r16
	rjmp	0b			; keep status bits
.L1:
	; and park
	ld	r16, X
	or	r16, r20
	st	X, r16

	rjmp	2b			; jump back to get out

;}}}
fb_xvline: ;{{{  draw vertical line from (r16,r17) for r18: hardcoded to know about HRES=16
	push	r16
	push	r18
	push	r19
	push	XL
	push	XH

	; setup X register for framebuffer
	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	mov	XL, r17
	swap	XL			; low-order bits * 16
	mov	r19, XL
	andi	r19, 0x0f		; save high-order bits of Y
	andi	XL, 0xf0		; cull these in X
	add	XH, r19			; add these in

	ldi	r19, 0x80		; bit we'll add in
	sbrc	r16, 0			; shift r19 right appropriately
	lsr	r19
	sbrc	r16, 1
	lsr	r19
	sbrc	r16, 1
	lsr	r19
	sbrc	r16, 2
	swap	r19			; swap rather than shift by 4

	lsr	r16
	lsr	r16
	lsr	r16			; r16 = X/8  (now 0-15)
	or	XL, r16			; pick right address

	; Note: don't need r16 anymore, r17 must be untouched
.L0:
	ld	r16, X			; set bit in framebuffer
	or	r16, r19
	st	X, r16

	adiw	XH:XL, 16		; next line please
	dec	r18
	brne	0b

	pop	XH
	pop	XL
	pop	r19
	pop	r18
	pop	r16

	ret


;}}}
fb_setbyte: ;{{{  write the 8-bits in r18 to the framebuffer at position (r16*8, r17)
	push	r19
	push	XL
	push	XH

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	mov	XL, r17
	swap	XL			; low-order bits * 16
	mov	r19, XL
	andi	r19, 0x0f		; save high-order bits of Ypos
	andi	XL, 0xf0		; cull these in XL
	add	XH, r19			; add these in

	; assert: XH:XL points at the start of the relevant line
	or	XL, r16			; appropriate byte

	st	X, r18

	pop	XH
	pop	XL
	pop	r19
	ret


;}}}
fb_writechar: ;{{{  writes the ASCII char in r18 to the framebuffer at position (r16, r17) using 5x7 font.
	; Note: r17 may be outside the framebuffer (<0 or >=96), this will deal with that sensibly.

	; check for valid character (obviously :))
	cpi	r18, 32
	brsh	0f
	rjmp	fb_writechar_out
.L0:
	cpi	r18, 128
	brlo	0f
	rjmp	fb_writechar_out
.L0:
	cpi	r17, 96
	brlo	0f			; unsigned, so range 0-95
	cpi	r17, -6
	brsh	0f			; between -6 and -1 then
	; else outside of visible Y range
	rjmp	fb_writechar_out
.L0:
	push	r0
	push	r1
	push	r17
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

	ldi	r21, 7			; default number of lines to write
	cpi	r17, 250
	brlo	0f			; if Y >= 0
.L1:
	; here means we're drawing off the top somewhere (Y<0), increment until Y==0
	adiw	ZH:ZL, 1		; next line in character data
	dec	r21			; one less line of character data
	inc	r17			; Y++
	brne	1b			; loop if still != 0
.L0:
	cpi	r17, 90			; if >= line 90, then can't see all of the character
	brlo	0f
	; here means we're drawing off the bottom somewhere (Y>=90), better do fewer lines
	ldi	r19, 89
	sub	r19, r17
	neg	r19			; r19 = Y-90 (number of lines we can see still)
	sub	r21, r19
.L0:

	; point X at the first byte in the framebuffer of interest
	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	mov	XL, r17
	swap	XL			; low-order bits * 16
	mov	r19, XL
	andi	r19, 0x0f		; save high-order bits of Ypos
	andi	XL, 0xf0		; cull these in XL
	add	XH, r19			; add these in

	; assert: XH:XL points at the start of the relevant line
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
	pop	r17
	pop	r1
	pop	r0
fb_writechar_out:
	ret

	; helpers for particular offsets:
	;
	; XH:XL = address of byte in FB
	; ZH:ZL = address of first character byte (5 LSBs)
	; r16,r17 = pos; r18 = char; r21 = count (7 for all-visible); r19-r20,r0-r1 = available.
fb_writechar_offs0:
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
fb_write04char: ;{{{  writes the ASCII char in r18 to the framebuffer at position (r16, r17) using 04b font.  Updates r16 for X width.
	; check for valid character (obviously :))
	cpi	r18, 32
	brsh	0f
	rjmp	fb_write04char_out
.L0:
	cpi	r18, 127
	brlo	0f
	rjmp	fb_write04char_out
.L0:
	; check X and Y in bounds
	cpi	r17, 96
	brlo	0f
	cpi	r17, 240
	brsh	0f
	rjmp	fb_write04char_out
.L0:
	cpi	r16, 128
	brlo	0f
	cpi	r16, 240
	brsh	0f
	rjmp	fb_write04char_out
.L0:
	push	ZH
	push	ZL
	push	XH
	push	XL
	push	r22
	push	r21
	push	r20
	push	r19
	push	r18
	push	r17
	push	r1
	push	r0

	subi	r18, 32					; normalise [0-94]

	clr	r19					; get address of font metric
	ldi	ZH:ZL, font_metrics_04b
	add	ZL, r18
	adc	ZH, r19

	lpm	r20, Z

	ldi	r19, 32					; get address of font data
	mul	r18, r19
	ldi	ZH:ZL, font_data_04b
	add	ZL, r0
	adc	ZH, r1

	; point X at the first byte in the framebuffer of interest
	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	; fix for negative Y offsets up to -16
	ldi	r22, 16			; default height is 16
	;{{{  setup for out-of-bounds Y => r17, r22, ZH:ZL left with modified Y/height/pointer
	cpi	r17, 240
	brlo	0f			; Y >= 0
	mov	r19, r17
	neg	r19
	sub	r22, r19		; adjust height to accomodate lost rows
	lsl	r19			; multiply inverted Y by 2 for byte offset in font data
	clr	r17			; and make Y offset 0 (top)
	add	ZL, r19
	adc	ZH, r17			; borrowed zero
	rjmp	1f
.L0:
	; check to see if we run off the bottom of the display, and adjust height (r22) as needed
	mov	r19, r17
	ldi	r21, 15
	add	r19, r21		; r19 is potential last line
	ldi	r21, 96
	cp	r19, r21
	brlo	1f			; uninteresting if last-Y < 96
	ldi	r22, 111		; magic! (96 + 15)
	sub	r22, r19		; subtract potential last-Y (>=96) to get visible rows.
.L1:
	;}}}
	; r17 contains the starting line in the framebuffer (0..95)
	mov	XL, r17
	swap	XL			; low-order bits * 16
	mov	r19, XL
	andi	r19, 0x0f		; save high-order bits of Ypos
	andi	XL, 0xf0		; cull these in XL
	add	XH, r19			; add these in

	;{{{  check for negative X (r16) and/or out-of-bounds (<=-16, >=128)  => jump to specific X rendering versions
	cpi	r16, 113
	brlo	1f			; quick decision if 0 <= X < 113
	cpi	r16, 249		; -7
	brlo	0f
	rjmp	fb_write04char_xneg1	; if (-7 .. -1) then missing first byte
.L0:
	cpi	r16, 241		; -15
	brlo	0f
	rjmp	fb_write04char_xneg2	; if (-15 .. -8) then missing first two bytes
.L0:
	cpi	r16, 120
	brlo	0f
	rjmp	fb_write04char_xpos2	; if (121 .. 127) then missing last two bytes
.L0:
	rjmp	fb_write04char_xpos1	; if (113 .. 120) then missing last byte
.L1:
	;}}}

	; assert: XH:XL points at the start of the relevant line
	mov	r19, r16
	lsr	r19
	lsr	r19
	lsr	r19			; r19 = X / 8

	clr	r21
	add	XL, r19			; X is starting byte in video
	adc	XH, r21

	; r20 has font-metrics byte still
	mov	r19, r20
	andi	r19, 0x0f		; low-order bits (character data width, 0=16)
	brne	0f
	ldi	r19, 16
.L0:
	; r19 has relevant bit-width
	mov	r18, r20
	swap	r18
	andi	r18, 0x0f		; high-order bits (display width, 0=16, <char-width = 16+X)
	add	r18, r19		; r18 now has display width (i.e. how much we adjust r16 by).

	; try and be vaguely efficient in how we branch here..
	mov	r20, r16
	andi	r20, 0x07		; bit offset (X % 8)
	sbrc	r20, 2
	rjmp	1f			; branch if bit offsets 4-7
	; assert: bit offset is 0-3
	sbrc	r20, 1
	rjmp	2f			; branch if bit offset 2-3
	sbrc	r20, 0
	rjmp	fb_write04char_offs1	; branch if bit offset 1
	rjmp	fb_write04char_offs0	; else must be 0
.L2:
	; assert: bit offset is 2-3
	sbrc	r20, 0
	rjmp	fb_write04char_offs3	; branch if bit offset 3
	rjmp	fb_write04char_offs2	; else must be 2
.L1:
	; assert: bit offset is 4-7
	sbrc	r20, 1
	rjmp	2f			; branch if bit offset 6-7
	sbrc	r20, 0
	rjmp	fb_write04char_offs5	; branch if bit offset 5
	rjmp	fb_write04char_offs4	; else must be 4
.L2:
	; assert: bit offset is 6-7
	sbrc	r20, 0
	rjmp	fb_write04char_offs7	; branch if bit offset 7
	rjmp	fb_write04char_offs6	; else must be 6

fb_write04char_offs7:
	;{{{  plant character at bit offset 7.
.L0:
	clr	r20

	lpm	r0, Z+
	lsl	r0
	rol	r20			; put high-order bit into r20 low-order

	ld	r1, X			; write first video byte
	or	r1, r20
	st	X+, r1

	mov	r20, r0			; low-lorder 7 bits from first byte

	clr	r1
	lpm	r0, Z+
	lsl	r0
	rol	r1			; put high-order bit into r1 low-order

	or	r1, r20			; put back saved 7 bits from first byte
	mov	r20, r0			; low-lorder 7 bits from second byte

	ld	r0, X			; write second video byte
	or	r0, r1
	st	X+, r0

	ld	r1, X			; write third video byte
	or	r1, r20			; put in saved bits
	st	X+, r1

	adiw	XH:XL, 13		; next line please
	
	dec	r22
	brne	0b
	;}}}
	rjmp	fb_write04char_cont
fb_write04char_offs6:
	;{{{  plant character at bit offset 6.
.L0:
	clr	r20

	lpm	r0, Z+
	lsl	r0
	rol	r20
	lsl	r0
	rol	r20			; put high-order 2 bits into r20 low-order

	ld	r1, X			; write first video byte
	or	r1, r20
	st	X+, r1

	mov	r20, r0			; low-lorder 6 bits from first byte

	clr	r1
	lpm	r0, Z+
	lsl	r0
	rol	r1
	lsl	r0
	rol	r1			; put high-order 2 bits into r1 low-order

	or	r1, r20			; put back saved 6 bits from first byte
	mov	r20, r0			; low-lorder 6 bits from second byte

	ld	r0, X			; write second video byte
	or	r0, r1
	st	X+, r0

	ld	r1, X			; write third video byte
	or	r1, r20			; put in saved bits
	st	X+, r1

	adiw	XH:XL, 13		; next line please
	
	dec	r22
	brne	0b
	;}}}
	rjmp	fb_write04char_cont
fb_write04char_offs5:
	;{{{  plant character at bit offset 5.
.L0:
	clr	r20

	lpm	r0, Z+
	lsl	r0
	rol	r20
	lsl	r0
	rol	r20
	lsl	r0
	rol	r20			; put high-order 3 bits into r20 low-order

	ld	r1, X			; write first video byte
	or	r1, r20
	st	X+, r1

	mov	r20, r0			; low-lorder 5 bits from first byte

	clr	r1
	lpm	r0, Z+
	lsl	r0
	rol	r1
	lsl	r0
	rol	r1
	lsl	r0
	rol	r1			; put high-order 3 bits into r1 low-order

	or	r1, r20			; put back saved 5 bits from first byte
	mov	r20, r0			; low-lorder 5 bits from second byte

	ld	r0, X			; write second video byte
	or	r0, r1
	st	X+, r0

	ld	r1, X			; write third video byte
	or	r1, r20			; put in saved bits
	st	X+, r1

	adiw	XH:XL, 13		; next line please
	
	dec	r22
	brne	0b
	;}}}
	rjmp	fb_write04char_cont
fb_write04char_offs4:
	;{{{  plant character at bit offset 4.
.L0:
	lpm	r0, Z+
	swap	r0
	mov	r20, r0			; save leftover low-order bits here for now
	andi	r20, 0x0f		; mask in high nibble (swapped)

	ld	r1, X			; write first video byte
	or	r1, r20
	st	X+, r1

	mov	r20, r0			; ditto
	andi	r20, 0xf0		; pick out low nibble (swapped)
	mov	r1, r20

	lpm	r0, Z+
	swap	r0
	mov	r20, r0			; save leftover low-order bits here for now
	andi	r20, 0x0f		; mask in high nibble (swapped)
	or	r1, r20			; put back into saved bits from first byte

	mov	r20, r0			; becomes saved bit for next byte
	andi	r20, 0xf0		; pick out low nibble (swapped)

	ld	r0, X			; write second video byte
	or	r0, r1
	st	X+, r0

	ld	r1, X			; write third video byte
	or	r1, r20			; put in saved bits
	st	X+, r1

	adiw	XH:XL, 13		; next line please
	
	dec	r22
	brne	0b
	;}}}
	rjmp	fb_write04char_cont
fb_write04char_offs3:
	;{{{  plant character at bit offset 3.
.L0:
	clr	r20

	lpm	r0, Z+
	lsr	r0
	ror	r20
	lsr	r0
	ror	r20
	lsr	r0
	ror	r20			; save leftover bits here for now

	ld	r1, X			; write first video byte
	or	r1, r0
	st	X+, r1

	clr	r1
	lpm	r0, Z+
	lsr	r0
	ror	r1
	lsr	r0
	ror	r1
	lsr	r0
	ror	r1			; save leftover bits here for now
	or	r0, r20			; put back in saved bit from first byte
	mov	r20, r1			; becomes saved bit for next byte

	ld	r1, X			; write second video byte
	or	r1, r0
	st	X+, r1

	ld	r1, X			; write third video byte
	or	r1, r20			; put in saved bits
	st	X+, r1

	adiw	XH:XL, 13		; next line please
	
	dec	r22
	brne	0b
	;}}}
	rjmp	fb_write04char_cont
fb_write04char_offs2:
	;{{{  plant character at bit offset 2.
.L0:
	clr	r20

	lpm	r0, Z+
	lsr	r0
	ror	r20
	lsr	r0
	ror	r20			; save leftover bits here for now

	ld	r1, X			; write first video byte
	or	r1, r0
	st	X+, r1

	clr	r1
	lpm	r0, Z+
	lsr	r0
	ror	r1
	lsr	r0
	ror	r1			; save leftover bits here for now
	or	r0, r20			; put back in saved bit from first byte
	mov	r20, r1			; becomes saved bit for next byte

	ld	r1, X			; write second video byte
	or	r1, r0
	st	X+, r1

	ld	r1, X			; write third video byte
	or	r1, r20			; put in saved bits
	st	X+, r1

	adiw	XH:XL, 13		; next line please
	
	dec	r22
	brne	0b
	;}}}
	rjmp	fb_write04char_cont
fb_write04char_offs1:
	;{{{  plant character at bit offset 1.
.L0:
	clr	r20

	lpm	r0, Z+
	lsr	r0
	ror	r20			; save leftover bit here for now

	ld	r1, X			; write first video byte
	or	r1, r0
	st	X+, r1

	clr	r1
	lpm	r0, Z+
	lsr	r0
	ror	r1			; save leftover bit here for now
	or	r0, r20			; put back in saved bit from first byte
	mov	r20, r1			; becomes saved bit for next byte

	ld	r1, X			; write second video byte
	or	r1, r0
	st	X+, r1

	ld	r1, X			; write third video byte
	or	r1, r20			; put in saved bits
	st	X+, r1

	adiw	XH:XL, 13		; next line please
	
	dec	r22
	brne	0b
	;}}}
	rjmp	fb_write04char_cont
fb_write04char_offs0:
	;{{{  plant character at bit offset 0.
.L0:
	lpm	r0, Z+
	st	X+, r0
	lpm	r0, Z+
	st	X+, r0
	adiw	XH:XL, 14		; next line please
	
	dec	r22
	brne	0b
	;}}}
	rjmp	fb_write04char_cont

	; FIXME: incomplete!
	;{{{  blocks for handling one byte missing at front (X < 0)
fb_write04char_xneg1:
	rjmp	fb_write04char_cont



	;}}}
	;{{{  blocks for handling two bytes missing at start (X < -7)
fb_write04char_xneg2:
	rjmp	fb_write04char_cont

	;}}}
	;{{{  blocks for handling one byte missing at end (X > 112)
fb_write04char_xpos1:
	rjmp	fb_write04char_cont

	;}}}
	;{{{  blocks for handling two bytes missing at end (X > 120)
fb_write04char_xpos2:
	rjmp	fb_write04char_cont

	;}}}

fb_write04char_cont:
	add	r16, r18		; adjust for display width

	pop	r0
	pop	r1
	pop	r17
	pop	r18
	pop	r19
	pop	r20
	pop	r21
	pop	r22
	pop	XL
	pop	XH
	pop	ZL
	pop	ZH
fb_write04char_out:
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

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	mov	XL, r17
	swap	XL			; low-order bits * 16
	mov	r20, XL
	andi	r20, 0x0f		; save high-order bits of Ypos
	andi	XL, 0xf0		; cull these in XL
	add	XH, r20			; add these in

	; assert: XH:XL points at the start of the relevant line
	or	XL, r16			; appropriate byte

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

fb_drawline_redo:
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

	; assert: vertical line, from (r16,r17) for r19
	tst	r19
	brpl	1f

	; flip around
	neg	r19
	sub	r17, r19
	rjmp	fb_drawline_redo

.L1:
	; assert: vertical line, from (r16,r17) for r19
	mov	r18, r19
	rcall	fb_xvline
	rjmp	9f

.L0:
	cpi	r19, 0
	brne	0f

	; assert: horizontal line, from (r16,r17) for r18
	tst	r18
	brpl	1f

	; flip around
	neg	r18
	sub	r16, r18
	rjmp	fb_drawline_redo

.L1:
	; assert: horizontal line, from (r16,r17) for r18
	rcall	fb_xhline
	rjmp	9f

.L0:
	; assert: some diagonal line sort
	; NOTE:
	;	r20 = d
	;	r10 = strt
	;	r11 = diag
	;
	;	r12 = adx,	r13 = ady
	;	r14 = sx,	r15 = sy
	;	r16 = x,	r17 = y
	;	r18 = dx,	r19 = dy

	;{{{  init adx, sx
	tst	r18
	brpl	0f

	mov	r12, r18
	neg	r12		; adx = -dx
	clr	r14
	dec	r14		; dx = -1
	rjmp	1f
.L0:
	mov	r12, r18	; adx = dx
	clr	r14
	inc	r14		; sx = 1
.L1:

	;}}}
	;{{{  init ady, sy
	tst	r19
	brpl	0f

	mov	r13, r19
	neg	r13		; ady = -dy
	clr	r15
	dec	r15		; sy = -1
	rjmp	1f
.L0:
	mov	r13, r19	; ady = dy
	clr	r15
	inc	r15		; sy = 1
.L1:
	;}}}

	cp	r12, r13
	brsh	0f
	rjmp	1f
.L0:
	;{{{  adx >= ady
	mov	r10, r13
	lsl	r10		; strt = 2 * ady
	mov	r11, r13
	sub	r11, r12
	lsl	r11		; diag = 2 * (ady - adx)
	mov	r20, r10
	sub	r20, r12	; d = strt - adx
.L2:				; while (run) ...
	tst	r12
	brmi	3f		; if (adx < 0), stop
	tst	r16
	brmi	3f		; if ((x < 0) || (x >= 128)), stop
	tst	r17
	brmi	3f		; if (y < 0), stop
	cpi	r17, 96
	brsh	3f		; if (y >= 96), stop

	;{{{  guts of X render loop
	rcall	fb_xxsetpixel	; plot point
	add	r16, r14	; x += sx
	dec	r12		; adx--
	tst	r20
	breq	4f		; if (d == 0)
	brmi	4f		;    || (d < 0), ..
	; assert: d > 0
	add	r20, r11	; d += diag
	add	r17, r15	; y += sy
	rjmp	2b
.L4:
	; assert: d <= 0
	add	r20, r10	; d += strt
	rjmp	2b
	;}}}

.L3:
	;}}}
	rjmp	9f		; all done, get out :)
.L1:
	;{{{  ady > adx
	mov	r10, r12
	lsl	r10		; strt = 2 * adx
	mov	r11, r12
	sub	r11, r13
	lsl	r11		; diag = 2 * (adx - ady)
	mov	r20, r10
	sub	r20, r13	; d = strt - ady
.L2:				; while (run) ...
	tst	r13
	brmi	3f		; if (ady < 0), stop
	tst	r17
	brmi	3f		; if (y < 0), stop
	cpi	r17, 96
	brsh	3f		; if (y >= 96), stop
	tst	r16
	brmi	3f		; if ((x < 0) || (x >= 128)), stop

	;{{{  guts of Y render loop
	rcall	fb_xxsetpixel	; plot point
	add	r17, r15	; y += sy
	dec	r13		; ady--
	tst	r20
	breq	4f		; if (d == 0)
	brmi	4f		;    || (d < 0)
	; assert: d > 0
	add	r20, r11	; d += diag
	add	r16, r14	; x += sx
	rjmp	2b
.L4:
	; assert: d <= 0
	add	r20, r10	; d += strt
	rjmp	2b
	;}}}

.L3:
	;}}}

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
fb_xdrawline: ;{{{  draws a line starting at (r16,r17) [unsigned] for (r18,r19) [signed]: hard-coded to know about HRES=16
	push	r16
	push	r17
	push	r18
	push	r19
	push	r20
	push	r21
	push	r22
	push	r23
	push	XL
	push	XH

fb_xdrawline_redo:
	;{{{  check for horizontal and vertical lines first (jmp to L9)
	cpi	r17, 96
	brlo	0f			; in range (unsigned)
	rjmp	9f			; outside raster
.L0:
	tst	r16
	brpl	0f			; is positive
	rjmp	9f			; negative, so outside raster (left or right = same thing)
.L0:
	cpi	r18, 0			; vertical line?
	brne	0f			; nope -> branch

	; assert: vertical line, from (r16,r17) for r19
	tst	r19
	brpl	1f			; direction is positive

	; else, negative line, flip around
	neg	r19
	sub	r17, r19		; move Y down by the right amount
	rjmp	fb_xdrawline_redo	; start again (need to check that we're in the raster still)
.L1:
	; assert: vertical line positive, from (r16,r17) for r19
	mov	r18, r19
	rcall	fb_xvline
	rjmp	9f			; all done, get out!

.L0:	; not a vertical line
	cpi	r19, 0			; horizontal line?
	brne	0f			; nope -> branch

	; assert: horizontal line, from (r16,r17) for r18
	tst	r18
	brpl	1f

	; else, negative size, flip around
	neg	r18
	sub	r16, r18		; move X back by the right amount
	rjmp	fb_xdrawline_redo	; start again (need to check that we're in the raster still)
.L1:
	; assert: horizontal line, from (r16,r17) for r18
	rcall	fb_xhline
	rjmp	9f
.L0:
	;}}}
	; assert: some diagonal line sort
	; NOTE:
	;	r20 = d
	;	r10 = strt
	;	r11 = diag
	;	r21 = running bit
	;	r22 = temp
	;
	;	r23[0] = sx-neg, r23[1] = sy-neg
	;
	;	r12 = adx,	r13 = ady
	;	r16 = x,	r17 = y				; params
	;	r18 = dx,	r19 = dy			; params
	;
	;	XH:XL = running framebuffer offset

	clr	r23		; because we'll use this for storing both direction flags

	;{{{  init adx, sx
	mov	r12, r18	; adx = dx
	tst	r18
	brpl	0f

	neg	r12		; adx = -adx
	ori	r23, 0x01
.L0:

	;}}}
	;{{{  init ady, sy
	mov	r13, r19	; ady = dy
	tst	r19
	brpl	0f

	neg	r13		; ady = -ady
	ori	r23, 0x02
.L0:
	;}}}
	;{{{  setup XH:XL for right framebuffer start byte (trashes r20)
	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	mov	XL, r17
	swap	XL			; low-order bits of Y * 16
	mov	r20, XL
	andi	r20, 0x0f		; save high-order bits of Ypos
	andi	XL, 0xf0		; cull in XL
	add	XH, r20			; add these in

	; assert: XH:XL points at the start of the relevant framebuffer line
	mov	r20, r16
	lsr	r20
	lsr	r20
	lsr	r20
	or	XL, r20			; XH:XL now points at correct starting byte (line + (X/8))
	;}}}
	;{{{  setup r21 for the right start bit (based on starting X position in r16)
	ldi	r21, 0x80
	sbrc	r16, 0
	lsr	r21
	sbrc	r16, 1
	lsr	r21
	sbrc	r16, 1
	lsr	r21
	sbrc	r16, 2
	swap	r21			; swap rather than shift by 4

	;}}}

	cp	r12, r13
	brsh	0f
	rjmp	1f
.L0:
	;{{{  adx >= ady
	mov	r10, r13
	lsl	r10		; strt = 2 * ady
	mov	r11, r13
	sub	r11, r12
	lsl	r11		; diag = 2 * (ady - adx)
	mov	r20, r10
	sub	r20, r12	; d = strt - adx
.L2:				; while (run) ...
	tst	r12
	brmi	3f		; if (adx < 0), stop
	tst	r16
	brmi	3f		; if ((x < 0) || (x >= 128)), stop
	tst	r17
	brmi	3f		; if (y < 0), stop
	cpi	r17, 96
	brsh	3f		; if (y >= 96), stop

	;{{{  guts of X render loop
	ld	r22, X		; plot point
	or	r22, r21	;
	st	X, r22		;

	sbrc	r23, 0		; skip if sx == 1
	rjmp	21f
	; assert: sx == 1
	inc	r16		; x++
	lsr	r21		; plotted point right one
	brcc	22f
	ldi	r21, 0x80	; fell-out RHS, wrap to other side
	inc	XL		; and next byte
	rjmp	22f
.L21:
	; assert: sx == -1
	dec	r16		; x--
	lsl	r21		; plotted point left one
	brcc	22f
	ldi	r21, 0x01	; fell-out LHS, wrap to other side
	dec	XL		; and previous byte
.L22:

	dec	r12		; adx--
	tst	r20
	breq	4f		; if (d == 0)
	brmi	4f		;    || (d < 0), ..
	; assert: d > 0
	add	r20, r11	; d += diag

	sbrc	r23, 1		; skip if sy == 1
	rjmp	23f		; jump if sy == -1
	; assert: sy == 1, so line down
	inc	r17		; y++
	adiw	XH:XL, HRES	; next line
	rjmp	24f
.L23:
	; assert: sy == -1, so line up
	dec	r17		; y--
	sbiw	XH:XL, HRES	; previous line
.L24:
	rjmp	2b
.L4:
	; assert: d <= 0
	add	r20, r10	; d += strt
	rjmp	2b
	;}}}

.L3:
	;}}}
	rjmp	9f		; all done, get out :)
.L1:
	;{{{  ady > adx
	mov	r10, r12
	lsl	r10		; strt = 2 * adx
	mov	r11, r12
	sub	r11, r13
	lsl	r11		; diag = 2 * (adx - ady)
	mov	r20, r10
	sub	r20, r13	; d = strt - ady
.L2:				; while (run) ...
	tst	r13
	brmi	3f		; if (ady < 0), stop
	tst	r17
	brmi	3f		; if (y < 0), stop
	cpi	r17, 96
	brsh	3f		; if (y >= 96), stop
	tst	r16
	brmi	3f		; if ((x < 0) || (x >= 128)), stop

	;{{{  guts of Y render loop
	ld	r22, X		; plot point
	or	r22, r21	;
	st	X, r22		;

	sbrc	r23, 1		; skip if sy == 1
	rjmp	21f
	; assert: sy == 1
	inc	r17		; y++
	adiw	XH:XL, HRES	; next line down
	rjmp	22f
.L21:
	; assert: sy == -1
	dec	r17		; y--
	sbiw	XH:XL, HRES	; previous line up
.L22:
	dec	r13		; ady--
	tst	r20
	breq	4f		; if (d == 0)
	brmi	4f		;    || (d < 0)
	; assert: d > 0
	add	r20, r11	; d += diag
	sbrc	r23, 0		; skip if sx == 1
	rjmp	23f
	; assert: sx == 1, so increment and maybe roll-right
	inc	r16
	lsr	r21
	brcc	24f
	ldi	r21, 0x80	; fell-out RHS wrap to other side
	inc	XL		; and next byte along
	rjmp	24f
.L23:
	; assert: sx == -1, so decrement and maye roll-elft
	dec	r16
	lsl	r21
	brcc	24f
	ldi	r21, 0x01	; fell-out LHS, wrap to other side
	dec	XL		; and previous byte
.L24:
	rjmp	2b
.L4:
	; assert: d <= 0
	add	r20, r10	; d += strt
	rjmp	2b
	;}}}

.L3:
	;}}}
.L9:					; get out here.
	pop	XH
	pop	XL
	pop	r23
	pop	r22
	pop	r21
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	ret
;}}}

math_sinv: ;{{{  computes r16 * sin(r17), returns high-order bits (signed) in r16, trashes r17
	push	r0
	push	r1
	push	ZL
	push	ZH

	ldi	ZH:ZL, D_sin_table
	clr	r0
	add	ZL, r17
	adc	ZH, r0
	lpm	r17, Z			; load sin(r17) into r17
	mulsu	r17, r16
	mov	r16, r1			; high-order bits of the result

	pop	ZH
	pop	ZL
	pop	r1
	pop	r0
	ret

;}}}
math_cosv: ;{{{  computes r16 * cos(r17), returns high-order bits (signed) in r16, trashes r17
	push	r0
	push	r1
	push	ZL
	push	ZH

	ldi	ZH:ZL, D_cos_table
	clr	r0
	add	ZL, r17
	adc	ZH, r0
	lpm	r17, Z			; load sin(r17) into r17
	mulsu	r17, r16
	mov	r16, r1			; high-order bits of the result

	pop	ZH
	pop	ZL
	pop	r1
	pop	r0
	ret

;}}}

vid_waitbot: ;{{{  waits for the render to reach bottom of visible area (so we can start updating things)
	push	r16

.L0:
	mov	r16, LINE_REGION	; basically wait for (r4 == 1) and (r5 == (PAL_BBLANK_LINES-1)).
	andi	r16, 0x03
	cpi	r16, 1
	brne	0b			; nope, wait some more
	mov	r16, LINE_COUNT
	cpi	r16, PAL_BBLANK_LINES-1
	brne	0b			; nope, wait some more (will be waiting a long time here..)

	; yep, at start of bottom blanking region.
	; this gives us basically 120 PAL lines worth of time before the next frame is drawn <= 7.68ms ~= 122880 cycles at 16 MHz --- MANY LOTS :-)
	; in the order of 5-6k cycles will be consumed with interrupt handling, but still leaving in excess of 100k cycles.
	
	pop	r16
	ret
;}}}
vid_waitbot1: ;{{{  waits for the render to reach just after bottom of visible area (forces delay with the above)
	push	r16

.L0:
	mov	r16, LINE_REGION	; basically wait for (r4 == 1) and (r5 == (PAL_BBLANK_LINES-2)).
	andi	r16, 0x03
	cpi	r16, 1
	brne	0b			; nope, wait some more
	mov	r16, LINE_COUNT
	cpi	r16, PAL_BBLANK_LINES-2
	brne	0b			; nope, wait some more (will be waiting a long time here..)

	; yep, in 2nd line of bottom blanking region.
	; this gives us basically 119 PAL lines worth of time before the next frame is drawn <= 7.616ms ~= 121856 cycles at 16 MHz --- MANY LOTS :-)
	pop	r16
	ret

;}}}
vid_waitflip: ;{{{  waits for the current rendering to finish (even if it's not started yet..) and flips the framebuffer
	push	r16
	rcall	vid_waitbot
	ldi	r16, 0x80
	eor	LINE_REGION, r16	; flip top bit
	pop	r16
	ret
;}}}

demo_renderlines: ;{{{  renders the lines defined in SRAM
	push	r16
	push	r17
	push	r18
	push	r19
	push	r20
	push	XH
	push	XL

	ldi	XH:XL, V_rdrpnts
	ldi	r20, 8
.L0:
	ld	r16, X+
	ld	r17, X+
	ld	r18, X+
	ld	r19, X+
	call	fb_xdrawline
	dec	r20
	brne	0b

	pop	XL
	pop	XH
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	ret

;}}}
demo_starfield_init: ;{{{  initialise starfield variables
	push	r16
	push	r17
	push	XL
	push	XH
	push	ZL
	push	ZH

	; Note: these start off-screen to the right
	ldi	ZH:ZL, D_starfield_init
	ldi	XH:XL, V_sfld_l1
	ldi	r16, (16*3*3)		; this many in total (16x3 in 3 layers)
.L0:
	lpm	r17, Z+			; copy out of program memory into SRAM
	st	X+, r17
	dec	r16
	brne	0b

	ldi	XH:XL, V_sfld_pos
	clr	r16
	st	X, r16

	pop	ZH
	pop	ZL
	pop	XH
	pop	XL
	pop	r17
	pop	r16
	ret
;}}}
demo_starfield_advance: ;{{{  advances starfield, uses V_sfld_pos to know what to move
	push	r16
	push	r17
	push	r18
	push	XL
	push	XH

	ldi	XH:XL, V_sfld_pos
	ld	r18, X			; r18 = V_sfld_pos

	ldi	XH:XL, V_sfld_l1
	; always move top frame points
	ldi	r16, 16
.L0:
	ld	r17, X
	cpi	r17, 2			; don't draw anything left of this
	brne	1f
	; fell of, move back
	ldi	r17, 125
	st	X+, r17
	lds	r17, TCNT1L
	andi	r17, 0x7f		; restrict to 0->127
	sbrc	r17, 6			; if bit 6 clear, skip next
	andi	r17, 0x5f		; if bit 6 set, clear bit 5
	cpi	r17, 94
	brlo	102f			; branch if new Y < 94
	subi	r17, 4			; else take 4 off
.L102:
	st	X+, r17			; new Y position
	adiw	XH:XL, 1		; advance by 1 (type)
	rjmp	11f
.L1:
	dec	r17
	st	X+, r17
	adiw	XH:XL, 2		; advance by 2 (Y and type)
.L11:
	dec	r16
	brne	0b

	; only move middle layer every other frame
	sbrs	r18, 0
	rjmp	2f
	; assert: odd numbered frame, so move middle layer
	ldi	XH:XL, V_sfld_l2

	ldi	r16, 16
.L0:
	ld	r17, X
	cpi	r17, 2			; don't draw anything left of this
	brne	1f
	; fell of, move back
	ldi	r17, 125
	st	X+, r17
	lds	r17, TCNT1L
	andi	r17, 0x7f		; restrict to 0->127
	sbrc	r17, 6			; if bit 6 clear, skip next
	andi	r17, 0x5f		; if bit 6 set, clear bit 5
	cpi	r17, 94
	brlo	102f			; branch if new Y < 94
	subi	r17, 4			; else take 4 off
.L102:
	st	X+, r17			; new Y position
	adiw	XH:XL, 1		; advance by 1 (type)
	rjmp	11f
.L1:
	dec	r17
	st	X+, r17
	adiw	XH:XL, 2		; advance by 2 (Y and type)
.L11:
	dec	r16
	brne	0b
.L2:

	; only move bottom layer every 4th frame
	mov	r16, r18
	andi	r16, 0x03
	cpi	r16, 0x03
	brne	2f
	; assert: 4th frame

	ldi	XH:XL, V_sfld_l3

	ldi	r16, 16
.L0:
	ld	r17, X
	cpi	r17, 2			; don't draw anything left of this
	brne	1f
	; fell of, move back
	ldi	r17, 126
.L1:
	dec	r17
	st	X+, r17
	adiw	XH:XL, 2		; advance by 2 (Y and type)
	dec	r16
	brne	0b

.L2:
	ldi	XH:XL, V_sfld_pos
	inc	r18			; next position please (will wrap harmlessly)
	st	X, r18

	pop	XH
	pop	XL
	pop	r18
	pop	r17
	pop	r16
	ret
;}}}
demo_starfield_draw: ;{{{  draws the starfield
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

	ldi	ZH:ZL, V_sfld_l1
	ldi	r19, (16*3)		; how many thing we might be drawing

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	r20, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	r20, hi(V_framebuffer2)	;

.L0:
	ld	r16, Z+			; load X
	ld	r17, Z+			; load Y

	cpi	r16, 126
	brlo	2f
	; X >= 126, so off the right-hand edge somewhere.
	ld	r18, Z+			; load type (to discard)
	rjmp	5f			; next star please
.L2:

	;{{{  sort out correct position in the framebuffer
	mov	XH, r20			; start of framebuffer
	mov	XL, r17
	swap	XL			; low-order bits of Y * 16
	mov	r18, XL
	andi	r18, 0x0f		; save high-order bits of Ypos
	andi	XL, 0xf0		; cull in XL
	add	XH, r18			; add these in

	; assert: XH:XL points at the start of the relevant framebuffer line
	mov	r18, r16
	lsr	r18
	lsr	r18
	lsr	r18
	or	XL, r18			; XH:XL now points at correct starting byte (line + (X/8))
	;}}}

	ld	r18, Z+			; load type
	cpi	r18, 0			; single point?
	brne	2f
	rjmp	3f
.L2:
	cpi	r18, 1			; small burst?
	brne	2f
	rjmp	4f
.L2:
	; else must be large burst
	;{{{  large burst, XH:XL on top-left pixel's byte
	mov	r17, r16
	andi	r17, 0x07
	cpi	r17, 6			; spills over by 1 pixel?
	breq	22f
	cpi	r17, 7			; spills over by 2 pixels?
	breq	23f
	; else fits cleanly in the particular byte (at most shift 5).
	;{{{  put pattern in variable position (0-5)
	ldi	r18, 0xa0
	ldi	r21, 0x40

	; shift pattern right appropriately
	sbrs	r16, 0
	rjmp	21f
	lsr	r18
	lsr	r21
.L21:
	sbrs	r16, 1
	rjmp	21f
	lsr	r18
	lsr	r18
	lsr	r21
	lsr	r21
.L21:
	sbrs	r16, 2
	rjmp	21f
	swap	r18
	swap	r21
.L21:

	; could have only shifted by at most 3.
	; paint :)  [pattern in r18, r21]
	ld	r17, X
	or	r17, r18
	st	X, r17
	adiw	XH:XL, HRES		; next line
	ld	r17, X
	or	r17, r21
	st	X, r17
	adiw	XH:XL, HRES		; next line
	ld	r17, X
	or	r17, r18
	st	X, r17

	;}}}
	rjmp	29f
.L22:
	;{{{  spills over by 1 pixel, but known position
	ld	r17, X
	ori	r17, 0x02
	st	X+, r17
	ld	r17, X
	ori	r17, 0x80
	st	X, r17
	adiw	XH:XL, HRES-1		; next line please
	ld	r17, X
	ori	r17, 0x01
	st	X, r17
	adiw	XH:XL, HRES		; next line please
	ld	r17, X
	ori	r17, 0x02
	st	X+, r17
	ld	r17, X
	ori	r17, 0x80
	st	X, r17
	;}}}
	rjmp	29f
.L23:
	;{{{  spills over by 2 pixels, but known position
	ld	r17, X
	ori	r17, 0x01
	st	X+, r17

	ld	r17, X
	ori	r17, 0x40
	st	X, r17

	adiw	XH:XL, HRES		; next line please (+1)
	ld	r17, X
	ori	r17, 0x80
	st	X, r17

	adiw	XH:XL, HRES-1		; next line
	ld	r17, X
	ori	r17, 0x01
	st	X+, r17

	ld	r17, X
	ori	r17, 0x40
	st	X, r17
	;}}}
	rjmp	29f

	;}}}
.L29:
	rjmp	5f
.L3:
	;{{{  single point, XH:XL on correct byte already
	ldi	r18, 0x80		; default bit to set (0)
	sbrc	r16, 0			; shift r18 right appropriately
	lsr	r18
	sbrc	r16, 1
	lsr	r18
	sbrc	r16, 1
	lsr	r18
	sbrc	r16, 2
	swap	r18			; swap rather than shift by 4

	ld	r17, X
	or	r17, r18		; put in point
	st	X, r17

	;}}}
	rjmp	5f
.L4:
	;{{{  small burst, XH:XL on top-left pixel's byte
	mov	r17, r16
	andi	r17, 0x07		; trim out last 3 bits
	cpi	r17, 6			; spills over by 1 pixel?
	breq	42f
	cpi	r17, 7			; spills over by 2 pixels?
	breq	43f
	; else fits cleanly in the particular byte
	;{{{  put pattern in variable position (0-5)
	ldi	r18, 0x40
	ldi	r21, 0xe0

	sbrs	r16, 0			; shift pattern right appropriately
	rjmp	41f
	lsr	r18
	lsr	r21
.L41:
	sbrs	r16, 1
	rjmp	41f
	lsr	r18
	lsr	r18
	lsr	r21
	lsr	r21
.L41:
	sbrs	r16, 2
	rjmp	41f
	swap	r18
	swap	r21
.L41:

	; then paint :)  [pattern in r18, r21]
	ld	r17, X
	or	r17, r18
	st	X, r17
	adiw	XH:XL, HRES		; next line
	ld	r17, X
	or	r17, r21
	st	X, r17
	adiw	XH:XL, HRES		; next line
	ld	r17, X
	or	r17, r18
	st	X, r17

	;}}}
	rjmp	49f
.L42:
	;{{{  spills over by 1 pixel, but known position at least :)
	ld	r17, X
	ori	r17, 0x01
	st	X, r17
	adiw	XH:XL, HRES		; next line
	ld	r17, X
	ori	r17, 0x03
	st	X+, r17
	ld	r17, X
	ori	r17, 0x80
	st	X, r17
	adiw	XH:XL, HRES-1		; next line
	ld	r17, X
	ori	r17, 0x01
	st	X, r17

	;}}}
	rjmp	49f
.L43:
	;{{{  spills over by 2 pixels, but known position
	ld	r17, X+			; and discard
	ld	r17, X
	ori	r17, 0x80
	st	X, r17
	adiw	XH:XL, HRES-1		; next line
	ld	r17, X
	ori	r17, 0x01
	st	X+, r17
	ld	r17, X
	ori	r17, 0xc0
	st	X, r17
	adiw	XH:XL, HRES		; next line, +1
	ld	r17, X
	ori	r17, 0x80
	st	X, r17
	;}}}

.L49:
	;}}}
	rjmp	5f

.L5:
	dec	r19
	breq	1f
	rjmp	0b
.L1:

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
	ret
;}}}
demo_sctext_init: ;{{{  preps for scrolly text, flash address given in r17:r16, Y position in r18, start mode in r19
	push	r18

	sts	V_sctext + 0, r17		; store string address in FLASH
	sts	V_sctext + 1, r16
	sts	V_sctext + 5, r18		; store Y position
	clr	r18
	sts	V_sctext + 2, r18		; offset high-byte = 0
	cpi	r19, 0
	brne	0f
	; assert: mode = scroll-in-R, so use offset LSB for X offset
	ldi	r18, 126
.L0:
	sts	V_sctext + 3, r18
	sts	V_sctext + 4, r19

	pop	r18
	ret
;}}}
demo_sctext_renderstep: ;{{{  renders scrolly text and advances depending on mode
	push	r16
	push	r17
	push	r19
	push	ZL
	push	ZH

	lds	ZH, V_sctext + 0		; load FLASH address (regardless)
	lds	ZL, V_sctext + 1

	lds	r19, V_sctext + 4		; load type
	cpi	r19, 1
	brlo	0f				; branch if scroll-in-R
	brne	2f				; branch if not hold (so must be scroll-out-L)
	rjmp	1f				; branch if hold
.L2:						; else scroll-out-L
	lds	r17, V_sctext + 2		; load text offset into r17:r16 -> upper 13-bits are message offset, lower 3 bits are character-offset
	lds	r16, V_sctext + 3
	clr	r19
	clc
	ror	r17
	ror	r16
	ror	r19				; shift r17:r16 right, into r19
	ror	r17
	ror	r16
	ror	r19				; shift r17:r16 right, into r19
	ror	r17
	ror	r16
	ror	r19				; shift r17:r16 right, into r19

	; r17:r16 now has the 13-bit message offset, r19 has 3 MSBs with the char offset
	lsr	r19
	swap	r19				; r19 = 0-5

	add	ZL, r16
	adc	ZH, r17				; adjust for message offset

	ldi	r16, 5
	sub	r16, r19			; r16 = 5-0, X position (relies on the fact we don't render some of the left-most stuff)
	lds	r17, V_sctext + 5		; Y position

	call	fb_writestring

	cpi	r16, 1				; was the X position one? (means end of this character)
	breq	2f
	; moving within a character, add 2 to low-order bits
	lds	ZH, V_sctext + 2
	lds	ZL, V_sctext + 3
	adiw	ZH:ZL, 2
	sts	V_sctext + 2, ZH
	sts	V_sctext + 3, ZL
	rjmp	3f

.L2:
	; end of this character, onto next.  Z still pointing at the first character to display, if the null-character, don't update
	lpm	r19, Z
	tst	r19
	breq	3f				; abandon if null-terminator

	lds	ZH, V_sctext + 2
	lds	ZL, V_sctext + 3
	adiw	ZH:ZL, 8
	andi	ZL, 0xf8			; clear out low-order bits
	sts	V_sctext + 2, ZH
	sts	V_sctext + 3, ZL

.L3:
	rjmp	9f
.L0:
	; scroll in from right, offset LSB has X position to draw at.
	lds	r16, V_sctext + 3		; offset LSB (= X pos)
	lds	r17, V_sctext + 5		; Y position
	call	fb_writestring
	; remove 2 from X position
	subi	r16, 2
	sts	V_sctext + 3, r16
	brne	2f
	; reached zero, change mode to 'hold'
	ldi	r19, 1
	sts	V_sctext + 4, r19		; set mode = 1
.L2:
	rjmp	9f
.L1:
	; holding, so just draw and leave alone
	lds	r16, V_sctext + 3		; offset LSB (= X pos)
	lds	r17, V_sctext + 5		; Y position
	call	fb_writestring
	rjmp	9f

.L9:
	pop	ZH
	pop	ZL
	pop	r19
	pop	r17
	pop	r16
	ret

;}}}
demo_svtext_init: ;{{{  preps for scrolly text, flash address given in r17:r16, X position in r18, Y position in r19, start mode in r20 (==0, target in r21)
	sts	V_svtext + 0, r17		; store string address in FLASH
	sts	V_svtext + 1, r16
	sts	V_svtext + 2, r19		; initial Y position (negative values accepted to get slight lines)
	sts	V_svtext + 3, r20		; start mode
	sts	V_svtext + 4, r18		; X position
	cpi	r20, 0
	brne	0f
	sts	V_svtext + 5, r21		; target Y for scroll-in-top
	rjmp	1f
.L0:
	push	r21
	clr	r21
	sts	V_svtext + 5, r21
	pop	r21
.L1:

	ret
;}}}
demo_svtext_renderstep: ;{{{  renders vertical scrolly text and advances depending on mode
	push	r16
	push	r17
	push	r19
	push	ZL
	push	ZH

	lds	ZH, V_svtext + 0		; load FLASH address (regardless)
	lds	ZL, V_svtext + 1

	; draw this at the particular X/Y coords regardless
	lds	r16, V_svtext + 4		; X position
	lds	r17, V_svtext + 2		; Y position
	call	fb_writestring

	lds	r19, V_svtext + 3		; load mode/type
	cpi	r19, 1
	brlo	0f				; branch if scroll-in-top
	brne	1f				; branch if not hold (so must be scroll-out-bottom)
	; else holding
	rjmp	9f

.L0:
	; scrolling in from the top, Y may be negative
	inc	r17				; next line down
	sts	V_svtext + 2, r17
	lds	r19, V_svtext + 5		; load target Y
	cp	r17, r19			; here yet?
	brne	2f
	; yes, so set mode to hold
	ldi	r19, 1
	sts	V_svtext + 3, r19
.L2:
	rjmp	9f

.L1:
	; scrolling out at the bottom
	inc	r17
	sts	V_svtext + 2, r17
	cpi	r17, 96				; can't see it anymore?
	brne	9f
	ldi	r19, 1
	sts	V_svtext + 3, r19		; set mode = 1 (holding)

.L9:
	pop	ZH
	pop	ZL
	pop	r19
	pop	r17
	pop	r16
	ret
;}}}
demo_svrtext_init: ;{{{  preps for scrolly text, flash address given in r17:r16, X position in r18, Y position in r19, start mode in r20 (==0, target in r21)
	sts	V_svrtext + 0, r17		; store string address in FLASH
	sts	V_svrtext + 1, r16
	sts	V_svrtext + 2, r19		; initial Y position (negative values accepted to get slight lines)
	sts	V_svrtext + 3, r20		; start mode
	sts	V_svrtext + 4, r18		; X position
	cpi	r20, 0
	brne	0f
	sts	V_svrtext + 5, r21		; target Y for scroll-in-bottom
	rjmp	1f
.L0:
	push	r21
	clr	r21
	sts	V_svrtext + 5, r21
	pop	r21
.L1:

	ret
;}}}
demo_svrtext_renderstep: ;{{{  renders vertical scrolly text and advances depending on mode (this one scrolls bottom-up)
	push	r16
	push	r17
	push	r19
	push	ZL
	push	ZH

	lds	ZH, V_svrtext + 0		; load FLASH address (regardless)
	lds	ZL, V_svrtext + 1

	; draw this at the particular X/Y coords regardless
	lds	r16, V_svrtext + 4		; X position
	lds	r17, V_svrtext + 2		; Y position
	call	fb_writestring

	lds	r19, V_svrtext + 3		; load mode/type
	cpi	r19, 1
	brlo	0f				; branch if scroll-in-bottom
	brne	1f				; branch if not hold (so must be scroll-out-top)
	; else holding
	rjmp	9f
.L0:
	; scrolling in from the bottom
	dec	r17				; next line up
	sts	V_svrtext + 2, r17
	lds	r19, V_svrtext + 5		; load target Y
	cp	r17, r19			; here yet?
	brne	2f
	; yes, so set mode to hold
	ldi	r19, 1
	sts	V_svrtext + 3, r19
.L2:
	rjmp	9f

.L1:
	; scrolling out via the top, will deliberately wrap Y
	dec	r17
	sts	V_svrtext + 2, r17
	cpi	r17, -7				; gone completely?
	brne	9f
	ldi	r19, 1
	sts	V_svrtext + 3, r19		; set mode = 1 (holding)

.L9:
	pop	ZH
	pop	ZL
	pop	r19
	pop	r17
	pop	r16
	ret
;}}}
demo_pac_render: ;{{{  renders a pacman on the display at r16,r17  (right facing).  Only use when it is known all-on-screen.
	push	r16
	push	r19
	push	r20
	push	XL
	push	XH
	push	ZL
	push	ZH

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	mov	XL, r17
	swap	XL			; low-order bits * 16
	mov	r19, XL
	andi	r19, 0x0f		; save high-order bits of Ypos
	andi	XL, 0xf0		; cull these in XL
	add	XH, r19			; add these in

	; assert: XH:XL points at the start of the relevant line
	clr	r19
	clc
	ror	r16			; divide r16/8 and shift 3 LSB into r19 3 MSB
	ror	r19
	ror	r16
	ror	r19
	ror	r16
	ror	r19

	lsr	r19
	swap	r19			; swap around so r19 = 0-7 (starting bit offset in X)
	or	XL, r16			; appropriate byte

	; X now points at the start in the framebuffer, can re-use r16, r19, r20

	; NOTE: TODO: check Y and X overruns in the future
	cpi	r19, 5
	brsh	1f			; 3 bytes per line
	ldi	ZH:ZL, D_pac_glyph2
	lsl	r19
	swap	r19			; multiply by 32, (0-4 -> 0-128)
	clr	r16
	add	ZL, r19
	adc	ZH, r16			; Z points at the appropriately X-offset'd data (2 bytes per line)

	ldi	r19, 13			; this many lines
.L0:
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X+, r16
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X, r16
	adiw	XH:XL, HRES-1		; next line in framebuffer
	dec	r19
	brne	0b
	rjmp	9f			; done, so get out

.L1:
	; similar to the above, but 3 bytes per line.  r19 is 5-7
	subi	r19, 5			; r19 = 0,1,2
	ldi	ZH:ZL, D_pac_glyph3
	mov	r20, r19
	lsl	r20			; r20 = 0,2,4
	add	r20, r19		; r20 = 0,3,6
	swap	r20			; r20 = 0,48,96
	clr	r16
	add	ZL, r20
	adc	ZH, r16			; Z points at the appropriately X-offset'd data (3 bytes per line)

	ldi	r19, 13			; this many lines
.L0:
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X+, r16
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X+, r16
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X, r16
	adiw	XH:XL, HRES-2		; next line in framebuffer
	dec	r19
	brne	0b
	; else done :)

.L9:

	pop	ZH
	pop	ZL
	pop	XH
	pop	XL
	pop	r20
	pop	r19
	pop	r16
	ret


;}}}
demo_ghost_render: ;{{{  renders a ghost on the display at r16,r17  (left facing).  Only use when it is known all-on-screen.
	push	r16
	push	r19
	push	r20
	push	XL
	push	XH
	push	ZL
	push	ZH

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	mov	XL, r17
	swap	XL			; low-order bits * 16
	mov	r19, XL
	andi	r19, 0x0f		; save high-order bits of Ypos
	andi	XL, 0xf0		; cull these in XL
	add	XH, r19			; add these in

	; assert: XH:XL points at the start of the relevant line
	clr	r19
	clc
	ror	r16			; divide r16/8 and shift 3 LSB into r19 3 MSB
	ror	r19
	ror	r16
	ror	r19
	ror	r16
	ror	r19

	lsr	r19
	swap	r19			; swap around so r19 = 0-7 (starting bit offset in X)
	or	XL, r16			; appropriate byte

	; X now points at the start in the framebuffer, can re-use r16, r19, r20

	; NOTE: TODO: check Y and X overruns in the future
	cpi	r19, 3
	brsh	1f			; 3 bytes per line
	ldi	ZH:ZL, D_ghost_glyph2
	lsl	r19
	swap	r19			; multiply by 32 (0-2 -> 0-64)
	clr	r16
	add	ZL, r19
	adc	ZH, r16			; Z points at the appropriate X-offset'd data (2 bytes per line)

	ldi	r19, 14			; this many lines
.L0:
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X+, r16
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X, r16
	adiw	XH:XL, HRES-1		; next line in framebuffer
	dec	r19
	brne	0b
	rjmp	9f			; done, so get out

.L1:
	; similar to the above, but 3 bytes per line.  r19 is 3-7
	subi	r19, 3			; r19 = 0,1,2,3,4
	ldi	ZH:ZL, D_ghost_glyph3
	mov	r20, r19
	lsl	r20			; r20 = 0,2,4,6,8
	add	r20, r19		; r20 = 0,3,6,9,12
	swap	r20			; r20 = 0,48,96,144,192
	clr	r16
	add	ZL, r20
	adc	ZH, r16			; Z points at the appropriately X-offset'd data (3 bytes per line)

	ldi	r19, 14			; this many lines
.L0:
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X+, r16
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X+, r16
	ld	r16, X
	lpm	r20, Z+
	or	r16, r20
	st	X, r16
	adiw	XH:XL, HRES-2		; next line in framebuffer
	dec	r19
	brne	0b
	; else done :)

.L9:


	pop	ZH
	pop	ZL
	pop	XH
	pop	XL
	pop	r20
	pop	r19
	pop	r16
	ret
;}}}
demo_disc: ;{{{  fills a circle at r16,r17 radius r18
	nop
	nop
	ret
;}}}
demo_circle: ;{{{  draws a circle at r16,r17 radius r18
	push	r15
	push	r19
	push	r20
	push	r21
	push	r22
	push	r23

	ldi	r20, 0			; r20 = x = 0
	mov	r21, r18		; r21 = y = R
	ldi	r19, 1
	sub	r19, r18		; r19 = d = 1 - R

	rcall	demo_circle_pnts
.L0:
	cp	r20, r21		; while (x < y)
	brsh	5f			;

	cpi	r19, 1
	brge	1f			; if (d > 0)
	; else d <= 0
	mov	r22, r20
	lsl	r22
	ldi	r23, 3
	add	r23, r22		; r23 = 2*x + 3
	add	r19, r23		; d += (2*x + 3)
	rjmp	2f
.L1:
	; d > 0
	mov	r22, r20
	sub	r22, r21
	lsl	r22
	ldi	r23, 5
	add	r23, r22		; r23 = 5 + 2*(x-y), signed
	add	r19, r23		; d += (2*(x-y) + 5)
	dec	r21			; y--
.L2:
	inc	r20			; x++
	rcall	demo_circle_pnts
	rjmp	0b
.L5:

	pop	r23
	pop	r22
	pop	r21
	pop	r20
	pop	r19
	pop	r15
	ret

demo_circle_pnts:			; helper that plots the 8 points, (r16,r17) is the centre, (r20,r21) the x and y values, r22,r23 may be trashed
	push	r16
	push	r17

	mov	r22, r16
	mov	r23, r17

	add	r17, r21
	add	r16, r20
	call	fb_xxsetpixel		; (X + x, Y + y)
	mov	r16, r22
	sub	r16, r20
	call	fb_xxsetpixel		; (X - x, Y + y)
	mov	r17, r23
	sub	r17, r21
	call	fb_xxsetpixel		; (X - x, Y - y)
	mov	r16, r22
	add	r16, r20
	call	fb_xxsetpixel		; (X + x, Y - y)

	mov	r16, r22		; put right
	mov	r17, r23

	add	r17, r20
	add	r16, r21
	call	fb_xxsetpixel		; (X + y, Y + x)
	mov	r16, r22
	sub	r16, r21
	call	fb_xxsetpixel		; (X - y, Y + x)
	mov	r17, r23
	sub	r17, r20
	call	fb_xxsetpixel		; (X - y, Y - x)
	mov	r16, r22
	add	r16, r21
	call	fb_xxsetpixel		; (X + y, Y - x)

	pop	r17
	pop	r16
	ret

;}}}
demo_xorcirc_plain: ;{{{  fills the framebuffer with raw I_xorcirc_image data from (r16,r17)
	push	r0
	push	r1
	push	r17
	push	r18
	push	r19
	push	r20
	push	XL
	push	XH
	push	ZL
	push	ZH

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	clr	XL

	ldi	ZH:ZL, I_xorcirc_image
	ldi	r18, 32
	mul	r17, r18		; offset for particular Y into r1:r0
	add	ZL, r0
	adc	ZH, r1

	mov	r0, r16
	lsr	r0
	lsr	r0
	lsr	r0			; r0 = byte offset (X/8)
	clr	r1
	add	ZL, r0
	adc	ZH, r1			; advance for X byte offset (3 LSB still in r16)

	mov	r18, r16
	andi	r18, 0x07		; mask bit offset

	; decision tree based on r18 value.. bit grotty
	ldi	r20, 0x01
	sbrc	r18, 0
	lsl	r20
	sbrc	r18, 1
	lsl	r20
	sbrc	r18, 1
	lsl	r20
	sbrc	r18, 2
	swap	r20
	; r20 will be bit-set appropriately

	sbrc	r20, 0
	rjmp	10f			; zero bit offset (simple)
	sbrc	r20, 1
	rjmp	11f			; 1 bit offset
	sbrc	r20, 2
	rjmp	12f			; 2 bit offset
	sbrc	r20, 3
	rjmp	13f			; 3 bit offset
	sbrc	r20, 4
	rjmp	14f			; 4 bit offset
	sbrc	r20, 5
	rjmp	15f			; 5 bit offset
	sbrc	r20, 6
	rjmp	16f			; 6 bit offset

	;{{{  else 7 bit offset
	ldi	r19, 96			; how many lines
.L162:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L163:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round

	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB
	clc
	rol	r0			; and again
	rol	r1
	clc
	rol	r0			; and again
	rol	r1

	ldi	r17, 0xf0
	and	r0, r17			; save high-order bits
	swap	r0			; move over to low-order
	swap	r1
	and	r1, r17			; save high-order bits (previous low-order)
	or	r1, r0			; blend

	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	163b			; round again for next byte on line
	dec	r19
	breq	164f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	162b			; round again for next line
.L164:
	rjmp	9f
	
	;}}}
	rjmp	9f

.L10:
	;{{{  0 bit offset (simplest case)
	ldi	r19, 96			; how many lines
.L102:
	ldi	r18, 16			; width in bytes
.L103:
	lpm	r0, Z+			; load image piece
	st	X+, r0			; store in framebuffer
	dec	r18
	brne	103b			; round again for next byte on line
	dec	r19
	breq	104f			; done here
	; advance Z to next line in source image (+16 bytes from where we are)
	adiw	ZH:ZL, 16
	rjmp	102b			; round again for next line
.L104:
	rjmp	9f
	;}}}
.L11:
	;{{{  1 bit offset
	ldi	r19, 96			; how many lines
.L112:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L113:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round

	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB

	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	113b			; round again for next byte on line
	dec	r19
	breq	114f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	112b			; round again for next line
.L114:
	rjmp	9f
	
	;}}}
.L12:
	;{{{  2 bit offset
	ldi	r19, 96			; how many lines
.L122:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L123:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round
	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB
	clc
	rol	r0			; and again
	rol	r1

	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	123b			; round again for next byte on line
	dec	r19
	breq	124f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	122b			; round again for next line
.L124:
	rjmp	9f
	
	;}}}
.L13:
	;{{{  3 bit offset
	ldi	r19, 96			; how many lines
.L132:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L133:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round
	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB
	clc
	rol	r0			; and again
	rol	r1
	clc
	rol	r0			; and again
	rol	r1

	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	133b			; round again for next byte on line
	dec	r19
	breq	134f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	132b			; round again for next line
.L134:
	rjmp	9f
	
	;}}}
.L14:
	;{{{  4 bit offset
	ldi	r19, 96			; how many lines
.L142:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L143:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round
	ldi	r17, 0xf0
	and	r0, r17			; save high-order bits
	swap	r0			; move over to low-order
	swap	r1
	and	r1, r17			; save high-order bits (previous low-order)
	or	r1, r0			; blend

	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	143b			; round again for next byte on line
	dec	r19
	breq	144f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	142b			; round again for next line
.L144:
	rjmp	9f
	
	;}}}
.L15:
	;{{{  5 bit offset
	ldi	r19, 96			; how many lines
.L152:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L153:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round

	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB

	ldi	r17, 0xf0
	and	r0, r17			; save high-order bits
	swap	r0			; move over to low-order
	swap	r1
	and	r1, r17			; save high-order bits (previous low-order)
	or	r1, r0			; blend

	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	153b			; round again for next byte on line
	dec	r19
	breq	154f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	152b			; round again for next line
.L154:
	rjmp	9f
	
	;}}}
.L16:
	;{{{  6 bit offset
	ldi	r19, 96			; how many lines
.L162:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L163:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round

	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB
	clc
	rol	r0			; and again
	rol	r1

	ldi	r17, 0xf0
	and	r0, r17			; save high-order bits
	swap	r0			; move over to low-order
	swap	r1
	and	r1, r17			; save high-order bits (previous low-order)
	or	r1, r0			; blend

	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	163b			; round again for next byte on line
	dec	r19
	breq	164f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	162b			; round again for next line
.L164:
	rjmp	9f
	
	;}}}

.L9:
	pop	ZH
	pop	ZL
	pop	XH
	pop	XL
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r1
	pop	r0
	ret
;}}}
demo_xorcirc_xor: ;{{{  xor's the framebuffer with raw I_xorcirc_image data from (r16,r17)
	push	r0
	push	r1
	push	r17
	push	r18
	push	r19
	push	r20
	push	XL
	push	XH
	push	ZL
	push	ZH

	sbrc	LINE_REGION, 7		; select framebuffer we're not rendering
	ldi	XH, hi(V_framebuffer1)	;
	sbrs	LINE_REGION, 7		;
	ldi	XH, hi(V_framebuffer2)	;

	clr	XL

	ldi	ZH:ZL, I_xorcirc_image
	ldi	r18, 32
	mul	r17, r18		; offset for particular Y into r1:r0
	add	ZL, r0
	adc	ZH, r1

	mov	r0, r16
	lsr	r0
	lsr	r0
	lsr	r0			; r0 = byte offset (X/8)
	clr	r1
	add	ZL, r0
	adc	ZH, r1			; advance for X byte offset (3 LSB still in r16)

	mov	r18, r16
	andi	r18, 0x07		; mask bit offset

	; decision tree based on r18 value.. bit grotty
	ldi	r20, 0x01
	sbrc	r18, 0
	lsl	r20
	sbrc	r18, 1
	lsl	r20
	sbrc	r18, 1
	lsl	r20
	sbrc	r18, 2
	swap	r20
	; r20 will be bit-set appropriately

	sbrc	r20, 0
	rjmp	10f			; zero bit offset (simple)
	sbrc	r20, 1
	rjmp	11f			; 1 bit offset
	sbrc	r20, 2
	rjmp	12f			; 2 bit offset
	sbrc	r20, 3
	rjmp	13f			; 3 bit offset
	sbrc	r20, 4
	rjmp	14f			; 4 bit offset
	sbrc	r20, 5
	rjmp	15f			; 5 bit offset
	sbrc	r20, 6
	rjmp	16f			; 6 bit offset

	;{{{  else 7 bit offset
	ldi	r19, 96			; how many lines
.L162:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L163:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round

	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB
	clc
	rol	r0			; and again
	rol	r1
	clc
	rol	r0			; and again
	rol	r1

	ldi	r17, 0xf0
	and	r0, r17			; save high-order bits
	swap	r0			; move over to low-order
	swap	r1
	and	r1, r17			; save high-order bits (previous low-order)
	or	r1, r0			; blend

	ld	r0, X			; load framebuffer piece
	eor	r1, r0			; blend
	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	163b			; round again for next byte on line
	dec	r19
	breq	164f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	162b			; round again for next line
.L164:
	rjmp	9f
	
	;}}}
	rjmp	9f

.L10:
	;{{{  0 bit offset (simplest case)
	ldi	r19, 96			; how many lines
.L102:
	ldi	r18, 16			; width in bytes
.L103:
	lpm	r0, Z+			; load image piece
	ld	r1, X			; load framebuffer piece
	eor	r0, r1			; blend
	st	X+, r0			; store in framebuffer
	dec	r18
	brne	103b			; round again for next byte on line
	dec	r19
	breq	104f			; done here
	; advance Z to next line in source image (+16 bytes from where we are)
	adiw	ZH:ZL, 16
	rjmp	102b			; round again for next line
.L104:
	rjmp	9f
	;}}}
.L11:
	;{{{  1 bit offset
	ldi	r19, 96			; how many lines
.L112:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L113:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round

	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB

	ld	r0, X			; load framebuffer piece
	eor	r1, r0			; blend
	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	113b			; round again for next byte on line
	dec	r19
	breq	114f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	112b			; round again for next line
.L114:
	rjmp	9f
	
	;}}}
.L12:
	;{{{  2 bit offset
	ldi	r19, 96			; how many lines
.L122:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L123:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round
	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB
	clc
	rol	r0			; and again
	rol	r1

	ld	r0, X			; load framebuffer piece
	eor	r1, r0			; blend
	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	123b			; round again for next byte on line
	dec	r19
	breq	124f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	122b			; round again for next line
.L124:
	rjmp	9f
	
	;}}}
.L13:
	;{{{  3 bit offset
	ldi	r19, 96			; how many lines
.L132:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L133:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round
	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB
	clc
	rol	r0			; and again
	rol	r1
	clc
	rol	r0			; and again
	rol	r1

	ld	r0, X			; load framebuffer piece
	eor	r1, r0			; blend
	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	133b			; round again for next byte on line
	dec	r19
	breq	134f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	132b			; round again for next line
.L134:
	rjmp	9f
	
	;}}}
.L14:
	;{{{  4 bit offset
	ldi	r19, 96			; how many lines
.L142:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L143:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round
	ldi	r17, 0xf0
	and	r0, r17			; save high-order bits
	swap	r0			; move over to low-order
	swap	r1
	and	r1, r17			; save high-order bits (previous low-order)
	or	r1, r0			; blend

	ld	r0, X			; load framebuffer piece
	eor	r1, r0			; blend
	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	143b			; round again for next byte on line
	dec	r19
	breq	144f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	142b			; round again for next line
.L144:
	rjmp	9f
	
	;}}}
.L15:
	;{{{  5 bit offset
	ldi	r19, 96			; how many lines
.L152:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L153:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round

	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB

	ldi	r17, 0xf0
	and	r0, r17			; save high-order bits
	swap	r0			; move over to low-order
	swap	r1
	and	r1, r17			; save high-order bits (previous low-order)
	or	r1, r0			; blend

	ld	r0, X			; load framebuffer piece
	eor	r1, r0			; blend
	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	153b			; round again for next byte on line
	dec	r19
	breq	154f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	152b			; round again for next line
.L154:
	rjmp	9f
	
	;}}}
.L16:
	;{{{  6 bit offset
	ldi	r19, 96			; how many lines
.L162:
	ldi	r18, 16			; width in bytes
	lpm	r1, Z+			; load first bit of image data
.L163:
	lpm	r0, Z+			; load next bit of image data
	mov	r20, r0			; save for next round

	clc
	rol	r0			; high bit of r0 into carry
	rol	r1			; and into r1 LSB
	clc
	rol	r0			; and again
	rol	r1

	ldi	r17, 0xf0
	and	r0, r17			; save high-order bits
	swap	r0			; move over to low-order
	swap	r1
	and	r1, r17			; save high-order bits (previous low-order)
	or	r1, r0			; blend

	ld	r0, X			; load framebuffer piece
	eor	r1, r0			; blend
	st	X+, r1			; store in framebuffer
	mov	r1, r20			; what we read out last time
	dec	r18
	brne	163b			; round again for next byte on line
	dec	r19
	breq	164f			; done here
	; advance Z to next line in source image (+15 bytes from where we are)
	adiw	ZH:ZL, 15
	rjmp	162b			; round again for next line
.L164:
	rjmp	9f
	
	;}}}

.L9:
	pop	ZH
	pop	ZL
	pop	XH
	pop	XL
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r1
	pop	r0
	ret
;}}}

lvar_init: ;{{{  initialises local stuffs
	push	r16
	push	r17
	push	r18
	push	XL
	push	XH

	ldi	r17:r16, 0
	sts	V_scroll1_h, r17
	sts	V_scroll1_l, r16

	sts	V_sndpos_h, r17
	sts	V_sndpos_l, r16

	;{{{  setup V_rdrpnts -- just copy from static stuff for now
	push	ZH
	push	ZL

	ldi	XH:XL, V_rdrpnts
	ldi	ZH:ZL, D_rdrpnts_static
	ldi	r18, 32
.L0:
	lpm	r16, Z+
	st	X+, r16
	dec	r18
	brne	0b

	pop	ZL
	pop	ZH
	;}}}

	pop	XH
	pop	XL
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

	call	fb_clearall
	call	vidhw_setup
	call	lvar_init
	call	snd_hwsetup

	sei				; enable interrupts

	; at this point we're effectively going :)
prg_code:

	ldi	r25:r24, 0		; frame counter

	;{{{  first things to initialise: starfield and scrolly messages for first round
	call	demo_starfield_init
	ldi	r17:r16, S_sfmsg1
	ldi	r18, 48			; Y position
	ldi	r19, 0
	call	demo_sctext_init

	ldi	r17:r16, S_sfmsg2
	ldi	r18, 46			; X position
	ldi	r19, -6			; initial Y position
	ldi	r20, 0			; mode = scroll in top
	ldi	r21, 34			; target Y position
	call	demo_svtext_init


	;}}}
	;{{{  major frames 0-2  (starfield and scrollies)
.L0:
	call	demo_starfield_advance
	call	fb_clear
	call	demo_starfield_draw

	cpi	r25, 1
	brne	2f
	; major frame 1, scroll or hold until minor frame 210
	cpi	r24, 210
	brne	1f
	; assert: frame 1:210, start scrolling off after hold
	ldi	r16, 2
	sts	V_sctext + 4, r16
	sts	V_svtext + 3, r16
	ldi	r16, 8
	sts	V_sctext + 3, r16	; set X offset to 8 (next character)
	rjmp	4f
.L1:
	; assert: frame 1:(!210)
	cpi	r24, 25
	brlo	3f			; branch if < 1:25
	; here means we're in frame >= 1:25
.L4:
	call	demo_svtext_renderstep	; else add in vertical scrolling text
.L3:
	call	demo_sctext_renderstep	; do text render and step
	call	vid_waitflip
	adiw	r25:r24, 1
	rjmp	0b			; go again on next frame :)

.L2:	; here if not major frame 1
	cpi	r25, 2
	brne	2f
	; major frame 2, continue scrolling off (until minor frame 30)
	cpi	r24, 30
	brlo	1f
	cpi	r24, 60
	breq	3f			; if frame 2:60, jump out into frame 3:00
	; else minor frame >= 30, so we're done with scrolly text for now
	call	vid_waitflip
	adiw	r25:r24, 1
	rjmp	0b
.L1:
	; assert: frames 2:0 -> 2:29.
	call	demo_sctext_renderstep	; do text render and step
	call	demo_svtext_renderstep	; 
	call	vid_waitflip
	adiw	r25:r24, 1
	rjmp	0b			; go again on next frame :)
.L2:
	; major frame 0,3
	cpi	r25, 3
	breq	9f
	call	vid_waitflip
	adiw	r25:r24, 1
	rjmp	0b			; go round again
.L3:
	ldi	r25:r24, 0x0300
.L9:
	;}}}
	; when we arrive here, in frame 3:0
	;{{{  initialise next scrollies
	ldi	r17:r16, S_sfmsg3
	ldi	r18, 48			; Y position
	ldi	r19, 0
	call	demo_sctext_init

	ldi	r17:r16, S_sfmsg4
	ldi	r18, 20			; X position
	ldi	r19, -6			; initial Y
	ldi	r20, 0			; mode = scroll in top
	ldi	r21, 34			; target Y position
	call	demo_svtext_init

	ldi	r17:r16, S_sfmsg5
	ldi	r18, 16			; X position
	ldi	r19, 95			; initial Y
	ldi	r20, 0			; mode = scroll in bottom
	ldi	r21, 62			; target Y position
	call	demo_svrtext_init

	;}}}
	;{{{  major frames 3-4  (starfield and scrollies)
.L0:
	call	demo_starfield_advance
	call	fb_clear
	call	demo_starfield_draw

	cpi	r25, 4
	brlo	1f			; in major frame 3
	breq	2f			; in major frame 4
	; else we just went into frame 5
	rjmp	9f
.L1:
	; major frame 3, scroll or hold until minor frame 210
	cpi	r24, 210
	brne	1f
	; assert: frame 3:210, start scrolling off after hold
	ldi	r16, 2
	sts	V_sctext + 4, r16
	sts	V_svtext + 3, r16
	sts	V_svrtext + 3, r16
	ldi	r16, 8
	sts	V_sctext + 3, r16	; set X offset to 8 (next character)
.L1:
	; assert: in major frame 3, only do vertical scroll if minor >= 25
	cpi	r24, 25
	brlo	3f
	call	demo_svtext_renderstep
	call	demo_svrtext_renderstep
.L3:
	call	demo_sctext_renderstep
	call	vid_waitflip
	adiw	r25:r24, 1
	rjmp	0b			; next frame please
.L2:
	; major frame 4, scroll until minor frame 30
	cpi	r24, 30
	brlo	1f
	cpi	r24, 60
	breq	3f			; at frame 4:60, fast advance to 5:0
	; else minor frame >= 30, so no scrolly text for now
	call	vid_waitflip
	adiw	r25:r24, 1
	rjmp	0b
.L1:
	call	demo_svtext_renderstep
	call	demo_svrtext_renderstep
	call	demo_sctext_renderstep
	call	vid_waitflip
	adiw	r25:r24, 1
	rjmp	0b			; next frame please
.L3:
	ldi	r25:r24, 0x0500
.L9:
	;}}}
	; when we arrive here, in frame 5:0
	;{{{  initialise next scrollies (horizontal one is long)
	ldi	r17:r16, S_sfmsg6
	ldi	r18, 80			; sit near the bottom
	ldi	r19, 2			; scrolling off mode
	call	demo_sctext_init

	;}}}
	;{{{  major frames 5 (starfield and scrolly)
.L0:
	call	demo_starfield_advance
	call	fb_clear
	call	demo_starfield_draw

	call	demo_sctext_renderstep
	call	vid_waitflip
	adiw	r25:r24, 1
	cpi	r25, 6			; reached major frame 6 yet?
	breq	9f
	rjmp	0b			; next frame please
.L9:
	;}}}
	; when we arrive here, in frame 6:0
	;{{{  major frame 6, 7, 8  (testing fonts)
.L0:
	call	fb_clear
	ldi	r16, 16
	ldi	r17, 10
	ldi	r18, 'H'
	call	fb_write04char
	ldi	r16, 32
	ldi	r17, 10
	ldi	r18, 'e'
	call	fb_write04char
	ldi	r16, 48
	ldi	r17, 10
	ldi	r18, 'l'
	call	fb_write04char
	ldi	r16, 64
	ldi	r17, 10
	ldi	r18, 'o'
	call	fb_write04char
	ldi	r16, 80
	ldi	r17, 10
	ldi	r18, '.'
	call	fb_write04char

	; testing some scrolly stuff
	ldi	r16, 127
	mov	r17, r24
	andi	r17, 0x7f
	sub	r16, r17
	ldi	r17, 40
	ldi	r18, 'A'
	call	fb_write04char

	; more scrolly stuff
	ldi	r17, -15
	mov	r16, r24
	cpi	r16, 111
	brlo	1f
	andi	r16, 0x3f		; (0-64)
.L1:
	add	r17, r16		; Y position (-15 -> 95)

	ldi	r16, 16
	ldi	r18, 'd'
	call	fb_write04char

	call	vid_waitflip
	adiw	r25:r24, 1

	cpi	r25, 9			; reached major frame 9 yet?
	breq	9f
	rjmp	0b			; next frame please
.L9:
	;}}}
	;{{{  major frame ..., 9  (testing xor circles)
	; general algorithm here lifted from Insolit Dust's stuff on sourceforge (http://insolitdust.sourceforge.net/code.html)
.L0:
	ldi	r16, 64
	mov	r17, r24
	call	math_cosv
	ldi	r18, 64
	add	r16, r18
	mov	r19, r16		; save temporarily

	ldi	r16, 5
	mov	r17, r24
	mul	r16, r17
	mov	r17, r0			; take low-order bits as angle
	ldi	r16, 48
	call	math_sinv
	ldi	r17, 48
	add	r16, r17

	mov	r17, r16
	mov	r16, r19

	call	demo_xorcirc_plain

	mov	r17, r24
	ldi	r16, 67
	add	r17, r16
	ldi	r16, 68
	call	math_cosv
	ldi	r18, 64
	add	r16, r18
	mov	r19, r16		; save temporarily

	mov	r17, r24
	ldi	r16, 68
	add	r17, r16
	ldi	r16, 5
	mul	r16, r17
	mov	r17, r0			; take low-order bits as angle
	ldi	r16, 51
	call	math_sinv
	ldi	r17, 48
	add	r16, r17

	mov	r17, r16
	mov	r16, r19

	call	demo_xorcirc_xor

	call	vid_waitflip
	adiw	r25:r24, 1

	cpi	r25, 10			; reached major frame 10 yet?
	breq	9f
	rjmp	0b			; next frame please

.L9:
	;}}}
	rjmp	prg_code_test_cont

	;{{{  major frames 6, 7, 8  (starfield, scrolly and pacman)
.L0:
	call	demo_starfield_advance
	call	fb_clear
	call	demo_starfield_draw

	call	demo_sctext_renderstep

	;--------- BEGIN HACKY TEST
	ldi	r16, 26
	mov	r17, r24		; minor frame can be angle :)
	lsl	r17			; multiply by 2
	call	math_sinv
	mov	r18, r16

	ldi	r16, 36
	mov	r17, r24
	call	math_cosv
	ldi	r17, 40			; Y offset
	sub	r17, r16		; adjust

	ldi	r16, 30			; X offset
	add	r16, r18		; adjust

	call	demo_pac_render

	ldi	r16, 30
	mov	r17, r24
	call	math_sinv
	ldi	r17, 40			; Y offset
	add	r17, r16		; adjust

	ldi	r16, 100
	mov	r18, r24
	andi	r18, 0x3f		; 0-63
	sub	r16, r18

	call	demo_ghost_render

	ldi	r16, 30
	mov	r17, r24
	lsl	r17
	call	math_cosv
	ldi	r17, 60			; Y offset
	add	r17, r16		; adjust

	ldi	r16, 110
	mov	r18, r24
	andi	r18, 0x3f		; 0-63
	sub	r16, r18

	call	demo_ghost_render

	ldi	r16, 6
	mov	r17, r24
	call	math_sinv
	mov	r19, r16
	ldi	r16, 16
	mov	r18, r24		; minor frame 0-255
	lsr	r18
	lsr	r18			; r18 = minor frame / 4
	add	r16, r18
	ldi	r17, 48
	ldi	r18, 10
	add	r18, r19		; previously computed radius

	call	demo_circle

	;--------- END HACKY TEST

	call	vid_waitflip
	adiw	r25:r24, 1
	cpi	r25, 9			; reached major frame 9 yet?
	breq	9f
	rjmp	0b
.L9:
	;}}}

	; DUMMY: this end loop doesn't really do anything usefor or interesting..
prg_code_test_cont:
.L0:
	call	demo_starfield_advance
	call	fb_clear
	call	demo_starfield_draw
	call	vid_waitflip
	adiw	r25:r24, 1
	rjmp	0b




	ldi	r16, 199		; 4 seconds
.L0:
	call	vid_waitbot1
	call	vid_waitflip
	dec	r16
	brne	0b

	ldi	r20, 1
	ldi	r21, 1
.L1:
	call	fb_clear
	ldi	r16, 0
	ldi	r17, 0
	ldi	r18, 128
	call	fb_xhline
	ldi	r18, 96
	ldi	r16, 5
	call	fb_xvline
	ldi	r16, 0
	ldi	r17, 95
	ldi	r18, 128
	call	fb_xhline
	ldi	r16, 127
	ldi	r17, 0
	ldi	r18, 96
	call	fb_xvline

	;ldi	r16, 2
	;ldi	r17, 0
	;ldi	r18, 96
	;rcall	fb_xvline
	;ldi	r16, 4
	;rcall	fb_xvline
	;ldi	r16, 7
	;rcall	fb_xvline
	;ldi	r16, 11
	;rcall	fb_xvline

	ldi	r16, 4			; X = 32
	ldi	r17, 9
	ldi	r18, 11			; W = 88
	ldi	r19, 86
	ldi	ZH, hi(I_ada_image)
	ldi	ZL, lo(I_ada_image)
	call	fb_pbyteblit


	; moving portions
	ldi	r16, 0
	mov	r17, r21
	ldi	r18, 128
	call	fb_xhline

	mov	r16, r20
	ldi	r17, 0
	ldi	r18, 96
	call	fb_xvline

	; testing arbitrary lines
	call	demo_renderlines

;	ldi	r17, 0
;	ldi	r16, 0
;	mov	r18, r20
;	mov	r19, r21
;	call	fb_drawline
;	ldi	r16, 127
;	neg	r18
;	call	fb_drawline
;	ldi	r17, 95
;	ldi	r16, 0
;	mov	r18, r20
;	mov	r19, r21
;	neg	r19
;	call	fb_drawline

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
	ldi	ZH, hi(S_message)
	ldi	ZL, lo(S_message)

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

	add	ZL, r24
	adc	ZH, r25			; adjust for message offset

	mov	r17, r16
	ldi	r16, 5
	sub	r16, r17		; r16 = 5-0, X position
	ldi	r17, 2			; Y position

	call	fb_writestring

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

	; TESTING SOUND STUFF


	call	vid_waitflip
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

S_sfmsg1:	.const	"   Computing at Kent", 0x00
S_sfmsg2:	.const	"(8-bit)", 0x00
S_sfmsg3:	.const	"   Computer Science ", 0x00
S_sfmsg4:	.const	"Why not try our", 0x00
S_sfmsg5:	.const	"degree programme?", 0x00
S_sfmsg6:	.const	"                     Learn how to do things like bit-manipulation for ",
			"starfields and scrolly text, PWM programming for multi-channel sound ",
			"and PAL video generation with some USART stuff, churning away on an Atmel ATMega2560 (Arduino) at 16 MHz.  ", 0x00

.include "fb-ada.inc"

D_lineset_count:
	.const	2, 0

D_lineset:
	.const16	0x6010, 0x1000
	.const16	0x6010, 0x0010

D_pac_glyph2:
	.const		0x0f, 0x80, 0x3f, 0xe0, 0x7f, 0xf0, 0x7f, 0xf0,
			0xff, 0xc0, 0xfe, 0x00, 0xf0, 0x00, 0xfe, 0x00,
			0xff, 0xc0, 0x7f, 0xf0, 0x7f, 0xf0, 0x3f, 0xe0,
			0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

	.const		0x07, 0xc0, 0x1f, 0xf0, 0x3f, 0xf8, 0x3f, 0xf8,
			0x7f, 0xe0, 0x7f, 0x00, 0x78, 0x00, 0x7f, 0x00,
			0x7f, 0xe0, 0x3f, 0xf8, 0x3f, 0xf8, 0x1f, 0xf0,
			0x07, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

	.const		0x03, 0xe0, 0x0f, 0xf8, 0x1f, 0xfc, 0x1f, 0xfc,
			0x3f, 0xf0, 0x3f, 0x80, 0x3c, 0x00, 0x3f, 0x80,
			0x3f, 0xf0, 0x1f, 0xfc, 0x1f, 0xfc, 0x0f, 0xf8,
			0x03, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

	.const		0x01, 0xf0, 0x07, 0xfc, 0x0f, 0xfe, 0x0f, 0xfe,
			0x1f, 0xf8, 0x1f, 0xc0, 0x1e, 0x00, 0x1f, 0xc0,
			0x1f, 0xf8, 0x0f, 0xfe, 0x0f, 0xfe, 0x07, 0xfc,
			0x01, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

	.const		0x00, 0xf8, 0x03, 0xfe, 0x07, 0xff, 0x07, 0xff,
			0x0f, 0xfc, 0x0f, 0xe0, 0x0f, 0x00, 0x0f, 0xe0,
			0x0f, 0xfc, 0x07, 0xff, 0x07, 0xff, 0x03, 0xfe,
			0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

D_pac_glyph3:
;	.const		0x00, 0x7c, 0x00, 0x01, 0xff, 0x00, 0x03, 0xff, 0x80, 0x03, 0xff, 0x80,		; 12
;			0x07, 0xfe, 0x00, 0x07, 0xf0, 0x00, 0x07, 0x80, 0x00, 0x07, 0xf0, 0x00,		; 24
;			0x07, 0xfe, 0x00, 0x03, 0xff, 0x80, 0x03, 0xff, 0x80, 0x01, 0xff, 0x00,		; 36
;			0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00		; 48
	.const		0x00, 0x7c, 0x00, 0x01, 0xff, 0x00, 0x03, 0xff, 0x80, 0x03, 0xff, 0x80,		; 12
			0x07, 0xff, 0x80, 0x07, 0xfc, 0x00, 0x07, 0x80, 0x00, 0x07, 0xfc, 0x00,		; 24
			0x07, 0xff, 0x80, 0x03, 0xff, 0x80, 0x03, 0xff, 0x80, 0x01, 0xff, 0x00,		; 36
			0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00		; 48

;	.const		0x00, 0x3e, 0x00, 0x00, 0xff, 0x80, 0x01, 0xff, 0xc0, 0x01, 0xff, 0xc0,
;			0x03, 0xff, 0x00, 0x03, 0xf8, 0x00, 0x03, 0xc0, 0x00, 0x03, 0xf8, 0x00,
;			0x03, 0xff, 0x00, 0x01, 0xff, 0xc0, 0x01, 0xff, 0xc0, 0x00, 0xff, 0x80,
;			0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	.const		0x00, 0x3e, 0x00, 0x00, 0xff, 0x80, 0x01, 0xff, 0xc0, 0x01, 0xff, 0xc0,
			0x03, 0xff, 0xc0, 0x03, 0xff, 0xc0, 0x03, 0xf8, 0x00, 0x03, 0xff, 0xc0,
			0x03, 0xff, 0xc0, 0x01, 0xff, 0xc0, 0x01, 0xff, 0xc0, 0x00, 0xff, 0x80,
			0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

;	.const		0x00, 0x1f, 0x00, 0x00, 0x7f, 0xc0, 0x00, 0xff, 0xe0, 0x00, 0xff, 0xe0,
;			0x01, 0xff, 0x80, 0x01, 0xfc, 0x00, 0x01, 0xe0, 0x00, 0x01, 0xfc, 0x00,
;			0x01, 0xff, 0x80, 0x00, 0xff, 0xe0, 0x00, 0xff, 0xe0, 0x00, 0x7f, 0xc0,
;			0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	.const		0x00, 0x1f, 0x00, 0x00, 0x7f, 0xc0, 0x00, 0xff, 0xe0, 0x00, 0xff, 0xe0,
			0x01, 0xff, 0xe0, 0x01, 0xff, 0x00, 0x01, 0xe0, 0x00, 0x01, 0xff, 0x00,
			0x01, 0xff, 0xe0, 0x00, 0xff, 0xe0, 0x00, 0xff, 0xe0, 0x00, 0x7f, 0xc0,
			0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

D_ghost_glyph2:
	.const		0x07, 0x80, 0x1f, 0xe0, 0x3f, 0xf0, 0x4f, 0x38,
			0x06, 0x18, 0x67, 0x98, 0xe7, 0x9c, 0xcf, 0x3c,
			0xff, 0xfc, 0xff, 0xfc, 0xff, 0xfc, 0xff, 0xfc,
			0xdc, 0xec, 0x8c, 0xc4, 0x00, 0x00, 0x00, 0x00

	.const		0x03, 0xc0, 0x0f, 0xf0, 0x1f, 0xf8, 0x27, 0x9c,
			0x03, 0x0c, 0x33, 0xcc, 0x73, 0xce, 0x67, 0x9e,
			0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe,
			0x6e, 0x76, 0x46, 0x62, 0x00, 0x00, 0x00, 0x00

	.const		0x01, 0xe0, 0x07, 0xf8, 0x0f, 0xfc, 0x13, 0xce,
			0x01, 0x86, 0x19, 0xe6, 0x39, 0xe7, 0x33, 0xcf,
			0x3f, 0xff, 0x3f, 0xff, 0x3f, 0xff, 0x3f, 0xff,
			0x37, 0x3b, 0x23, 0x31, 0x00, 0x00, 0x00, 0x00

D_ghost_glyph3:
	.const		0x00, 0xf0, 0x00, 0x03, 0xfc, 0x00, 0x07, 0xfe, 0x00, 0x09, 0xe7, 0x00,
			0x00, 0xc3, 0x00, 0x0c, 0xf3, 0x00, 0x1c, 0xf3, 0x80, 0x19, 0xe7, 0x80,
			0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80,
			0x1b, 0x9d, 0x80, 0x11, 0x98, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

	.const		0x00, 0x78, 0x00, 0x01, 0xfe, 0x00, 0x03, 0xff, 0x00, 0x04, 0xf3, 0x80,
			0x00, 0x61, 0x80, 0x06, 0x79, 0x80, 0x0e, 0x79, 0xc0, 0x0c, 0xf3, 0xc0,
			0x0f, 0xff, 0xc0, 0x0f, 0xff, 0xc0, 0x0f, 0xff, 0xc0, 0x0f, 0xff, 0xc0,
			0x0d, 0xce, 0xc0, 0x08, 0xcc, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

	.const		0x00, 0x3c, 0x00, 0x00, 0xff, 0x00, 0x01, 0xff, 0x80, 0x02, 0x79, 0xc0,
			0x00, 0x30, 0xc0, 0x03, 0x3c, 0xc0, 0x07, 0x3c, 0xe0, 0x06, 0x79, 0xe0,
			0x07, 0xff, 0xe0, 0x07, 0xff, 0xe0, 0x07, 0xff, 0xe0, 0x07, 0xff, 0xe0,
			0x06, 0xe7, 0x60, 0x04, 0x66, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	
	.const		0x00, 0x1e, 0x00, 0x00, 0x7f, 0x80, 0x00, 0xff, 0xc0, 0x01, 0x3c, 0xe0,
			0x00, 0x18, 0x60, 0x01, 0x9e, 0x60, 0x03, 0x9e, 0x70, 0x03, 0x3c, 0xf0,
			0x03, 0xff, 0xf0, 0x03, 0xff, 0xf0, 0x03, 0xff, 0xf0, 0x03, 0xff, 0xf0,
			0x03, 0x73, 0xb0, 0x02, 0x33, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	
	.const		0x00, 0x0f, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x7f, 0xe0, 0x00, 0x9e, 0x70,
			0x00, 0x0c, 0x30, 0x00, 0xcf, 0x30, 0x01, 0xcf, 0x38, 0x01, 0x9e, 0x78,
			0x01, 0xff, 0xf8, 0x01, 0xff, 0xf8, 0x01, 0xff, 0xf8, 0x01, 0xff, 0xf8,
			0x01, 0xb9, 0xd8, 0x01, 0x19, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

D_notetab:
	.const16	0x3bb9, 0x385f, 0x3535, 0x3238, 0x2f67, 0x2cbe, 0x2a3b, 0x27dc, 0x259f, 0x2383, 0x2185, 0x1fa3		; C2 -> B2 (65.406 -> 123.47)
	.const16	0x1ddd, 0x1c20, 0x1a9b, 0x191c, 0x17b4, 0x165f, 0x151d, 0x13ee, 0x12d0, 0x11c1, 0x10c2, 0x0fd2		; C3 -> B3 (130.81 -> 246.94)
	.const16	0x0eee, 0x0e18, 0x0d4d, 0x0c8e, 0x0bda, 0x0b2f, 0x0a8f, 0x09f7, 0x0968, 0x08e1, 0x0861, 0x07e9		; C4 -> B4 (261.62 -> 493.88)
	.const16	0x0777, 0x070c, 0x06a7, 0x0647, 0x05ed, 0x0598, 0x0547, 0x04fc, 0x04b4, 0x0470, 0x0431, 0x03f4		; C5 -> B5 (523.25 -> 987.77)
	.const16	0x03bc, 0x0386, 0x0353, 0x0324, 0x02f6, 0x02cc, 0x02a4, 0x027e, 0x025a, 0x0238, 0x0218, 0x01fa		; C6 -> B6 (1046.5 -> 1975.5)
	.const16	0x0100													; made up for now..

D_rdrpnts_static:
	.const		40, 50, -5, 10
	.const		40, 50, 10, 0
	.const		35, 60, 40, 0
	.const		50, 40, 0, 10
	.const		50, 40, 10, 0
	.const		60, 40, 0, 10
	.const		60, 50, 10, 0
	.const		70, 50, 5, 10

D_starfield_init:
	.const		167, 11, 0, 180, 71, 1, 204, 73, 1, 201, 65, 0,
			166, 56, 1, 254, 30, 0, 254, 44, 1, 149, 76, 0,
			241, 83, 0, 169, 81, 1, 163, 63, 1, 136, 36, 0,
			214, 18, 1, 216, 14, 1, 156, 14, 1, 204, 50, 1

	.const		166, 11, 1, 149, 18, 1, 235, 40, 1, 133, 29, 2,
			163, 92, 2, 226, 24, 2, 220, 32, 0, 219, 77, 2,
			145, 55, 2, 181, 55, 1, 206, 6, 0, 208, 17, 2,
			182, 31, 2, 175, 45, 2, 145, 16, 1, 217, 20, 2

	.const		136, 11, 2, 170, 29, 1, 159, 44, 1, 172, 77, 0,
			232, 67, 1, 203, 33, 1, 130, 79, 0, 205, 28, 2,
			131, 27, 0, 189, 49, 0, 150, 8, 1, 184, 66, 2,
			229, 34, 1, 133, 46, 1, 246, 10, 1, 238, 92, 2


;		CHANNEL1		CHANNEL2		CHANNEL3
;-----------------------------------------------------------------------------------
D_soundtrk:
	.const	NOTE_OFF,	0x00,	NOTE_OFF,	0x00,	NOTE_OFF,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_END,	0xff,	NOTE_END,	0xff,	NOTE_END,	0xff,		; PREMATURE: THE END

		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x01,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x02,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x03,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x02,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x01,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x02,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x03,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x02,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x01,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x02,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x03,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x02,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x01,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x02,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_G3,	0x03,	NOTE_G4,	0x82,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,

		NOTE_G3,	0x02,	NOTE_C4,	0x02,	NOTE_E4,	0x02,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_A3,	0x02,	NOTE_C4,	0x02,	NOTE_F4,	0x02,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_B3,	0x02,	NOTE_D4,	0x02,	NOTE_G4,	0x02,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_A3,	0x02,	NOTE_C4,	0x02,	NOTE_F4,	0x02,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,

		NOTE_G3,	0x02,	NOTE_C4,	0x02,	NOTE_E4,	0x02,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_A3,	0x02,	NOTE_C4,	0x02,	NOTE_F4,	0x02,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_B3,	0x02,	NOTE_D4,	0x02,	NOTE_G4,	0x02,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_A3,	0x02,	NOTE_C4,	0x02,	NOTE_F4,	0x02,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_OFF,	0x00,	NOTE_OFF,	0x00,	NOTE_OFF,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,
		NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,	NOTE_NOCHANGE,	0x00,

		NOTE_END,	0xff,	NOTE_END,	0xff,	NOTE_END,	0xff		; THE END

.include "sincostab.inc"
.include "fb-xorcirc.inc"

.include "fb-font04b.inc"

