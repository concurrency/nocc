;
;	test_avr41.asm -- Moppy replacement code for floppy-drive fun
;	Copyright (C) 2014 Fred Barnes, University of Kent <frmb@kent.ac.uk>
;

.mcu	"atmega2560"
.include "atmega1280.inc"

;{{{  hardware wiring notes (Arduino Mega 2560)
;
; On the 18x2 connector, even numbered pins connect to 'step', odd numbered pins to 'dir'.
; MIDI channel mapping is thus:
;	0  = 22, 23
;	1  = 24, 25
;	     ...
;	15 = 52, 53
;
;}}}

.data
.org	RAMSTART

;{{{  channel states
D_chans:
	.space	32

;}}}

