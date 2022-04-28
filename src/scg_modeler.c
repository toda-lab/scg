#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "scg_modeler.h"
#include "scg_assert.h"

static void init_idmgr (idmgr_t *p,
        const int pid[], int len,
        int *nissued, int total,
        void (*encoder) (const int *, int *, int, const param_t *, const idmgr_t *),
        void (*decoder) (int,         int *, int, const param_t *, const idmgr_t *),
        bool (*accepted)(const int *, const data_t *));

static void  delete_idmgr (idmgr_t *p);


void read_input (FILE *in, data_t *data)
{
	if (data->nclues > 0) {
		fprintf(stderr, "ERROR: Clue cells are already read.\n");
		exit(EXIT_FAILURE);
	}

	cell_t *cs = data->cs;
	assert(cs != NULL);

	const int size = data->size;

	int i, j;
	int nclues; // number of clue cells with duplicates
	i = j = nclues = 0;

	while(fscanf(in, "%d %d", &i,&j) != EOF) {

		if (0 < i && i <= size 
		 && 0 < j && j <= size 
		 && nclues < (size * size)) {

			cs[nclues].I = i - 1; 
			cs[nclues].J = j - 1;
			nclues++;

		} else if (false == (nclues < (size * size))) {

			fprintf(stderr, "ERROR: Too many clue cells are given.\n");
			exit(EXIT_FAILURE);

		} else {

			fprintf(stderr, "ERROR: Invalid number is found at line %d.\n", nclues + 1);
			exit(EXIT_FAILURE);

		}

	}

	// sort
	for (int m = nclues; m > 0; m--) {
		for (int k = 1; k < m; k++) {
			if (larger_cell(cs[k], cs[k - 1])) {
				cell_t tmp = cs[k - 1];
				cs[k - 1]    = cs[k];
				cs[k]        = tmp;
			}
		}
	}

#ifndef NDEBUG
// check
for (int k = 1; k < nclues; k++) {
	assert(larger_cell(cs[k - 1], cs[k]) || equal_cell(cs[k - 1], cs[k]));
}
#endif

	if (nclues == 0) return;

	// count the number of clues without duplicates
	int count = 1;
	for (int k = 1; k < nclues; k++) {
		if (false == equal_cell(cs[count - 1], cs[k])) {
			cs[count++] = cs[k];
		}
	}

	data->nclues = count; 
}

void init_data (data_t *data, int rank, int bound)
{
	data->rank    = rank;
	data->size    = rank * rank;
	data->bound   = bound;
	data->nissued = 0;

	const int size = data->size;

	data->cs = (cell_t*)malloc(sizeof(cell_t) * size * size);
	if (data->cs == NULL) {
		fprintf(stderr, "ERROR: Memory allocation failed.\n");
		exit(EXIT_FAILURE);
	}

	data->nclues = 0;

	param_t *p = (param_t *)malloc(sizeof(param_t));
	if (p == NULL) {
		fprintf(stderr, "ERROR: Memory allocation failed.\n");
		exit(EXIT_FAILURE);
	
	}
	data->p = p;

	init_param(p);
	
	data->pid_I = add_param(0, size - 1, tag_I, p);
	data->pid_J = add_param(0, size - 1, tag_J, p);
	data->pid_N = add_param(1, size,     tag_N, p); // Do not include 0!
	data->pid_K = add_param(0, bound,    tag_K, p);

	assert(data->pid_I >= 0);
	assert(data->pid_J >= 0);
	assert(data->pid_N >= 0);
	assert(data->pid_K >= 0);

	data->nstrats = 0;
}

void delete_data (data_t *data)
{
	const int len = data->nstrats;

	for (int pos = 0; pos < len; pos++) {

		assert(data->strat[pos].idmgr != NULL);

		delete_idmgr(data->strat[pos].idmgr);
		free(data->strat[pos].idmgr);
		data->strat[pos].idmgr = NULL;
	}

	assert(data->cs != NULL);
	assert(data->p  != NULL);

	free(data->cs);
	data->cs = NULL;

	free(data->p);
	data->p = NULL;
}

