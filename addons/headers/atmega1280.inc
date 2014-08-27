;
;	atmega1280.inc
;	various constants for the Atmel AVR ATMEGA1280 (and friends: 640,1280,1281,2560,2561)
;
;	hacked up by Fred Barnes, November 2012  <frmb@kent.ac.uk>
;	definitions from ATMEGA1280 datasheet, Atmel Corp.
;

; =================================================================================================
; First block: extended registers, accessible only by ST/STS/STD and LD/LDS/LDD instructions.
; =================================================================================================

.equ	UDR3	=0x136
.equ	UBRR3H	=0x135
.equ	UBRR3L	=0x134
.equ	UCSR3C	=0x132
.equ	UCSR3B	=0x131
.equ	UCSR3A	=0x130

.equ	OCR5CH	=0x12d
.equ	OCR5CL	=0x12c
.equ	OCR5BH	=0x12b
.equ	OCR5BL	=0x12a
.equ	OCR5AH	=0x129
.equ	OCR5AL	=0x128
.equ	ICR5H	=0x127
.equ	ICR5L	=0x126
.equ	TCNT5H	=0x125
.equ	TCNT5L	=0x124
.equ	TCCR5C	=0x122
.equ	TCCR5B	=0x121
.equ	TCCR5A	=0x120

.equ	PORTL	=0x10b
.equ	DDRL	=0x10a
.equ	PINL	=0x109
.equ	PORTK	=0x108
.equ	DDRK	=0x107
.equ	PINK	=0x106
.equ	PORTJ	=0x105
.equ	DDRJ	=0x104
.equ	PINJ	=0x103
.equ	PORTH	=0x102
.equ	DDRH	=0x101
.equ	PINH	=0x100

.equ	UDR2	=0xd6
.equ	UBRR2H	=0xd5
.equ	UBRR2L	=0xd4
.equ	UCSR2C	=0xd2
.equ	UCSR2B	=0xd1
.equ	UCSR2A	=0xd0

.equ	UDR1	=0xce
.equ	UBRR1H	=0xcd
.equ	UBRR1L	=0xcc
.equ	UCSR1C	=0xca
.equ	UCSR1B	=0xc9
.equ	UCSR1A	=0xc8

.equ	UDR0	=0xc6
.equ	UBRR0H	=0xc5
.equ	UBRR0L	=0xc4
.equ	UCSR0C	=0xc2
.equ	UCSR0B	=0xc1
.equ	UCSR0A	=0xc0

.equ	TWAMR	=0xbd
.equ	TWCR	=0xbc
.equ	TWDR	=0xbb
.equ	TWAR	=0xba
.equ	TWSR	=0xb9
.equ	TWBR	=0xb8

.equ	ASSR	=0xb6

.equ	OCR2B	=0xb4
.equ	OCR2A	=0xb3
.equ	TCNT2	=0xb2
.equ	TCCR2B	=0xb1
.equ	TCCR2A	=0xb0

.equ	OCR4CH	=0xad
.equ	OCR4CL	=0xac
.equ	OCR4BH	=0xab
.equ	OCR4BL	=0xaa
.equ	OCR4AH	=0xa9
.equ	OCR4AL	=0xa8
.equ	ICR4H	=0xa7
.equ	ICR4L	=0xa6
.equ	TCNT4H	=0xa5
.equ	TCNT4L	=0xa4
.equ	TCCR4C	=0xa2
.equ	TCCR4B	=0xa1
.equ	TCCR4A	=0xa0

.equ	OCR3CH	=0x9d
.equ	OCR3CL	=0x9c
.equ	OCR3BH	=0x9b
.equ	OCR3BL	=0x9a
.equ	OCR3AH	=0x99
.equ	OCR3AL	=0x98
.equ	ICR3H	=0x97
.equ	ICR3L	=0x96
.equ	TCNT3H	=0x95
.equ	TCNT3L	=0x94
.equ	TCCR3C	=0x92
.equ	TCCR3B	=0x91
.equ	TCCR3A	=0x90

