; bug found..

.text
.org 0x400

.L11:
	nop
	nop
	nop
	rjmp	11b
	breq	101b

