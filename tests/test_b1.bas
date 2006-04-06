REM	test_b1.bas -- testing BASIC for LEGO RCX ..
REM	Fred Barnes, Jan 2006


REM	initialise brick

set sensor 1 "light"
set sensor 2 "touch"
set sensor 3 "touch"

set power A 2
set direction A forward

set motor A -3
set motor B 2

for x = 0 to 10
	wait 10
next x

REM 
REM start:
REM 	goto start

