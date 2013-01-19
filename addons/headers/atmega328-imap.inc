;
;	interrupt map for ATMEGA328
;

.org	0
	jmp	VEC_reset			; RESET
	jmp	VEC_int0 ?: skipirq		; INT0
	jmp	VEC_int1 ?: skipirq		; INT1
	jmp	VEC_pcint0 ?: skipirq		; PCINT0
	jmp	VEC_pcint1 ?: skipirq		; PCINT1
	jmp	VEC_pcint2 ?: skipirq		; PCINT2
	jmp	VEC_wdt ?: skipirq		; WDT
	jmp	VEC_timer2compa ?: skipirq	; TIMER2-COMPA
	jmp	VEC_timer2compb ?: skipirq	; TIMER2-COMPB
	jmp	VEC_timer2ovf ?: skipirq	; TIMER2-OVF
	jmp	VEC_timer1capt ?: skipirq	; TIMER1-CAPT
	jmp	VEC_timer1compa ?: skipirq	; TIMER1-COMPA
	jmp	VEC_timer1compb ?: skipirq	; TIMER1-COMPB
	jmp	VEC_timer1ovf ?: skipirq	; TIMER1-OVF
	jmp	VEC_timer0compa ?: skipirq	; TIMER0-COMPA
	jmp	VEC_timer0compb ?: skipirq	; TIMER0-COMPB

	jmp	VEC_timer0ovf ?: skipirq	; TIMER0-OVF
	jmp	VEC_spistc ?: skipirq		; SPI/STC
	jmp	VEC_usart0rx ?: skipirq		; USART0-RX
	jmp	VEC_usart0udre ?: skipirq	; USART0-UDRE
	jmp	VEC_usart0tx ?: skipirq		; USART0-TX
	jmp	VEC_analogcomp ?: skipirq	; ANALOG-COMP
	jmp	VEC_eeready ?: skipirq		; EE-READY
	jmp	VEC_analogcomp ?: skipirq	; ANALOG-COMP
	jmp	VEC_twi ?: skipirq		; TWI
	jmp	VEC_spmready ?: skipirq		; SPM-READY
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>

skipirq:
	reti

