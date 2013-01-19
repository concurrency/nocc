;
;	atmega328.inc
;	various constants for the Atmel AVR ATMEGA328 (and friends: 48A,48PA,88A,88PA,168A,168PA,328,328P)
;
;	hacked up by Fred Barnes, January 2013  <frmb@kent.ac.uk>
;	definitions from ATMEGA328 datasheet, Atmel Corp.
;

; =================================================================================================
; First block: extended registers, accessible only by ST/STS/STD and LD/LDS/LDD instructions.
; =================================================================================================

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
.equ	ADMUX	=0x7c
.equ	ADCSRB	=0x7b
.equ	ADCSRA	=0x7a
.equ	ADCH	=0x79
.equ	ADCL	=0x78

.equ	TIMSK2	=0x70
.equ	TIMSK1	=0x6f
.equ	TIMSK0	=0x6e

.equ	PCMSK2	=0x6d
.equ	PCMSK1	=0x6c
.equ	PCMSK0	=0x6b
.equ	EICRA	=0x69
.equ	PCICR	=0x68

.equ	OSCAL	=0x66
.equ	PRR	=0x64
.equ	CLKPR	=0x61
.equ	WDTCSR	=0x60

; =================================================================================================
; second chunk, accessible via IN/OUT instructions:  to use ST/LD, add 0x20 to these (0x20 -> 0x5f)
; =================================================================================================

.equ	SREG	=0x3f
.equ	SPH	=0x3e
.equ	SPL	=0x3d
.equ	SPMCSR	=0x37
.equ	MCUCR	=0x35
.equ	MCUSR	=0x34
.equ	SMCR	=0x33
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
.equ	TIFR2	=0x17
.equ	TIFR1	=0x16
.equ	TIFR0	=0x15
.equ	PORTD	=0x0b
.equ	DDRD	=0x0a
.equ	PIND	=0x09
.equ	PORTC	=0x08
.equ	DDRC	=0x07
.equ	PINC	=0x06
.equ	PORTB	=0x05
.equ	DDRB	=0x04
.equ	PINB	=0x03

; =================================================================================================
; other constants
; =================================================================================================

.equ	RAMEND		=0x08ff
.equ	RAMSTART	=0x100

; =================================================================================================
; special registers
; =================================================================================================

.def	XH		=r27
.def	XL		=r26
.def	YH		=r29
.def	YL		=r28
.def	ZH		=r31
.def	ZL		=r30