void add_strategy (data_t *data,
			stag_t tag,
			int  nvars,
			const int  *pid, int len,
			void (*encoder) (const int *, int *, int, const param_t *, const idmgr_t *),
			void (*decoder) (int,         int *, int, const param_t *, const idmgr_t *),
			bool (*accepted)(const int *, const data_t *),
			void (*fprint_literals_for_x) (FILE *, const data_t *),
			void (*fprint_literals_for_y) (FILE *, const data_t *),
			void (*fprint_cons_for_z    ) (FILE *, data_t *))
{
	assert(data != NULL);
	assert(pid  != NULL);
	assert(0 < len);
	assert(0 < nvars);

	idmgr_t *mgr = (idmgr_t*)malloc(sizeof(idmgr_t));
	if (mgr == NULL) {
		fprintf(stderr, "ERROR: Memory allocation failed.\n");
		exit(EXIT_FAILURE);
	}

	init_idmgr(mgr, pid, len, &(data->nissued), nvars, encoder, decoder, accepted);

	strat_t *strat = data->strat;
	const int num = data->nstrats;

	for (int pos = 0; pos < num; pos++){
		if (data->strat[pos].tag == tag) {
			fprintf(stderr, "ERROR: A strategy of the specified tag already exists.\n");
			exit(EXIT_FAILURE);
		}
	}

	assert(num < MAX_STRATS);

	strat[num].tag   = tag;
	strat[num].idmgr = mgr;
	strat[num].fprint_literals_for_x = fprint_literals_for_x;
	strat[num].fprint_literals_for_y = fprint_literals_for_y;
	strat[num].fprint_cons_for_z     = fprint_cons_for_z;
	
	data->nstrats++;

	assert_encoder_decoder(mgr, data);
}

// Reject if the current cell is a clue cell 
//           and the current step is not an initial step,
// accept otherwise.
bool accepted_general (const int *buf, const idmgr_t *mgr, const data_t *data)
{
	assert(mgr != NULL);

	const param_t *p = data->p;
	const int rank = data->rank;

	const int len = mgr->len;

	const int pid_I = data->pid_I;
	const int pid_J = data->pid_J;
	const int pid_N = data->pid_N;
	const int pid_K = data->pid_K;

	const int pos_I = pos_of_pid(pid_I, mgr);
	const int pos_J = pos_of_pid(pid_J, mgr);
	const int pos_K = pos_of_pid(pid_K, mgr);

	// do not reject if one of I, J, and K is not managed by mgr.
	if (pos_I < 0 || pos_J < 0 || pos_K < 0) {
		return true;
	}

	cell_t q = cell_at(buf[pos_I], buf[pos_J], rank);

	if (p->min[pid_K] < buf[pos_K]) {
		return false == is_clue_cell(q, data->cs, data->nclues);
	}

	return true;
}

// Get the idmgr of a specified strategy.
const idmgr_t *get_idmgr(stag_t tag, const data_t *data)
{
	const int len = data->nstrats;
	for (int pos = 0; pos < len; pos++) {
		if (data->strat[pos].tag == tag) return data->strat[pos].idmgr;
	}

	return NULL;
}

static void init_idmgr (idmgr_t *p, 
	const int pid[], int len, int *nissued, int total, 
	void (*encoder) (const int *, int *, int, const param_t *, const idmgr_t *), 
	void (*decoder) (int,         int *, int, const param_t *, const idmgr_t *),
	bool (*accepted)(const int*,   const data_t *))
{
	assert(p != NULL);
	assert(0 <= len);

	if (len > MAX_PARAMS) {
		fprintf(stderr, "ERROR: Cannot initialize id manager because of too many parameters.\n");
		exit(EXIT_FAILURE);
	}

	p->pid = (int*)malloc(sizeof(int) * len);
	if (p->pid == NULL) {
		fprintf(stderr, "ERROR: Memory allocation failed.\n");
		exit(EXIT_FAILURE);
	}

	for (int pos = 0; pos < len; pos++) {
		p->pid[pos] = pid[pos];	
	}
	p->len = len;

	p->first    = *nissued;
	p->total    = total;
	p->encoder  = encoder;
	p->decoder  = decoder;
	p->accepted = accepted;

	*nissued   = *nissued + total; // issue ids in a lump.
}

static void delete_idmgr (idmgr_t *p)
{
	assert(p->pid != NULL);

	free(p->pid);
	p->pid = NULL;
}

// Copy the current values of parameters (managed by mgr) to the array "to" ,
// where the "to" has length at least mgr->len.
void read_cur (const param_t *p, int *to, const idmgr_t *mgr)
{
	const int len = mgr->len;

	for (int pos = 0; pos < len; pos++) {
		const int pid = mgr->pid[pos];
		assert(p->act[pid] == true);

		to[pos] = p->cur[pid];
	}
}

// [Example]
//
// int *buf = (int*)malloc(sizeof(int) * (mgr->len));
// read_cur(p, buf, mgr);
// const int pos = pos_of_pid(pid_I, mgr);
// if (pos < 0)
//	printf("The parameter of pid %d is not managed by the specified id manager.\n", pid_I);
// else
//	printf("The current value of the parameter of pid %d is %d\n", pid_I, buf[pos]);
//
int pos_of_pid (int pid, const idmgr_t *mgr)
{
	const int len = mgr->len;

	for (int pos = 0; pos < len; pos++) {

		if (pid == mgr->pid[pos]) {
			return pos;
		}
	}

	return -1;
}

