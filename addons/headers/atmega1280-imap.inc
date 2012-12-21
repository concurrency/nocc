;
;	interrupt map for ATMEGA1280
;

.org	0
	jmp	VEC_reset			; RESET
	jmp	VEC_int0 ?: skipirq		; INT0
	jmp	VEC_int1 ?: skipirq		; INT1
	jmp	VEC_int2 ?: skipirq		; INT2
	jmp	VEC_int3 ?: skipirq		; INT3
	jmp	VEC_int4 ?: skipirq		; INT4
	jmp	VEC_int5 ?: skipirq		; INT5
	jmp	VEC_int6 ?: skipirq		; INT6
	jmp	VEC_int7 ?: skipirq		; INT7
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
	jmp	VEC_timer1compc ?: skipirq	; TIMER1-COMPC
	jmp	VEC_timer1ovf ?: skipirq	; TIMER1-OVF
	jmp	VEC_timer0compa ?: skipirq	; TIMER0-COMPA
	jmp	VEC_timer0compb ?: skipirq	; TIMER0-COMPB
	jmp	VEC_timer0ovf ?: skipirq	; TIMER0-OVF
	jmp	VEC_spistc ?: skipirq		; SPI/STC
	jmp	VEC_usart0rx ?: skipirq		; USART0-RX
	jmp	VEC_usart0udre ?: skipirq	; USART0-UDRE
	jmp	VEC_usart0tx ?: skipirq		; USART0-TX
	jmp	VEC_analogcomp ?: skipirq	; ANALOG-COMP
	jmp	VEC_adc ?: skipirq		; ADC
	jmp	VEC_eeready ?: skipirq		; EE-READY
	jmp	VEC_timer3capt ?: skipirq	; TIMER3-CAPT

	jmp	VEC_timer3compa ?: skipirq	; TIMER3-COMPA
	jmp	VEC_timer3compb ?: skipirq	; TIMER3-COMPB
	jmp	VEC_timer3compc ?: skipirq	; TIMER3-COMPC
	jmp	VEC_timer3ovf ?: skipirq	; TIMER3-OVF
	jmp	VEC_usart1rx ?: skipirq		; USART1-RX
	jmp	VEC_usart1udre ?: skipirq	; USART1-UDRE
	jmp	VEC_usart1tx ?: skipirq		; USART1-TX
	jmp	VEC_twi ?: skipirq		; TWI
	jmp	VEC_spmready ?: skipirq		; SPM-READY
	jmp	VEC_timer4capt ?: skipirq	; TIMER4-CAPT
	jmp	VEC_timer4compa ?: skipirq	; TIMER4-COMPA
	jmp	VEC_timer4compb ?: skipirq	; TIMER4-COMPB
	jmp	VEC_timer4compc ?: skipirq	; TIMER4-COMPC
	jmp	VEC_timer4ovf ?: skipirq	; TIMER4-OVF
	jmp	VEC_timer5capt ?: skipirq	; TIMER5-CAPT
	jmp	VEC_timer5compa ?: skipirq	; TIMER5-COMPA

	jmp	VEC_timer5compb ?: skipirq	; TIMER5-COMPB
	jmp	VEC_timer5compc ?: skipirq	; TIMER5-COMPC
	jmp	VEC_timer5ovf ?: skipirq	; TIMER5-OVF
	jmp	VEC_usart2rx ?: skipirq		; USART2-RX
	jmp	VEC_usart2udre ?: skipirq	; USART2-UDRE
	jmp	VEC_usart2tx ?: skipirq		; USART2-TX
	jmp	VEC_usart3rx ?: skipirq		; USART3-RX
	jmp	VEC_usart3udre ?: skipirq	; USART3-UDRE
	jmp	VEC_usart3tx ?: skipirq		; USART3-TX
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>
	jmp	skipirq				; <none>

skipirq:
	reti

