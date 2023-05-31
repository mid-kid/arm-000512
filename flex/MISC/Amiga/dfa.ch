Changes for DFA.C by Andreas Scherer, January 19, 1995.

@x l.40
int symfollowset PROTO((int[], int, int, int[]));

@y
int symfollowset PROTO((int[], int, int, int[]));
int snstods PROTO((int[], int, int[], int, int, int *));
@z

@x l.51
void check_for_backing_up( ds, state )
int ds;
int state[];
@y
void check_for_backing_up(
int ds,
int state[])
@z

@x l.101
void check_trailing_context( nfa_states, num_states, accset, nacc )
int *nfa_states, num_states;
int *accset;
register int nacc;
@y
void check_trailing_context(
int *nfa_states, int num_states,
int *accset,
register int nacc)
@z

@x l.247
int *epsclosure( t, ns_addr, accset, nacc_addr, hv_addr )
int *t, *ns_addr, accset[], *nacc_addr, *hv_addr;
@y
int *epsclosure(
int *t, int *ns_addr, int accset[], int *nacc_addr, int *hv_addr)
@z

@x l.401
	int *epsclosure(), snstods(), symlist[CSIZE + 1];
@y
	int symlist[CSIZE + 1];
@z
