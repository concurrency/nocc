;
;	tm12864-lcd.asm -- LCD module code for TM12864 connected to Arduino
;
;	PA0-7	<=>	DB0-7
;	PC0	->	E
;	PC1	->	D/I
;	PC2	->	R/W
;	PC3	->	CS1
;	PC4	->	CS2
;

.equ	TM_DDRA		=0xff		; all output
.equ	TM_PORTA	=0x00		; all zero to start with

.equ	TM_DDRC		=0x1f		; PC0-4
.equ	TM_PORTC	=0x00		; all zero to start with

.equ	TM_E_BIT	=0		; PC0
.equ	TM_DI_BIT	=1		; PC1
.equ	TM_RW_BIT	=2		; PC2
.equ	TM_CS1_BIT	=3		; PC3
.equ	TM_CS2_BIT	=4		; PC4

.include "tm12864-font5x7.inc"		; defines 'font_table_5x7'

tm12864_init:				;{{{  SUB: initialise LCD module ports
	push	r16

	ldi	r16, TM_DDRA
	out	DDRA, r16
	ldi	r16, TM_PORTA
	out	PORTA, r16

	ldi	r16, TM_DDRC
	out	DDRC, r16
	ldi	r16, TM_PORTC
	out	PORTC, r16

	pop	r16
	ret

;}}}

tm12864_set_chip:			;{{{ SUB: enables/disables chip select, r16={0=none,1=left,2=right}
	cpi	r16, 0x01
	breq	0f
	cpi	r16, 0x02
	breq	1f
	cbi	PORTC, TM_CS1_BIT
	cbi	PORTC, TM_CS2_BIT
	ret
.L0:
	cbi	PORTC, TM_CS1_BIT
	sbi	PORTC, TM_CS2_BIT
	ret
.L1:
	sbi	PORTC, TM_CS1_BIT
	cbi	PORTC, TM_CS2_BIT
	ret

;}}}
tm12864_pulse_enable:			;{{{ SUB: do a read/write pulse
	nop
	nop
	nop
	nop
	nop
	sbi	PORTC, TM_E_BIT
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
	cbi	PORTC, TM_E_BIT
	nop
	nop
	nop
	nop
	nop
	ret

;}}}
tm12864_rpulse_enable:			;{{{ SUB: do a read/write pulse (data picked up from PINA -> r16 before dropping E)
	nop
	nop
	nop
	nop
	nop
	sbi	PORTC, TM_E_BIT
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
	in	r16, PINA
	nop
	cbi	PORTC, TM_E_BIT
	nop
	nop
	nop
	nop
	nop
	ret

;}}}
tm12864_wait_ready:			;{{{ SUB: wait for ready state on active chip
	push	r16

	ldi	r16, 0x00
	out	PORTA, r16
	nop
	out	DDRA, r16		; set port-A to input
	nop
	sbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
.L0:
	rcall	tm12864_rpulse_enable	; do read pulse
	andi	r16, 0x80
	brne	0b

	; chip ready to accept more data :)
	ldi	r16, 0xff
	out	DDRA, r16		; set port-A back to output
	ldi	r16, 0x00
	out	PORTA, r16

	pop	r16
	ret
;}}}

