;
;	test_avr15.asm -- more TM12864 LCD module testing
;

.mcu	"ATMEGA1280"

.include "atmega1280.inc"

.data
.org	0x400
V_framebuffer:
	.space 0x400

.text
.include "atmega1280-imap.inc"
.include "tm12864-lcd.asm"


;
; start here
;
reset:
	cli				; disable interrupts
	ldi	r16, hi(RAMEND)		; setup stack pointer
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei				; enable interrupts

	ldi	r16, 0xff
	ldi	ZH, hi(DDRL)
	ldi	ZL, lo(DDRL)
	st	Z, r16			; port L all output
	ldi	r16, 0x55
	ldi	ZH, hi(PORTL)
	ldi	ZL, lo(PORTL)
	st	Z, r16			; bit pattern

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

	; FIXME: code here!
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	ldi	r18, 0x00
.L2:
	ldi	r16, 0x00		; start X
.L1:
	ldi	r17, 0x00		; Y
	add	r17, r18
	call	tm12864_fb_setpixel
	ldi	r17, 0x3f
	sub	r17, r18
	call	tm12864_fb_setpixel

	inc	r16
	cpi	r16, 0x80
	brne	1b

	inc	r18
	inc	r18
	cpi	r18, 0x04
	brne	2b
	
	call	tm12864_dump_buffer	; dump image

	clr	r16
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

	ldi	r17, 0x02		; row 2
	clr	r16
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	ldi	r18, 0x55
	call	tm12864_fb_setbyte
	inc	r16
	ldi	r18, 0xaa
	call	tm12864_fb_setbyte
	inc	r16
	ldi	r18, 0xff
	call	tm12864_fb_setbyte
	inc	r16
	ldi	r18, 0x81
	call	tm12864_fb_setbyte
	inc	r16
	ldi	r18, 0xff
	call	tm12864_fb_setbyte

	ldi	r17, 0x03		; row 3
	clr	r16
	ldi	r18, 125		; 125 pixels (25 chars)
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	ldi	ZH, hi(font_table_5x7)
	ldi	ZL, lo(font_table_5x7)
	call	tm12864_fb_setbytes_pm

	call	tm12864_dump_buffer	; dump image

	clr	r16
	ldi	r17, 0x04		; row 4
	ldi	r18, 13			; message length
	ldi	ZH, hi(message)
	ldi	ZL, lo(message)
	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	call	tm12864_fb_writestring_pm
	call	tm12864_dump_buffer	; dump image

.L0:
	sleep
	rjmp	0b

message:
.const	"Hello Lucy :)"