// Encode the combination of parameter values into a single integer,
// where value[pos] must be set the value of the parameter of id  mgr->pid[pos] in advance.
void default_encoder (const int *value, int *index, int rank, const param_t *p, const idmgr_t *mgr)
{
	assert(value != NULL);
	assert(index != NULL);

	int diff = 0;

	const int len = mgr->len;
	for (int pos = 0; pos < len; pos++) {

		const int pid = mgr->pid[pos];

		diff = diff * (p->max[pid] - p->min[pid] + 1);
		diff = diff + (value[pos]  - p->min[pid]);
	}

	*index = diff + (mgr->first);
}

void default_decoder (int index, int *value, int rank, const param_t *p, const idmgr_t *mgr)
{
	assert(value != NULL);

	int diff = index - (mgr->first);

	const int len = mgr->len;

	for (int pos = len - 1; pos >= 0; pos--) {

		const int pid = mgr->pid[pos];

		value[pos] = (diff % (p->max[pid] - p->min[pid] + 1)) + p->min[pid];
		diff       =  diff / (p->max[pid] - p->min[pid] + 1);
		
	}
}

// count the combinations of parameter values for I, J, N and K.
int count_combinations_of_IJNK_values (const param_t *p)
{
	const int pid_I = get_param(tag_I, p);
	const int pid_J = get_param(tag_J, p);
	const int pid_N = get_param(tag_N, p);
	const int pid_K = get_param(tag_K, p);

	assert(0 <= pid_I);	
	assert(0 <= pid_J);	
	assert(0 <= pid_N);	
	assert(0 <= pid_K);	

	int num = 1;
	const int len = p->npars;

	for (int pos = 0; pos < len; pos++) {
		if (pos == pid_I || pos == pid_J 
                 || pos == pid_N || pos == pid_K) {

			num = num * (p->max[pos] - p->min[pos] + 1);
		}

	}

	return num;
}

void init_param (param_t *p)
{
	p->end   = true;
	p->npars = 0;
}

// register a new parameter whose values range from min to max.
int add_param (int min, int max, stag_t tag, param_t *p)
{
	assert(p   != NULL);

	if (min < 0 || min > max) {
		fprintf(stderr, "ERROR: Invalid parameter range is specified.\n");
		exit(EXIT_FAILURE);
	}

	const int pid = p->npars;
	if (pid >= MAX_PARAMS) {
		fprintf(stderr, "ERROR: Cannot add parameter further.\n");
		exit(EXIT_FAILURE);
	}

	// check uniqueness
	for (int pos = 0; pos < pid; pos++) {
		if (p->tag[pos] == tag)	{
			fprintf(stderr, "ERROR: A parameter of the specified tag already exists.\n");
			exit(EXIT_FAILURE);
		}
	}

	p->cur[pid] = min;
	p->min[pid] = min;
	p->max[pid] = max;
	p->act[pid] = false;
	p->tag[pid] = tag;

	p->end = true;
	p->npars++;

	return pid;
}

// reset only the cur and end fields.
void reset_param (param_t *p)
{
	for (int pid = 0; pid < p->npars; pid++) {
		p->cur[pid] = p->min[pid];
	}

	p->end = false;
}

// If parameter cannot be incremented, set the end field.
void next_param (param_t *p)
{
	for (int pid = 0; pid < p->npars; pid++) {

		if (p->act[pid] == false) {
			continue;
		} else if (p->cur[pid] == p->max[pid]) {
			p->cur[pid] = p->min[pid];
			continue;
		} else {
			p->cur[pid]++;
			return;
		}

	}

	p->end = true; // no next parameter
}

// return -1 if there is no parameter with a specified tag.
int get_param (stag_t tag, const param_t *p)
{
	for (int pid = 0; pid < p->npars; pid++) {
		if (p->tag[pid] == tag) return pid;
	}

	return -1;
}

void make_all_inactive (param_t *p)
{
	for (int pid = 0; pid < p->npars; pid++) {
		p->act[pid] = false;
	}
}

void make_IJNK_active (param_t *p)
{
	const int pid_I = get_param(tag_I, p);
	const int pid_J = get_param(tag_J, p);
	const int pid_N = get_param(tag_N, p);
	const int pid_K = get_param(tag_K, p);

	assert(0 <= pid_I);	
	assert(0 <= pid_J);	
	assert(0 <= pid_N);	
	assert(0 <= pid_K);	

	p->act[pid_I] = true;
	p->act[pid_J] = true;
	p->act[pid_N] = true;
	p->act[pid_K] = true;
}

