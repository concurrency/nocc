;
;	test_avr15.asm -- more TM12864 LCD module testing
;

.mcu	"ATMEGA1280"

.include "atmega1280.inc"

.data
.org	0x400
V_framebuffer:
	.space 0x400
V_btnstatus:
	.space 1
V_spritepos:
	.space 1

.text
.include "atmega1280-imap.inc"
.include "tm12864-lcd.asm"

; interrupt vectors
VEC_int0:
VEC_int1:
VEC_int2:
	push	r16
	push	r17
	in	r17, SREG

	in	r16, PIND
	andi	r16, 0x07
	sts	PORTL, r16
	com	r16			; physical lines are inverted (button-pressed = 0, released = 1)
	andi	r16, 0x07
	sts	V_btnstatus, r16

	out	SREG, r17
	pop	r17
	pop	r16
	reti


msdelay:	;{{{  SUB: millisecond delay (in r16)
	ret

	push	r16
	push	r17
	push	r18

.L0:
	tst	r16
	breq	3f

	ldi	r17, 20
.L1:
	ldi	r18, 100
.L2:
	nop
	nop
	nop
	nop
	nop
	nop
	dec	r18
	brne	2b
	dec	r17
	brne	1b
	dec	r16
	rjmp	0b

.L3:
	pop	r18
	pop	r17
	pop	r16
	ret
;}}}


;
; start here
;
VEC_reset:
	cli				; disable interrupts
	ldi	r16, hi(RAMEND)		; setup stack pointer
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei				; enable interrupts

	ldi	r16, 0xff
	sts	DDRL, r16		; port L all output
	ldi	r16, 0x55
	sts	PORTL, r16		; bit pattern

	;	adding some buttons:
	;	connected to int0-int2
	ldi	r16, 0x15		; any-edge triggering int 0,1,2
	sts	EICRA, r16
	ldi	r16, 0x07		; interrupts 0,1,2 enabled
	out	EIMSK, r16

	clr	r16
	sts	V_btnstatus, r16
	ldi	r16, 60
	sts	V_spritepos, r16

	call	tm12864_init		; display init
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	call	tm12864_fb_init		; framebuffer init
	clr	r16
	call	tm12864_set_start	; start line = 0
	call	tm12864_dpy_on		; display on

	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	call	tm12864_dump_buffer	; dump image

	;	draw a simple border
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)

	clr	r16
	clr	r17
	ldi	r18, 0x80
	call	tm12864_fb_hline
	ldi	r17, 0x3f
	call	tm12864_fb_hline
	clr	r17
	ldi	r18, 0x40
	call	tm12864_fb_vline
	ldi	r16, 0x7f
	call	tm12864_fb_vline
	
	call	tm12864_dump_buffer	; dump image

	;	emit some text
	ldi	r16, 2
	ldi	r17, 0x01		; row 1
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	ldi	r18, 'a'
	call	tm12864_fb_writechar
	ldi	r18, '0'
	call	tm12864_fb_writechar
	ldi	r18, 'Z'
	call	tm12864_fb_writechar
	ldi	r18, '9'
	call	tm12864_fb_writechar

;	ldi	r17, 0x02		; row 2
;	clr	r16
;	ldi	XH, hi(V_framebuffer)
;	ldi	XL, lo(V_framebuffer)
;	ldi	r18, 0xfc
;	call	tm12864_fb_setbyte
;	inc	r16
;	ldi	r18, 0x3e
;	call	tm12864_fb_setbyte
;	inc	r16
;	ldi	r18, 0xfb
;	call	tm12864_fb_setbyte
;	inc	r16
;	ldi	r18, 0x3f
;	call	tm12864_fb_setbyte
;	inc	r16
;	ldi	r18, 0x3f
;	call	tm12864_fb_setbyte
;	inc	r16
;	ldi	r18, 0xfb
;	call	tm12864_fb_setbyte
;	inc	r16
;	ldi	r18, 0x3e
;	call	tm12864_fb_setbyte
;	inc	r16
;	ldi	r18, 0xfc
;	call	tm12864_fb_setbyte


	ldi	r16, 5
	ldi	r17, 2			; row 2
	ldi	r18, 19			; message length
	ldi	ZH, hi(message)
	ldi	ZL, lo(message)
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	call	tm12864_fb_writestring_pm

	call	tm12864_dump_buffer	; dump image

actloop:
	lds	r16, V_btnstatus
	tst	r16
	brne	1f
	sleep
	rjmp	actloop
.L1:
	; something is pressed
	mov	r17, r16
	andi	r17, 0x01		; left
	brne	2f
	mov	r17, r16
	andi	r17, 0x04		; right
	brne	3f
	; else fire

	rjmp	actloop
.L2:
	; move left
	lds	r16, V_spritepos
	tst	r16
	breq	4f			; cannot move left anymore
	dec	r16
	sts	V_spritepos, r16
	rjmp	5f

.L3:
	; move right
	lds	r16, V_spritepos
	cpi	r16, 0x78
	breq	4f			; cannot move right anymore
	inc	r16
	sts	V_spritepos, r16
	rjmp	5f

.L4:
	; no change, just delay a moment
	ldi	r16, 5
	rcall	msdelay
	rjmp	actloop

.L5:
	; something changed, redraw sprite and delay
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	ldi	r16, 7
	call	tm12864_fb_clearrow
	lds	r16, V_spritepos
	ldi	r17, 7
	ldi	ZH, hi(shipsprite)
	ldi	ZL, lo(shipsprite)
	ldi	r18, 8
	call	tm12864_fb_setbytes_pm

	; DEBUG: V_btnstatus on the screen
	lds	r18, V_btnstatus
	andi	r18, 0x0f
	ldi	r16, 120
	ldi	r17, 1		; row 1
	call	tm12864_fb_writenibble

	call	tm12864_dump_buffer

	ldi	r16, 5
	rcall	msdelay
	rjmp	actloop


message:
.const	"Arduino Invaders :)"

shipsprite:
.const	0x1c, 0x30, 0xf0, 0x3f, 0x3f, 0xf0, 0x30, 0x1c
aliensprite:
.const	0xfc, 0x3e, 0xfb, 0x3f, 0x3f, 0xfb, 0x3e, 0xfc

