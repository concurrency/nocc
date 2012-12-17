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

.L0:
	sleep
	rjmp	0b