.equ	OCR1CH	=0x8d
.equ	OCR1CL	=0x8c
.equ	OCR1BH	=0x8b
.equ	OCR1BL	=0x8a
.equ	OCR1AH	=0x89
.equ	OCR1AL	=0x88
.equ	ICR1H	=0x87
.equ	ICR1L	=0x86
.equ	TCNT1H	=0x85
.equ	TCNT1L	=0x84
.equ	TCCR1C	=0x82
.equ	TCCR1B	=0x81
.equ	TCCR1A	=0x80

.equ	DIDR1	=0x7f
.equ	DIDR0	=0x7e
.equ	DIDR2	=0x7d
.equ	ADMUX	=0x7c
.equ	ADCSRB	=0x7b
.equ	ADCSRA	=0x7a
.equ	ADCH	=0x79
.equ	ADCL	=0x78
.equ	XMCRB	=0x75
.equ	XMCRA	=0x74

.equ	TIMSK5	=0x73
.equ	TIMSK4	=0x72
.equ	TIMSK3	=0x71
.equ	TIMSK2	=0x70
.equ	TIMSK1	=0x6f
.equ	TIMSK0	=0x6e

.equ	PCMSK2	=0x6d
.equ	PCMSK1	=0x6c
.equ	PCMSK0	=0x6b
.equ	EICRB	=0x6a
.equ	EICRA	=0x69
.equ	PCICR	=0x68
.equ	PRR1	=0x65
.equ	PRR0	=0x64

; =================================================================================================
; second chunk, accessible via IN/OUT instructions:  to use ST/LD, add 0x20 to these (0x20 -> 0x5f)
; =================================================================================================

.equ	SREG	=0x3f
.equ	SPH	=0x3e
.equ	SPL	=0x3d
.equ	EIND	=0x3c
.equ	RAMPZ	=0x3b
.equ	SPMCSR	=0x37
.equ	MCUCR	=0x35
.equ	MCUSR	=0x34
.equ	SMCR	=0x33
.equ	OCDR	=0x31
.equ	ACSR	=0x30
.equ	SPDR	=0x2e
.equ	SPSR	=0x2d
.equ	SPCR	=0x2c
.equ	GPIOR2	=0x2b
.equ	GPIOR1	=0x2a
.equ	OCR0B	=0x28
.equ	OCR0A	=0x27
.equ	TCNT0	=0x26
.equ	TCCR0B	=0x25
.equ	TCCR0A	=0x24
.equ	GTCCR	=0x23
.equ	EEARH	=0x22
.equ	EEARL	=0x21
.equ	EEDR	=0x20

; =================================================================================================
; last chunk, accessible via IN/OUT or SBIS/SBIC instructions:  to use ST/LD, add 0x20 as before
; =================================================================================================

.equ	EECR	=0x1f
.equ	GPIOR0	=0x1e
.equ	EIMSK	=0x1d
.equ	EIFR	=0x1c
.equ	PCIFR	=0x1b
.equ	TIFR5	=0x1a
.equ	TIFR4	=0x19
.equ	TIFR3	=0x18
.equ	TIFR2	=0x17
.equ	TIFR1	=0x16
.equ	TIFR0	=0x15
.equ	PORTG	=0x14
.equ	DDRG	=0x13
.equ	PING	=0x12
.equ	PORTF	=0x11
.equ	DDRF	=0x10
.equ	PINF	=0x0f
.equ	PORTE	=0x0e
.equ	DDRE	=0x0d
.equ	PINE	=0x0c
.equ	PORTD	=0x0b
.equ	DDRD	=0x0a
.equ	PIND	=0x09
.equ	PORTC	=0x08
.equ	DDRC	=0x07
.equ	PINC	=0x06
.equ	PORTB	=0x05
.equ	DDRB	=0x04
.equ	PINB	=0x03
.equ	PORTA	=0x02
.equ	DDRA	=0x01
.equ	PINA	=0x00


; =================================================================================================
; specific bits for things
; =================================================================================================