void make_IJ_active (param_t *p)
{
	const int pid_I = get_param(tag_I, p);
	const int pid_J = get_param(tag_J, p);

	assert(0 <= pid_I);	
	assert(0 <= pid_J);	

	p->act[pid_I] = true;
	p->act[pid_J] = true;
}

void make_IJN_active (param_t *p)
{
	const int pid_I = get_param(tag_I, p);
	const int pid_J = get_param(tag_J, p);
	const int pid_N = get_param(tag_N, p);

	assert(0 <= pid_I);	
	assert(0 <= pid_J);	
	assert(0 <= pid_N);	

	p->act[pid_I] = true;
	p->act[pid_J] = true;
	p->act[pid_N] = true;
}

void make_IJK_active (param_t *p)
{
	const int pid_I = get_param(tag_I, p);
	const int pid_J = get_param(tag_J, p);
	const int pid_K = get_param(tag_K, p);

	assert(0 <= pid_I);	
	assert(0 <= pid_J);	
	assert(0 <= pid_K);	

	p->act[pid_I] = true;
	p->act[pid_J] = true;
	p->act[pid_K] = true;
}

// make all the parameters associated by mgr active.
void make_assoc_active(param_t *p, const idmgr_t *mgr)
{
	const int len = mgr->len;
	for (int pos = 0; pos < len; pos++) {
		const int pid = mgr->pid[pos];
		p->act[pid] = true;
	}
}

bool equal_cell (cell_t left, cell_t right) 
{
	return (left.I == right.I) && (left.J == right.J);
}

// whether the cell on the right is strictly larger than the cell on the left.
bool larger_cell (cell_t left, cell_t right) 
{
	return (left.I < right.I) || ((left.I == right.I) && (left.J < right.J ));
}

bool is_clue_cell (cell_t q, const cell_t *cs, int n) 
{
#ifndef NDEBUG
for (int pos = 1; pos < n; pos++) {
	assert(equal_cell(cs[pos-1], cs[pos])
		|| larger_cell(cs[pos-1], cs[pos]));
}
#endif

	for (int pos = 0; pos < n; pos++) {
		if (equal_cell(q, cs[pos])) {
			return true;
		} else if (larger_cell(q, cs[pos])) {
			return false;
		}
	}

	return false;
}

// Get the cell (i,j)
cell_t cell_at (int i, int j, int r)
{
	cell_t q;
	q.I = i;
	q.J = j;

	return q;	
}

// Get the m-th cell in the n-th block.
cell_t cell_in_block (int m, int n, int r)
{
	cell_t q;
	q.I = (n / r) * r + (m / r);
	q.J = (n % r) * r + (m % r);

	return q;
}

// Get (the index of ) the block to which the cell q belongs.
int ownerblock (cell_t q, int r)
{
	return (q.I / r) * r + (q.J / r);
}


// Answer whether the cell q is in the n-th block.
bool is_in_block (cell_t q, int n, int r)
{
	const int owener = ownerblock(q, r);

	return owener == n;
}

// For 1 <= n <= size,
// x_i_j_k = n <---> n is placed at (i,j) in step k
// x_i_j_k = 0 <---> no number is placed at (i,j) in step k
//
void fprint_decl_for_x (FILE *out, data_t *data) 
{
	fprintf(out, ";\n");
	fprintf(out, "; X Variables\n");

	param_t *p = data->p;

	const int pid_I = data->pid_I;
	const int pid_J = data->pid_J;
	const int pid_N = data->pid_N;
	const int pid_K = data->pid_K;

	make_all_inactive(p);
	make_IJK_active(p);

	const int maxnum = p->max[pid_N];
	assert(p->min[pid_N] == 1);

	for(reset_param(p); p->end == false; next_param(p)) {
		fprintf(out, "(int x_%d_%d_%d 0 %d)\n", 
			p->cur[pid_I], 
			p->cur[pid_J], 
			p->cur[pid_K], 
			maxnum);
	}
}

// y_i_j_n_k is true <---> n is a candidate at (i,j) in step k.
//
void fprint_decl_for_y (FILE *out, data_t *data) 
{
	fprintf(out, ";\n");
	fprintf(out, "; Y Variables\n");

	param_t *p = data->p;

	const int pid_I = data->pid_I;
	const int pid_J = data->pid_J;
	const int pid_N = data->pid_N;
	const int pid_K = data->pid_K;

	make_all_inactive(p);
	make_IJNK_active(p);

	for(reset_param(p); p->end == false; next_param(p)) {
		fprintf(out, "(bool y_%d_%d_%d_%d)\n", 
			p->cur[pid_I], 
			p->cur[pid_J], 
			p->cur[pid_N],
			p->cur[pid_K]);
	}
}

