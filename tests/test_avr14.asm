;
;	test_avr14.asm -- testing TM12864 LCD module (connected in the regular Arduino way for GLCD)
;

.mcu	"ATMEGA1280"


; include files part of the compiler infrastructure
.include "atmega1280.inc"

.data
.org	0x400
V_framebuffer: .space 0x400

.text
.include "atmega1280-imap.inc"

.include "tm12864-lcd.asm"


; expects "intensity" in r2, running value in r3
add_and_light:
	add	r3, r2
	brcc	aal_1
	sbi	PORTB, 7
	rjmp	aal_2
aal_1:
	cbi	PORTB, 7
aal_2:
	ret

; runs the specified brightness (in r2) for a fixed amount of time
delayloop:
	push	r18
	push	r17
	push	r16

	ldi	r16, 0x2
dloop_0:
	ldi	r17, 0x7f
dloop_1:
	rcall	add_and_light
	ldi	r18, 0x1f
dloop_2:
	nop
	nop
	nop
	nop

	dec	r18
	brne	dloop_2

	dec	r17
	brne	dloop_1

	dec	r16
	brne	dloop_0

	pop	r16
	pop	r17
	pop	r18
	ret

; initialises the framebuffer with some garbage!
init_framebuffer:
	push	XH
	push	XL
	push	r18
	push	r17
	push	r16

	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)

	ldi	r16, 0x00
	ldi	r17, 0x00
.L0:
	ldi	r18, 0x00
.L1:
	st	X+, r16
	inc	r16

	inc	r18
	cpi	r18, 0x10		; column 16?
	brne	1b
	inc	r17
	cpi	r17, 0x40		; row 64?
	brne	0b

	; else all done

	pop	r16
	pop	r17
	pop	r18
	pop	XL
	pop	XH
	ret

; start here

reset:
	cli
	ldi	r16, hi(RAMEND)
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei

	call	tm12864_init
	ldi	r16, 0x00
	call	tm12864_set_start
	call	tm12864_dpy_off
	call	tm12864_dpy_on

	rcall	init_framebuffer

	ldi	XH, hi(V_framebuffer)
	ldi	XL, lo(V_framebuffer)
	call	tm12864_dump_buffer

	ldi	r16, 0xc0
	out	DDRB, r16

	cbi	PORTB, 7
	clr	r3
	clr	r2

	clr	r16
.L0:
	call	delayloop		; will do lights as appropriate
	inc	r2
	brne	0b			; until r2 overflows to zero, loop
	dec	r2

	inc	r16
	andi	r16, 0x3f
	call	tm12864_set_start

.L1:
	call	delayloop
	dec	r2
	breq	2f
	rjmp	1b

.L2:

	inc	r16
	andi	r16, 0x3f
	call	tm12864_set_start

	cbi	PORTB, 7

	;ldi	XH, hi(V_framebuffer)
	;ldi	XL, lo(V_framebuffer)
	;call	tm12864_dump_buffer
	rjmp	0b