; SMCR (sleep mode control register)
.equ	SMCR_BIT_SE		=0x01			; sleep enable

.equ	SMCR_BIT_SM_IDLE	=0x00
.equ	SMCR_BIT_SM_ADCNR	=0x02			; ADC noise reduction sleep mode
.equ	SMCR_BIT_SM_PD		=0x04			; power-down
.equ	SMCR_BIT_SM_PS		=0x06			; power-save
.equ	SMCR_BIT_SM_STANDBY	=0x0c			; standby
.equ	SMCR_BIT_SM_ESTANDBY	=0x0e			; extended standby

; PRR0 (power reduction register 0)
.equ	PRR0_BIT_PRTWI		=0x80
.equ	PRR0_BIT_PRTIM2		=0x40
.equ	PRR0_BIT_PRTIM0		=0x20
.equ	PRR0_BIT_PRTIM1		=0x08
.equ	PRR0_BIT_PRSPI		=0x04
.equ	PRR0_BIT_PRUSART0	=0x02
.equ	PRR0_BIT_PRADC		=0x01

; PRR1 (power reduction register 1)
.equ	PRR1_BIT_PRTIM5		=0x20
.equ	PRR1_BIT_PRTIM4		=0x10
.equ	PRR1_BIT_PRTIM3		=0x08
.equ	PRR1_BIT_PRUSART3	=0x04
.equ	PRR1_BIT_PRUSART2	=0x02
.equ	PRR1_BIT_PRUSART1	=0x01

; TCCR0A (timer/counter control register A)
.equ	TCCR0A_BIT_COM0A1	=0x80
.equ	TCCR0A_BIT_COM0A0	=0x40
.equ	TCCR0A_BIT_COM0B1	=0x20
.equ	TCCR0A_BIT_COM0B0	=0x10
.equ	TCCR0A_BIT_WGM01	=0x02
.equ	TCCR0A_BIT_WGM00	=0x01

; TCCR{1,3,4,5}A (timer/counter control register A)
.equ	TCCR1A_BIT_COM1A1	=0x80
.equ	TCCR1A_BIT_COM1A0	=0x40
.equ	TCCR1A_BIT_COM1B1	=0x20
.equ	TCCR1A_BIT_COM1B0	=0x10
.equ	TCCR1A_BIT_COM1C1	=0x08
.equ	TCCR1A_BIT_COM1C0	=0x04
.equ	TCCR1A_BIT_WGM11	=0x02
.equ	TCCR1A_BIT_WGM10	=0x01

.equ	TCCR3A_BIT_COM3A1	=0x80
.equ	TCCR3A_BIT_COM3A0	=0x40
.equ	TCCR3A_BIT_COM3B1	=0x20
.equ	TCCR3A_BIT_COM3B0	=0x10
.equ	TCCR3A_BIT_COM3C1	=0x08
.equ	TCCR3A_BIT_COM3C0	=0x04
.equ	TCCR3A_BIT_WGM31	=0x02
.equ	TCCR3A_BIT_WGM30	=0x01

.equ	TCCR4A_BIT_COM4A1	=0x80
.equ	TCCR4A_BIT_COM4A0	=0x40
.equ	TCCR4A_BIT_COM4B1	=0x20
.equ	TCCR4A_BIT_COM4B0	=0x10
.equ	TCCR4A_BIT_COM4C1	=0x08
.equ	TCCR4A_BIT_COM4C0	=0x04
.equ	TCCR4A_BIT_WGM41	=0x02
.equ	TCCR4A_BIT_WGM40	=0x01

.equ	TCCR5A_BIT_COM5A1	=0x80
.equ	TCCR5A_BIT_COM5A0	=0x40
.equ	TCCR5A_BIT_COM5B1	=0x20
.equ	TCCR5A_BIT_COM5B0	=0x10
.equ	TCCR5A_BIT_COM5C1	=0x08
.equ	TCCR5A_BIT_COM5C0	=0x04
.equ	TCCR5A_BIT_WGM51	=0x02
.equ	TCCR5A_BIT_WGM50	=0x01