// z_m is true <---> some strategy or sudoku rule is applicable.
//
void fprint_decl_for_z (FILE *out, data_t *data) 
{
	fprintf(out, ";\n");
	fprintf(out, "; Z Variables\n");

	const param_t *p = data->p;

	const int len  = data->nstrats;
	const int rank = data->rank; 

	for (int pos = 0; pos < len; pos++) {

		idmgr_t *mgr = data->strat[pos].idmgr;
		int *buf = (int*)malloc(sizeof(int) * (mgr->len));

		const int first = mgr->first;
		const int end   = mgr->first + mgr->total;

		for (int index = first; index < end; index++) {
			mgr->decoder(index, buf, rank, p, mgr);

			if (true == mgr->accepted(buf, data)) {
				fprintf(out, "(bool z_%d)\n", index);
			}
		}

		free(buf);
	}
}

// For any clue cell (i,j),
// x_i_j_0 != 0 <---> some number must be placed at (i,j) in step 0.
//
// For any non-clue cell (i,j),
// x_i_j_0 = 0  <---> no number is placed at (i,j) in step 0.
//
void fprint_cons_for_init (FILE *out, data_t *data) 
{
	fprintf(out, ";\n");
	fprintf(out, "; Constraints for Initial States\n");

	param_t *p = data->p;

	const int rank  = data->rank;
	const int pid_I = data->pid_I;
	const int pid_J = data->pid_J;
	//const int pid_N = data->pid_N;

	make_all_inactive(p);
	make_IJ_active(p);

	for(reset_param(p); p->end == false; next_param(p)) {
		cell_t q = cell_at(
				p->cur[pid_I],
				p->cur[pid_J],
				rank);

		if (is_clue_cell(q, data->cs, data->nclues)) {
			fprintf(out, "(!= x_%d_%d_0 0)\n", q.I, q.J);	
		} else {
			fprintf(out, "(=  x_%d_%d_0 0)\n", q.I, q.J);	
		}
	}

	/*make_all_inactive(p);
	make_IJN_active(p);
	for(reset_param(p); p->end == false; next_param(p)) {
		cell_t q = cell_at(
				p->cur[pid_I],
				p->cur[pid_J],
				rank);
		if (is_clue_cell(q, data->cs, data->nclues)) {
			fprintf(out, "(imp (= x_%d_%d_0 %d) y_%d_%d_%d_0)\n",
					q.I, q.J, p->cur[pid_N],
					q.I, q.J, p->cur[pid_N]);
		}
	}*/

}

