; test_avr37.asm -- testing mul* encoding..

.mcu "atmega2560"

.text
.org 0
	ldi	r17, -20
	ldi	r16, 30
	mulsu	r17, r16

