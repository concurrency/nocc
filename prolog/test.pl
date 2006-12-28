/*
 * this is experimenting..  see if we can use gprolog sensibly to discover
 * parallel-usage and aliasing errors
 */

q :-
	get_fd_labeling(Lab),
	fd_set_vector_max(1024),
	parinputs(LD,Lab),
	write(LD), nl.

parinputs(LD,Lab) :-
	LD=[IA,IB],
	fd_domain(LD,0,20),
	alldifferent(LD).


alldifferent([]).
alldifferent([_]).
alldifferent([I,J|T]) :-
	I #\= J,
	alldifferent([J|T]).

lab(normal,L) :-
	fd_labeling(L).

lab(ff,L):-
	fd_labelingff(L).

get_fd_labeling(Lab) :-
	argument_counter(C),
	get_labeling1(C,Lab).

get_labeling1(1,normal).
	get_labeling1(2,Lab) :-
	argument_value(1,Lab).