// For 1 <= n <= size,
// x_i_j_k = n <---> some strategy is applicable in step k-1 
//		     or x_i_j_{k-1} = n.
//
// y_i_j_n_k is false <---> some strategy is applicable in step k-1 
//		   	    or sudoku rule is applicable in step k
//			    or y_i_j_n_{k-1} is false.
//
void fprint_cons_for_trans (FILE *out, data_t *data) 
{
	fprintf(out, ";\n");
	fprintf(out, "; Constraints for State Transitions\n");

	param_t *p = data->p;

	const int pid_I = data->pid_I;
	const int pid_J = data->pid_J;
	const int pid_N = data->pid_N;
	const int pid_K = data->pid_K;

	const int nstrats = data->nstrats; 

	make_all_inactive(p);
	make_IJNK_active(p);

	for(reset_param(p); p->end == false; next_param(p)) {

		// no strategy or rule for step 0
		if (p->min[pid_K] == p->cur[pid_K]) continue; 

		const cell_t q = cell_at(
					p->cur[pid_I],
					p->cur[pid_J],
					data->rank);

		if (is_clue_cell(q, data->cs, data->nclues)) {
			fprintf(out, "(iff ");
		  	  fprint_literal(out, 'x', 
				p->cur[pid_I], 
				p->cur[pid_J], 
				p->cur[pid_N],
				p->cur[pid_K]);

		  	  fprint_literal(out, 'x', 
				p->cur[pid_I], 
				p->cur[pid_J], 
				p->cur[pid_N],
				p->cur[pid_K]-1);

			fprintf(out, ")\n");

			continue;
		}


		fprintf(out, "(iff ");

		    fprint_literal(out, 'x', 
			p->cur[pid_I], 
			p->cur[pid_J], 
			p->cur[pid_N],
			p->cur[pid_K]);

		fprintf(out, " (or ");

		    for (int pos = 0; pos < nstrats; pos++) {
		    	if (data->strat[pos].fprint_literals_for_x != NULL) {
				data->strat[pos].fprint_literals_for_x(out, data);
			}
		    }

		    if (p->cur[pid_K] > p->min[pid_K]) {
		    	fprint_literal(out, 'x', 
				p->cur[pid_I], 
				p->cur[pid_J], 
				p->cur[pid_N],
				p->cur[pid_K] - 1);
		    }

		fprintf(out, ")");
		fprintf(out, ")\n");
	}

	make_all_inactive(p);
	make_IJNK_active(p);

	for(reset_param(p); p->end == false; next_param(p)) {

		const cell_t q = cell_at(
					p->cur[pid_I],
					p->cur[pid_J],
					data->rank);

		if (p->min[pid_K] < p->cur[pid_K] 
		&&  is_clue_cell(q, data->cs, data->nclues)) {

			fprintf(out, "(iff ");
			  fprint_literal(out, 'y', 
				p->cur[pid_I], 
				p->cur[pid_J], 
				p->cur[pid_N],
				p->cur[pid_K]);

			  fprint_literal(out, 'y', 
				p->cur[pid_I], 
				p->cur[pid_J], 
				p->cur[pid_N],
				p->cur[pid_K] - 1);
			fprintf(out, ")\n");

			continue;
		}

		fprintf(out, "(iff ");

		  fprint_literal(out, 'y', 
			p->cur[pid_I], 
			p->cur[pid_J], 
			p->cur[pid_N],
			p->cur[pid_K]);

		  fprintf(out, " (or ");

		    for (int pos = 0; pos < nstrats; pos++) {
		    	if (data->strat[pos].fprint_literals_for_y != NULL) {
				data->strat[pos].fprint_literals_for_y(out, data);
			}
						
		    }

		    if (p->cur[pid_K] > p->min[pid_K]) {
			fprint_literal (out, 'y', 
				p->cur[pid_I], 
				p->cur[pid_J], 
				p->cur[pid_N],
				p->cur[pid_K]-1);
		    }

		  fprintf(out, ")");
		fprintf(out, ")\n");
	}

}

// The condition on the left means that the grid does not change between k-1 and k.
// The condition on the right means that all cells are completed in step k.
//
void fprint_cons_for_final (FILE *out, data_t *data) 
{
	fprintf(out, ";\n");
	fprintf(out, "; Constraints for Final States\n");

	param_t *p = data->p;

	const int pid_I = data->pid_I;
	const int pid_J = data->pid_J;
	const int pid_N = data->pid_N;
	const int pid_K = data->pid_K;

	for (int k = p->min[pid_K] + 1; k <= p->max[pid_K]; k++) {
		// Note: the initial step must be skipped

		make_all_inactive(p);
		make_IJN_active(p);

		fprintf(out, "(imp ");

		fprintf(out, "(and ");
		for(reset_param(p); p->end == false; next_param(p)) {
			cell_t q = cell_at(
					p->cur[pid_I],
					p->cur[pid_J],
					data->rank);
			if (false == is_clue_cell(q, data->cs, data->nclues)) {
				fprintf(out, " (iff y_%d_%d_%d_%d y_%d_%d_%d_%d) ", 
					q.I, q.J, p->cur[pid_N], k - 1, 
					q.I, q.J, p->cur[pid_N], k);   
			}
		}
		fprintf(out, ") ");

		make_all_inactive(p);
		make_IJ_active(p);

		fprintf(out, "(and ");
		for(reset_param(p); p->end == false; next_param(p)) {
			cell_t q = cell_at(
					p->cur[pid_I],
					p->cur[pid_J],
					data->rank);

			if (false == is_clue_cell(q, data->cs, data->nclues)) {
				fprintf(out, " (!= x_%d_%d_%d 0) ", q.I, q.J, k);
			}
		}
		fprintf(out, ")");

		fprintf(out, ")\n");

	}

}

void fprint_cons_for_strat (FILE *out, data_t *data)
{
	const int len = data->nstrats;

	for (int pos = 0; pos < len; pos++) {

		if (data->strat[pos].fprint_cons_for_z != NULL) {
			data->strat[pos].fprint_cons_for_z(out, data);
		}

	}
}

// Let group_A be a row index, and let group_B be a block index.
// This function determines whether A and B have common cells.
static bool have_common_cell_ARBB (int group_A, int group_B, int rank)
{
	cell_t q = cell_in_block(0, group_B, rank);
	return q.I <= group_A && group_A < q.I + rank;
}