; TCCR{1,3,4,5}B (timer/counter control register B)
.equ	TCCR1B_BIT_ICNC1	=0x80
.equ	TCCR1B_BIT_ICES1	=0x40
.equ	TCCR1B_BIT_WGM13	=0x10
.equ	TCCR1B_BIT_WGM12	=0x08
.equ	TCCR1B_BIT_CS12		=0x04
.equ	TCCR1B_BIT_CS11		=0x02
.equ	TCCR1B_BIT_CS10		=0x01

.equ	TCCR3B_BIT_ICNC3	=0x80
.equ	TCCR3B_BIT_ICES3	=0x40
.equ	TCCR3B_BIT_WGM33	=0x10
.equ	TCCR3B_BIT_WGM32	=0x08
.equ	TCCR3B_BIT_CS32		=0x04
.equ	TCCR3B_BIT_CS31		=0x02
.equ	TCCR3B_BIT_CS30		=0x01

.equ	TCCR4B_BIT_ICNC4	=0x80
.equ	TCCR4B_BIT_ICES4	=0x40
.equ	TCCR4B_BIT_WGM43	=0x10
.equ	TCCR4B_BIT_WGM42	=0x08
.equ	TCCR4B_BIT_CS42		=0x04
.equ	TCCR4B_BIT_CS41		=0x02
.equ	TCCR4B_BIT_CS40		=0x01

.equ	TCCR5B_BIT_ICNC5	=0x80
.equ	TCCR5B_BIT_ICES5	=0x40
.equ	TCCR5B_BIT_WGM53	=0x10
.equ	TCCR5B_BIT_WGM52	=0x08
.equ	TCCR5B_BIT_CS52		=0x04
.equ	TCCR5B_BIT_CS51		=0x02
.equ	TCCR5B_BIT_CS50		=0x01

; TCCR{1,3,4,5}C (timer/counter control register C)
.equ	TCCR1C_BIT_FOC1A	=0x80
.equ	TCCR1C_BIT_FOC1B	=0x40
.equ	TCCR1C_BIT_FOC1C	=0x20

.equ	TCCR3C_BIT_FOC3A	=0x80
.equ	TCCR3C_BIT_FOC3B	=0x40
.equ	TCCR3C_BIT_FOC3C	=0x20

.equ	TCCR4C_BIT_FOC4A	=0x80
.equ	TCCR4C_BIT_FOC4B	=0x40
.equ	TCCR4C_BIT_FOC4C	=0x20

.equ	TCCR5C_BIT_FOC5A	=0x80
.equ	TCCR5C_BIT_FOC5B	=0x40
.equ	TCCR5C_BIT_FOC5C	=0x20

; TIMSK{1,3,4,5} (timer/counter interrupt mask register)
.equ	TIMSK1_BIT_ICIE1	=0x20
.equ	TIMSK1_BIT_OCIE1C	=0x08
.equ	TIMSK1_BIT_OCIE1B	=0x04
.equ	TIMSK1_BIT_OCIE1A	=0x02
.equ	TIMSK1_BIT_TOIE1	=0x01

.equ	TIMSK3_BIT_ICIE3	=0x20
.equ	TIMSK3_BIT_OCIE3C	=0x08
.equ	TIMSK3_BIT_OCIE3B	=0x04
.equ	TIMSK3_BIT_OCIE3A	=0x02
.equ	TIMSK3_BIT_TOIE3	=0x01

.equ	TIMSK4_BIT_ICIE4	=0x20
.equ	TIMSK4_BIT_OCIE4C	=0x08
.equ	TIMSK4_BIT_OCIE4B	=0x04
.equ	TIMSK4_BIT_OCIE4A	=0x02
.equ	TIMSK4_BIT_TOIE4	=0x01

.equ	TIMSK5_BIT_ICIE5	=0x20
.equ	TIMSK5_BIT_OCIE5C	=0x08
.equ	TIMSK5_BIT_OCIE5B	=0x04
.equ	TIMSK5_BIT_OCIE5A	=0x02
.equ	TIMSK5_BIT_TOIE5	=0x01