tm12864_dpy_on:				;{{{ SUB: turn display on
	push	r16
	push	r17

	ldi	r16, 0x01
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	ldi	r17, 0x3f		; display on
	out	PORTA, r17
	rcall	tm12864_pulse_enable	; enable pulse

	ldi	r16, 0x02
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	ldi	r17, 0x3f		; display on
	out	PORTA, r17
	rcall	tm12864_pulse_enable

	pop	r17
	pop	r16
	ret
;}}}
tm12864_dpy_off:			;{{{ SUB: turn display off
	push	r16
	push	r17

	ldi	r16, 0x01
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	ldi	r17, 0x3e		; display off
	out	PORTA, r17
	rcall	tm12864_pulse_enable	; enable pulse

	ldi	r16, 0x02
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	ldi	r17, 0x3e		; display off
	out	PORTA, r17
	rcall	tm12864_pulse_enable

	pop	r17
	pop	r16
	ret

;}}}
tm12864_set_start:			;{{{ SUB: set start line (r16=[0..63])
	push	r16
	push	r17

	mov	r17, r16
	ldi	r16, 0x01
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	ori	r17, 0xc0
	out	PORTA, r17
	rcall	tm12864_pulse_enable	; enable pulse

	ldi	r16, 0x02
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	ori	r17, 0xc0
	out	PORTA, r17
	rcall	tm12864_pulse_enable

	pop	r17
	pop	r16
	ret
;}}}
tm12864_set_start_half:			;{{{ SUB: set start line on selected chip (r16=[0..63])
	push	r16

	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	ori	r16, 0xc0
	out	PORTA, r16
	rcall	tm12864_pulse_enable

	pop	r16
	ret
;}}}
tm12864_set_page:			;{{{ SUB: set page (r16=[0..7])
	push	r16
	push	r17

	mov	r17, r16
	ldi	r16, 0x01
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	andi	r17, 0x07
	ori	r17, 0xb8
	out	PORTA, r17
	rcall	tm12864_pulse_enable	; enable pulse

	ldi	r16, 0x02
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	andi	r17, 0x07
	ori	r17, 0xb8
	out	PORTA, r17
	rcall	tm12864_pulse_enable

	pop	r17
	pop	r16
	ret
;}}}
tm12864_set_page_half:			;{{{ SUB: set page on selected chip (r16=[0..7])
	push	r16

	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	ori	r16, 0xb8
	out	PORTA, r16
	rcall	tm12864_pulse_enable

	pop	r16
	ret
;}}}
tm12864_set_addr:			;{{{ SUB: set Y address (r16=[0..63])
	push	r16
	push	r17

	mov	r17, r16
	ldi	r16, 0x01
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	andi	r17, 0x3f
	ori	r17, 0x40
	out	PORTA, r17
	rcall	tm12864_pulse_enable	; enable pulse

	ldi	r16, 0x02
	rcall	tm12864_set_chip
	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	andi	r17, 0x3f
	ori	r17, 0x40
	out	PORTA, r17
	rcall	tm12864_pulse_enable

	pop	r17
	pop	r16
	ret
	
;}}}
tm12864_set_addr_half:			;{{{ SUB: set Y address on selected chip (r16=[0..63])
	push	r16

	rcall	tm12864_wait_ready
	cbi	PORTC, TM_RW_BIT
	cbi	PORTC, TM_DI_BIT
	ori	r16, 0x40
	out	PORTA, r16
	rcall	tm12864_pulse_enable

	pop	r16
	ret
;}}}
tm12864_cycle_write:			;{{{ SUB: write data cycle (r16 = data)
	cbi	PORTC, TM_RW_BIT
	sbi	PORTC, TM_DI_BIT
	out	PORTA, r16
	rcall	tm12864_pulse_enable
	cbi	PORTC, TM_DI_BIT
	ret
;}}}

tm12864_dump_buffer:			;{{{ SUB: dumps memory image (at X=r27:r26) to device
	push	r16
	push	r17
	push	r18
	push	XL
	push	XH

	ldi	r18, 0x00
	ldi	r16, 0x01
.L0:
	rcall	tm12864_set_chip		; chip 1 (left) or 2 (right)
	; adjust X to start of memory for this chunk
	pop	XH
	pop	XL
	push	XL
	push	XH
	cpi	r16, 0x01
	breq	1f				; first page, no adjustment needed
	adiw	X, 32				; starts at offset 64
	adiw	X, 32
.L1:
	rcall	tm12864_wait_ready
	mov	r16, r18
	andi	r16, 0x07
	rcall	tm12864_set_page_half		; set page (0-7)

	ldi	r16, 0x00
	rcall	tm12864_set_addr_half		; set Y to 0

	ldi	r17, 0x40
.L2:
	rcall	tm12864_wait_ready
	ld	r16, X+				; load byte
	rcall	tm12864_cycle_write		; write out to device

	dec	r17
	brne	2b				; loop until whole column done

	; here means one row done, onto next page
	adiw	X, 32				; skip next 64 bytes (other half of screen)
	adiw	X, 32

	inc	r18
	cpi	r18, 8
	breq	3f				; switching to chip 2 (right)
	cpi	r18, 16
	brne	1b				; next page and process

	; here means we're done!
	rjmp	4f
.L3:
	ldi	r16, 0x02			; chip 2 (right)
	rjmp	0b

.L4:
	clr	r16
	rcall	tm12864_set_chip

	pop	XH
	pop	XL
	pop	r18
	pop	r17
	pop	r16
	ret
;}}}

