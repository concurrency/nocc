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
.equ	OCR3BL	=0x98
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
; other constants
; =================================================================================================

.equ	RAMEND	=0x1fff


