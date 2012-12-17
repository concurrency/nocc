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

.equ	TM_DDRC		=0x9f		; PC0-4+7
.equ	TM_PORTC	=0x00		; all zero to start with

.equ	TM_E_BIT	=0		; PC0
.equ	TM_DI_BIT	=1		; PC1
.equ	TM_RW_BIT	=2		; PC2
.equ	TM_CS1_BIT	=3		; PC3
.equ	TM_CS2_BIT	=4		; PC4

.equ	TM_DEBUG_BIT	=7		; PC7

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
	breq	tm12864_sc_1
	cpi	r16, 0x02
	breq	tm12864_sc_2
	cbi	PORTC, TM_CS1_BIT
	cbi	PORTC, TM_CS2_BIT
	rjmp	tm12864_sc_3
tm12864_sc_1:
	cbi	PORTC, TM_CS1_BIT
	sbi	PORTC, TM_CS2_BIT
	rjmp	tm12864_sc_3
tm12864_sc_2:
	sbi	PORTC, TM_CS1_BIT
	cbi	PORTC, TM_CS2_BIT
tm12864_sc_3:
	nop
	nop
	nop
	nop
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
tm12864_wr_1:
	rcall	tm12864_rpulse_enable	; do read pulse
	andi	r16, 0x80
	brne	tm12864_wr_1

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
tm12864_db_1:
	rcall	tm12864_set_chip		; chip 1 (left) or 2 (right)
	; adjust X to start of memory for this chunk
	pop	XH
	pop	XL
	push	XL
	push	XH
	cpi	r16, 0x01
	breq	tm12864_db_2			; first page, no adjustment needed
	adiw	X, 32				; starts at offset 64
	adiw	X, 32
tm12864_db_2:
	rcall	tm12864_wait_ready
	mov	r16, r18
	andi	r16, 0x07
	rcall	tm12864_set_page_half		; set page (0-7)

	ldi	r16, 0x00
	rcall	tm12864_set_addr_half		; set Y to 0

	ldi	r17, 0x40
tm12864_db_3:
	rcall	tm12864_wait_ready
	ld	r16, X+				; load byte
	rcall	tm12864_cycle_write		; write out to device

	dec	r17
	brne	tm12864_db_3			; loop until whole column done

	; here means one row done, onto next page
	adiw	X, 32				; skip next 64 bytes (other half of screen)
	adiw	X, 32

	inc	r18
	cpi	r18, 8
	breq	tm12864_db_4			; switching to chip 2 (right)
	cpi	r18, 16
	brne	tm12864_db_2			; next page and process

	; here means we're done!
	rjmp	tm12864_db_5
tm12864_db_4:
	ldi	r16, 0x02			; chip 2 (right)
	rjmp	tm12864_db_1

tm12864_db_5:
	clr	r16
	rcall	tm12864_set_chip

	pop	XH
	pop	XL
	pop	r18
	pop	r17
	pop	r16
	ret
;}}}


