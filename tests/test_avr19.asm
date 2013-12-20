;
;	test_avr19.asm -- proper test for ATMEGA328 (Arduino Duemilanove)
;

.mcu	"ATMEGA328"

; include files part of the compiler infrastructure
.include "atmega328.inc"

.text
.include "atmega328-imap.inc"

.include "dfrobot-arduino-ether.inc"

.function delayloop ()				;/*{{{*/
	push	r18
	push	r17
	push	r16

	ldi	r16, 0x20
.L0:
	ldi	r17, 0xff
.L1:
	ldi	r18, 0xff
.L2:
	nop
	nop
	nop
	nop

	dec	r18
	brne	2b

	dec	r17
	brne	1b

	dec	r16
	brne	0b

	pop	r16
	pop	r17
	pop	r18
.endfunction					;/*}}}*/


; some constant values
D_gateway:
	.const	192,168,16,1
D_submask:
	.const	255,255,255,0
D_hwaddr:
	.const	0x90,0xa2,0xda,0x0d,0xef,0x00
D_ipaddr:
	.const	192,168,16,133

VEC_reset:
	cli
	ldi	r16, hi(RAMEND)
	out	SPH, r16
	ldi	r16, lo(RAMEND)
	out	SPL, r16
	sei

	ldi	r16, 0x3f
	out	DDRC, r16			; PC0-5 output
	out	PORTC, r16			; lights off

	cbi	PORTC, 0

	call	dfr_ether_init
	call	dfr_ether_reset

	cbi	PORTC, 2

	ldi	r27:r26, W5100_GAR0
	ldi	r31:r30, D_gateway
	call	dfr_ether_write4code

	cbi	PORTC, 3

	ldi	r27:r26, W5100_SUBR0
	ldi	r31:r30, D_submask
	call	dfr_ether_write4code

	cbi	PORTC, 4

	ldi	r27:r26, W5100_SHAR0
	ldi	r31:r30, D_hwaddr
	call	dfr_ether_write6code

	cbi	PORTC, 5

	ldi	r27:r26, W5100_SIPR0
	ldi	r31:r30, D_ipaddr
	call	dfr_ether_write4code

	sbi	PORTC, 0
.L0:
	sleep
	rjmp	0b
