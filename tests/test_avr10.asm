;
;	test_avr10.asm -- testing data space
;

.mcu	"ATMEGA1280"

.equ	PORTB	=0x05
.equ	DDRB	=0x04
.equ	PINB	=0x03

.equ	SREG	=0x3f
.equ	SPH	=0x3e
.equ	SPL	=0x3d
.equ	RAMEND	=0x21ff			; SRAM occupies 0x0200 -> 0x21ff
.equ	RAMSTART	=0x200

.data
.org	RAMSTART

V_foo:	.space 1
V_bar:	.space 1
V_arry:	.space 20

.text
.org	0
	rjmp	reset

.org	0x100
reset:
	cli
	ldi	r16, hi(RAMEND)
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei

	ldi	r16, 0x80
	out	DDRB, r16

	ldi	r16, 0x01
	sts	V_foo, r16

	lds	r17, V_foo

	ldi	r27, hi(V_arry)
	ldi	r26, lo(V_arry)

	ldi	r29, hi(V_tmp)
	ldi	r30, lo(V_tmp)

	st	Y+, r17
	st	Y+0, r17
	clr	r18
	st	Y+1, r18
	st	X, r19
	adiw	Y, 4

	ld	r20, Y

loop:
	sbi	PORTB, 7			; LED on

	rjmp	loop

.eeprom
.org	0
.const	"Fred", 0, 12
.const16	RAMSTART, V_tmp + 2

.data
.org	RAMSTART + 0x100
V_tmp:	.space 2

