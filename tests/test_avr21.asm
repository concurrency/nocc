;
;	test_avr21.asm -- testing LDI on 16-bits
;

.data
.org	0x124
V_foo:
	.space	4

.text
.org	0

reset:
	ldi	r27:r26, V_foo
	jmp	reset
	