// Let group_A be a column index, and let group_B be a block index.
// This function determines whether A and B have common cells.
static bool have_common_cell_ACBB (int group_A, int group_B, int rank)
{
	cell_t q = cell_in_block(0, group_B, rank);
	return q.J <= group_A && group_A < q.J + rank;
}

// Let group_A be a block index, and let group_B be a row index.
// This function determines whether A and B have common cells.
static bool have_common_cell_ABBR (int group_A, int group_B, int rank)
{
	return have_common_cell_ARBB(group_B, group_A, rank);
}

// Let group_A be a block index, and let group_B be a column index.
// This function determines whether A and B have common cells.
static bool have_common_cell_ABBC (int group_A, int group_B, int rank)
{
	return have_common_cell_ACBB(group_B, group_A, rank);
}

// Let group_A and group_B be group indices, where a group is a row, a column, or a block.
// This function determines whether A and B have common cells.
bool have_common_cell(int group_A, int group_B, int type_AB, int rank)
{
	switch (type_AB) {
		case TYPE_ARBB:
			return have_common_cell_ARBB(group_A, group_B, rank);

		case TYPE_ACBB:
			return have_common_cell_ACBB(group_A, group_B, rank);

		case TYPE_ABBR:
			return have_common_cell_ABBR(group_A, group_B, rank);

		case TYPE_ABBC:
			return have_common_cell_ABBC(group_A, group_B, rank);

		default:
			assert(0);
			exit(EXIT_FAILURE);
	}

	return false;
}

// Print out all literals while parameters running over all possible cases.
// The function arg->test() determines whether the current case is accepted or not.
// [example]
// test_not_equal_number() determines whether the current number is not equal to a particular number.
// test_not_equal_cell()   determines whether the current cell is not equal to a particular cell.
// test_not_in_block()     determines whether the current cell is not in a particular block.
//
// * The particular number, cell, and block must be set by set_testarg() in advance.
// * In accordance with this, the corresponding test function must be set by set_runarg() in advance.
//
void fprint_literals_running_over (FILE *out, const param_t *p, const runarg_t *arg)
{
	const int rank = arg->testarg->rank;

	const int pid_I = get_param(tag_I, p);
	const int pid_J = get_param(tag_J, p);
	const int pid_N = get_param(tag_N, p);
	const int pid_K = get_param(tag_K, p);

	assert(pid_I >= 0);
	assert(pid_J >= 0);
	assert(pid_N >= 0);
	assert(pid_K >= 0);

	cur_t cur;

	switch (arg->type) {
		case 'v':
			assert(p->min[pid_I] <= arg->fixed_I && arg->fixed_I <= p->max[pid_I]);
			assert(p->min[pid_J] <= arg->fixed_J && arg->fixed_J <= p->max[pid_J]);
			assert(p->min[pid_K] <= arg->fixed_K && arg->fixed_K <= p->max[pid_K]);

			// run over all numbers in the cell of index (arg->fixed_I, arg->fixed_J).
			for(int n = p->min[pid_N]; n <= p->max[pid_N]; n++) {

				cur.N = n;
				cur.I = arg->fixed_I;
				cur.J = arg->fixed_J;

				if (arg->test(&cur, arg->testarg) == true) {
					fprint_literal(out,
						arg->symb,
						arg->fixed_I, 
						arg->fixed_J, 
						cur.N,
						arg->fixed_K);
				}

			}

			break;

		case 'r':
			assert(p->min[pid_I] <= arg->fixed_I && arg->fixed_I <= p->max[pid_I]);
			assert(p->min[pid_N] <= arg->fixed_N && arg->fixed_N <= p->max[pid_N]);
			assert(p->min[pid_K] <= arg->fixed_K && arg->fixed_K <= p->max[pid_K]);

			// run over all cells in the row of index arg->fixed_I.
			for(int j = p->min[pid_J]; j <= p->max[pid_J]; j++) {

				cur.I = arg->fixed_I;
				cur.J = j;
				cur.N = arg->fixed_N;

				if (arg->test(&cur, arg->testarg) == true) {
					fprint_literal(out,
						arg->symb,
						arg->fixed_I, 
						cur.J, 
						arg->fixed_N,
						arg->fixed_K);

				}

			}

			break;

		case 'c':
			assert(p->min[pid_J] <= arg->fixed_J && arg->fixed_J <= p->max[pid_J]);
			assert(p->min[pid_N] <= arg->fixed_N && arg->fixed_N <= p->max[pid_N]);
			assert(p->min[pid_K] <= arg->fixed_K && arg->fixed_K <= p->max[pid_K]);

			// run over all cells in the column of index arg->fixed_J.
			for(int i = p->min[pid_I]; i <= p->max[pid_I]; i++) {

				cur.I = i;
				cur.J = arg->fixed_J;
				cur.N = arg->fixed_N;

				if (arg->test(&cur, arg->testarg) == true) {

					fprint_literal(out,
						arg->symb,
						cur.I, 
						arg->fixed_J, 
						arg->fixed_N,
						arg->fixed_K);

				}
			}

			break;

		case 'b':
			assert(p->min[pid_N] <= arg->fixed_N && arg->fixed_N <= p->max[pid_N]);
			assert(p->min[pid_K] <= arg->fixed_K && arg->fixed_K <= p->max[pid_K]);

			// run over all cells in the block of index arg->fixed_B.
			const int size = rank * rank;
			for (int m = 0; m < size; m++) {
				cell_t q = cell_in_block(m, arg->fixed_B, rank);

				cur.I = q.I;
				cur.J = q.J;
				cur.N = arg->fixed_N;

				assert_cell_index(cur.I, rank);
				assert_cell_index(cur.J, rank);
				assert(is_in_block(q, arg->fixed_B, rank));

				if (arg->test(&cur, arg->testarg) == true) {

					fprint_literal(out,
						arg->symb,
						cur.I, 
						cur.J, 
						arg->fixed_N,
						arg->fixed_K);
				}
			}

			break;

		default:
			assert(0);
			exit(EXIT_FAILURE);
	}

}

