rem	vim:syntax=rcxbasic
rem	test_b2.bas -- testing BASIC for LEGO RCX ..
rem	Fred Barnes, Apr 2006

rem	initialise brick

set sensor 1 "light"
set sensor 2 "touch"
set sensor 3 "touch"


rem	motors off

set motor A 0
set motor C 0

rem	say how we handle events

on sensor 2 "press" goto bump


rem	program proper

reset:
	set motor A 2
	set motor C 2

loop:
	goto loop


bump:
	set motor A 0
	set motor C 0

	sound "click"
	sleep 50

	set motor A -1
	set motor C -2

	rem	make sure we reverse for a bit
	sleep 200

	set motor A 0
	set motor C 0
	sleep 50

	goto reset


