;
;	interrupt map for ATMEGA1280
;

.org	0
	jmp	reset		; RESET
	jmp	int0vec		; INT0
	jmp	int1vec		; INT1
	jmp	int2vec		; INT2
	jmp	skipirq		; INT3
	jmp	skipirq		; INT4
	jmp	skipirq		; INT5
	jmp	skipirq		; INT6
	jmp	skipirq		; INT7
	jmp	skipirq		; PCINT0
	jmp	skipirq		; PCINT1
	jmp	skipirq		; PCINT2
	jmp	skipirq		; WDT
	jmp	skipirq		; TIMER2-COMPA
	jmp	skipirq		; TIMER2-COMPB
	jmp	skipirq		; TIMER2-OVF

	jmp	skipirq		; TIMER1-CAPT
	jmp	skipirq		; TIMER1-COMPA
	jmp	skipirq		; TIMER1-COMPB
	jmp	skipirq		; TIMER1-COMPC
	jmp	skipirq		; TIMER1-OVF
	jmp	skipirq		; TIMER0-COMPA
	jmp	skipirq		; TIMER0-COMPB
	jmp	skipirq		; TIMER0-OVF
	jmp	skipirq		; SPI/STC
	jmp	skipirq		; USART0-RX
	jmp	skipirq		; USART0-UDRE
	jmp	skipirq		; USART0-TX
	jmp	skipirq		; ANALOG-COMP
	jmp	skipirq		; ADC
	jmp	skipirq		; EE-READY
	jmp	skipirq		; TIMER3-CAPT

	jmp	skipirq		; TIMER3-COMPA
	jmp	skipirq		; TIMER3-COMPB
	jmp	skipirq		; TIMER3-COMPC
	jmp	skipirq		; TIMER3-OVF
	jmp	skipirq		; USART1-RX
	jmp	skipirq		; USART1-UDRE
	jmp	skipirq		; USART1-TX
	jmp	skipirq		; TWI
	jmp	skipirq		; SPM-READY
	jmp	skipirq		; TIMER4-CAPT
	jmp	skipirq		; TIMER4-COMPA
	jmp	skipirq		; TIMER4-COMPB
	jmp	skipirq		; TIMER4-COMPC
	jmp	skipirq		; TIMER4-OVF
	jmp	skipirq		; TIMER5-CAPT
	jmp	skipirq		; TIMER5-COMPA

	jmp	skipirq		; TIMER5-COMPB
	jmp	skipirq		; TIMER5-COMPC
	jmp	skipirq		; TIMER5-OVF
	jmp	skipirq		; USART2-RX
	jmp	skipirq		; USART2-UDRE
	jmp	skipirq		; USART2-TX
	jmp	skipirq		; USART3-RX
	jmp	skipirq		; USART3-UDRE
	jmp	skipirq		; USART3-TX
	jmp	skipirq		; <none>
	jmp	skipirq		; <none>
	jmp	skipirq		; <none>
	jmp	skipirq		; <none>
	jmp	skipirq		; <none>
	jmp	skipirq		; <none>
	jmp	skipirq		; <none>

skipirq:
	reti