; TIFR{1,3,4,5} (timer/counter interrupt flag register)
.equ	TIFR1_BIT_ICF1		=0x20
.equ	TIFR1_BIT_OCF1C		=0x08
.equ	TIFR1_BIT_OCF1B		=0x04
.equ	TIFR1_BIT_OCF1A		=0x02
.equ	TIFR1_BIT_TOV1		=0x01

.equ	TIFR3_BIT_ICF3		=0x20
.equ	TIFR3_BIT_OCF3C		=0x08
.equ	TIFR3_BIT_OCF3B		=0x04
.equ	TIFR3_BIT_OCF3A		=0x02
.equ	TIFR3_BIT_TOV3		=0x01

.equ	TIFR4_BIT_ICF4		=0x20
.equ	TIFR4_BIT_OCF4C		=0x08
.equ	TIFR4_BIT_OCF4B		=0x04
.equ	TIFR4_BIT_OCF4A		=0x02
.equ	TIFR4_BIT_TOV4		=0x01

.equ	TIFR5_BIT_ICF5		=0x20
.equ	TIFR5_BIT_OCF5C		=0x08
.equ	TIFR5_BIT_OCF5B		=0x04
.equ	TIFR5_BIT_OCF5A		=0x02
.equ	TIFR5_BIT_TOV5		=0x01

; UCSR1A (USART/MSPIM control and status register A)
.equ	UCSR1A_BIT_RXC1		=0x80		; MSPIM
.equ	UCSR1A_BIT_TXC1		=0x40		; MSPIM
.equ	UCSR1A_BIT_UDRE1	=0x20		; MSPIM
.equ	UCSR1A_BIT_FE1		=0x10
.equ	UCSR1A_BIT_DOR1		=0x08
.equ	UCSR1A_BIT_UPE1		=0x04
.equ	UCSR1A_BIT_U2X1		=0x02
.equ	UCSR1A_BIT_MPCM1	=0x01

; UCSR1B (USART/MSPIM control and status register B)
.equ	UCSR1B_BIT_RXCIE1	=0x80		; MSPIM
.equ	UCSR1B_BIT_TXCIE1	=0x40		; MSPIM
.equ	UCSR1B_BIT_UDRIE1	=0x20		; MSPIM
.equ	UCSR1B_BIT_RXEN1	=0x10		; MSPIM
.equ	UCSR1B_BIT_TXEN1	=0x08		; MSPIM
.equ	UCSR1B_BIT_UCSZ12	=0x04
.equ	UCSR1B_BIT_RXB81	=0x02
.equ	UCSR1B_BIT_TXB81	=0x01

; UCSR1C (USART/MSPIM control and status register C)
.equ	UCSR1C_BIT_UMSEL11	=0x80		; MSPIM = 1
.equ	UCSR1C_BIT_UMSEL10	=0x40		; MSPIM = 1
.equ	UCSR1C_BIT_UPM11	=0x20
.equ	UCSR1C_BIT_UPM10	=0x10
.equ	UCSR1C_BIT_USBS1	=0x08
.equ	UCSR1C_BIT_UCSZ11	=0x04		; MSPIM_BIT_UDORD1
.equ	UCSR1C_BIT_UCSZ10	=0x02		; MSPIM_BIT_UCPHA1
.equ	UCSR1C_BIT_UCPOL1	=0x01		; MSPIM_BIT_UCPOL1

; =================================================================================================
; other constants
; =================================================================================================

.equ	RAMEND		=0x21ff
.equ	RAMSTART	=0x200

; =================================================================================================
; special registers
; =================================================================================================

.def	XH		=r27
.def	XL		=r26
.def	YH		=r29
.def	YL		=r28
.def	ZH		=r31
.def	ZL		=r30

; Note: we reserve r0 and r1 for interrupt handlers
.def	INTREG0		=r0
.def	INTREG1		=r1


; make segments start in the right place by default
.text
.org	0
.data
.org	RAMSTART

