; testing something specific

.text
.org 0x100

.def	ZH=r31
.def	ZL=r30
.def	XH=r27
.def	XL=r26

ZipLabel:
	ldi	ZH:ZL, 0x3512
	ldi	XH:XL, 0x0c00
	rjmp	10f

.L10:
	ldi	r20, 96
.L101:
	ldi	r18, 16
.L102:
	lpm	r0, Z+
	st	X+, r0
	dec	r18
	brne	102b
	dec	r20
	breq	103f
	adiw	ZH:ZL, 16
	rjmp	101b
.L103:
	nop
	nop
	rjmp	103b

