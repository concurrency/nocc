rem	vim:syntax=rcxbasic
rem	test_b1.bas -- testing BASIC for LEGO RCX ..
rem	Fred Barnes, Jan 2006


rem	initialise brick

set sensor 1 "light"
set sensor 2 "touch"
set sensor 3 "touch"

set power A 2
set direction A forward

set motor A -3
set motor B 2

for x = 0 to 10
	sleep 10
	sound "click"
next x

rem 
rem start:
rem 	goto start