tm12864_fb_init:			;{{{  SUB: initialises framebuffer at X (blanks it)
	push	r16
	push	r17
	push	XH
	push	XL

	; 1024 bytes, so (256 * 4)
	ldi	r16, 0x00
	ldi	r17, 0x00
.L0:
	st	X+, r16
	st	X+, r16
	st	X+, r16
	st	X+, r16
	inc	r17
	brne	0b

	pop	XL
	pop	XH
	pop	r17
	pop	r16
	ret
;}}}
tm12864_fb_setpixel:			;{{{  SUB: sets pixel in framebuffer at X, at coords in r16,r17
	push	r16
	push	r17
	push	r18
	push	YH
	push	YL

	mov	YH, XH			; Y = X
	mov	YL, XL

	mov	r18, r17
	lsr	r18
	lsr	r18
	lsr	r18
	lsr	r18
	andi	r18, 0x03
	add	YH, r18			; adjust high-offset by ((Y >> 4) & 0x03)

	sbrc	r17, 3			; skip if bit 3 in Y is clear
	ori	r16, 0x80		; else set high-bit in X
	clr	r18
	add	YL, r16			; adjust low-offset by (X + ((Y << 4) & 0x80))
	adc	YH, r18			; carry into high-offset

	; Y register now contains byte of interest
	ld	r18, Y
	andi	r17, 0x07		; low 3 bits specify bit-position in screen-byte
	inc	r17
	clr	r16
	sec
.L0:
	rol	r16
	dec	r17
	brne	0b

	or	r18, r16		; or in the particular bit
	st	Y, r18			; write back

	pop	YL
	pop	YH
	pop	r18
	pop	r17
	pop	r16
	ret
;}}}
tm12864_fb_clrpixel:			;{{{  SUB: clears pixel in framebuffer at X, at coords in r16,r17
	push	r16
	push	r17
	push	r18
	push	YH
	push	YL

	mov	YH, XH			; Y = X
	mov	YL, XL

	mov	r18, r17
	lsr	r18
	lsr	r18
	lsr	r18
	lsr	r18
	andi	r18, 0x03
	add	YH, r18			; adjust high-offset by ((Y >> 4) & 0x03)

	sbrc	r17, 3			; skip if bit 3 in Y is clear
	ori	r16, 0x80		; else set high-bit in X
	clr	r18
	add	YL, r16			; adjust low-offset by (X + ((Y << 4) & 0x80))
	adc	YH, r18			; carry into high-offset

	; Y register now contains byte of interest
	ld	r18, Y
	andi	r17, 0x07		; low 3 bits specify bit-position in screen-byte
	inc	r17
	ser	r16
	clc
.L0:
	rol	r16
	dec	r17
	brne	0b

	and	r18, r16		; and out the particular bit
	st	Y, r18			; write back

	pop	YL
	pop	YH
	pop	r18
	pop	r17
	pop	r16
	ret
;}}}

tm12864_fb_setbyte:			;{{{  SUB: write the 8-bits in r18 to the framebuffer at X, at position (r16, r17*8)
	push	r16
	push	r17
	push	YH
	push	YL

	movw	YL, XL			; Y = X
	clc
	ror	r17			; Y /= 2 for high offset, LSB kept in carry
	brcc	0f
	ori	r16, 0x80		; if Y & 1, X |= 0x80
.L0:
	add	YH, r17
	clr	r17
	add	YL, r16			; set low-offset += X
	adc	YH, r17			; add in carry

	; Y now has the byte address we're interested in
	st	Y, r18

	pop	YL
	pop	YH
	pop	r17
	pop	r16
	ret

;}}}
tm12864_fb_setbytes_pm:			;{{{  SUB: write r18 bytes from Z (in program memory) to the framebuffer at X, starting at (r16, r17*8)
	push	r16
	push	r17
	push	r18
	push	r19
	push	ZH
	push	ZL

	mov	r19, r18		; we need r18
.L0:
	lpm	r18, Z+			; load byte from Z++
	rcall	tm12864_fb_setbyte

	inc	r16			; X++
	dec	r19			; count--
	brne	0b

	pop	ZL
	pop	ZH
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	ret

;}}}
tm12864_fb_writechar:			;{{{  SUB: writes the ASCII char in r18 to the framebuffer at X, starting at (r16,r17*8), using 5x7 font; updates r16
	push	ZH
	push	ZL
	push	r17
	push	r18
	push	r19

	; Note: we know we only have chars 32 -> 127 inclusive.
	cpi	r18, 32
	brlo	2f
	cpi	r18, 128
	brsh	2f

	subi	r18, 32			; adjust for index

	ldi	ZH, hi(font_table_5x7)	; byte offset of table start
	ldi	ZL, lo(font_table_5x7)

	; multiply r18 by 5 to get actual character-data offset
	ldi	r19, 5
	mul	r18, r19		; result in r1:r0

	add	ZL, r0			; add to Z
	adc	ZH, r1

	ldi	r18, 5			; 5 bytes wide
	rcall	tm12864_fb_setbytes_pm
	ldi	r18, 6
	add	r16, r18		; update X position for next chatacter

.L2:
	pop	r19
	pop	r18
	pop	r17
	pop	ZL
	pop	ZH
	ret

;}}}
tm12864_fb_writestring_pm:		;{{{  SUB: writes the ASCII string pointed to by Z (progmem) for r18 chars into the framebuffer at X, starting at (r16,r17*8)
	push	r16
	push	r18
	push	r19
	push	ZH
	push	ZL

	mov	r19, r18		; put character count in r19
.L0:
	lpm	r18, Z+			; load ASCII character
	rcall	tm12864_fb_writechar
	dec	r19
	brne	0b

	pop	ZL
	pop	ZH
	pop	r19
	pop	r18
	pop	r16
	ret
;}}}