void fprint_term (FILE *out, const param_t *p, const runarg_t *arg)
{
	fprintf(out, "(and ");

	fprint_literals_running_over(out, p, arg);

	fprintf(out, " )\n");
}

void fprint_clause (FILE *out, const param_t *p, const runarg_t *arg)
{
	fprintf(out, "(or ");

	fprint_literals_running_over(out, p, arg);

	fprintf(out, " )\n");
}

void set_runarg (runarg_t *runarg,
			int i, int j, int n, int b, int k, 
			char type, char symb,
			bool (*test)(const cur_t *, const testarg_t *), testarg_t *testarg)
{
	assert(runarg != NULL);

	runarg->fixed_I = i;
	runarg->fixed_J = j;
	runarg->fixed_N = n;
	runarg->fixed_B = b;
	runarg->fixed_K = k;

	runarg->type = type;
	runarg->symb = symb;

	runarg->test = test;
	runarg->testarg = testarg;
}

void set_testarg(testarg_t *arg, int i, int j, int n, int b, int r)
{
	arg->I = i;
	arg->J = j;
	arg->N = n;
	arg->B = b;

	arg->rank = r;
}

bool test_not_equal_number (const cur_t *cur, const testarg_t *arg)
{
	assert(0 < cur->N && cur->N <= arg->rank * arg->rank);
	assert(0 < arg->N && arg->N <= arg->rank * arg->rank);

	return cur->N != arg->N;
}

bool test_not_equal_cell (const cur_t *cur, const testarg_t *arg)
{
	assert_cell_index(cur->I, arg->rank);
	assert_cell_index(cur->J, arg->rank);
	assert_cell_index(arg->I, arg->rank);
	assert_cell_index(arg->J, arg->rank);

	return (cur->I != arg->I) || (cur->J != arg->J);
}

bool test_not_in_block (const cur_t *cur, const testarg_t *arg)
{
	assert_cell_index(cur->I, arg->rank);
	assert_cell_index(cur->J, arg->rank);
	assert_cell_index(arg->B, arg->rank); // this is abuse but must pass. 

	cell_t q = cell_at(cur->I, cur->J, arg->rank);

	return  false == is_in_block(q, arg->B, arg->rank);
}

bool test_not_in_row (const cur_t *cur, const testarg_t *arg)
{
	assert_cell_index(cur->I, arg->rank);
	assert_cell_index(arg->I, arg->rank);

	return cur->I != arg->I;
}

bool test_not_in_column (const cur_t *cur, const testarg_t *arg)
{
	assert_cell_index(cur->J, arg->rank);
	assert_cell_index(arg->J, arg->rank);

	return cur->J != arg->J;
}

// Print out primitive proposition for X or Y variable.
void fprint_literal (FILE *out, char symb, int i, int j, int n, int k)
{
	assert(0 <= k);

	switch (symb) {
		case 'x':
			fprintf(out, " (= x_%d_%d_%d %d) ",   i, j, k, n);
			break;

		case 'y':
			fprintf(out, " (not y_%d_%d_%d_%d) ", i, j, n, k); 
			break;

		default:
			assert(0);
			exit(EXIT_FAILURE);
	}
}

