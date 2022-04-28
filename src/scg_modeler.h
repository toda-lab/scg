#ifndef SCG_MODELER_H
#define SCG_MODELER_H

#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>

#include "scg_tag.h"

// Types of groups A and B
#define TYPE_ARBB (0)  // A: Row    B: Block
#define TYPE_ACBB (1)  // A: Column B: Block
#define TYPE_ABBR (2)  // A: Block  B: Row
#define TYPE_ABBC (3)  // A: Block  B: Column

#define MAX_PARAMS (100) // maximum number of parameters
#define MAX_STRATS (100) // maximum number of strategies

typedef struct st_cell     cell_t;

typedef struct st_data     data_t;
typedef struct st_param    param_t;
typedef struct st_strat    strat_t;
typedef struct st_idmgr    idmgr_t;

typedef struct st_runarg   runarg_t;
typedef struct st_testarg  testarg_t;
typedef struct st_cur      cur_t;

struct st_cur {
        int I;
        int J;
        int N;
};


struct st_cell {
        int I;   // row index,   ranging from 0 to size - 1.
        int J;   // colum index, ranging from 0 to size - 1.
};

struct st_strat {
        stag_t tag;
        idmgr_t *idmgr;
        void (*fprint_literals_for_x) (FILE *, const data_t *);
        void (*fprint_literals_for_y) (FILE *, const data_t *);
        void (*fprint_cons_for_z)     (FILE *, data_t *);
};

// collection of all necessary data
struct st_data {
        int rank;      // rank * rank = size (standard sudoku has rank 3)
        int size;      // number of rows (equiv. columns) of sudoku board.
        int bound;     // maximum step size

        int nissued;   // total number of ids for auxiliary variabels (i.e. Z variables)

        cell_t *cs;  // array of clue cells, initialized with length size*size
        int nclues;

        param_t *p;    // combination of parameters

        int pid_I, pid_J, pid_N, pid_K; // parameter ids

        strat_t strat[MAX_STRATS]; // strategies
        int nstrats;
};

// combination of parameters
// I    :row index
// J    :column index
// N    :number to be placed in a cell 
// K    :step index
// B    :block index (but this is not handled by param_t)

struct st_param {
        int     cur[MAX_PARAMS]; // current value
        int     min[MAX_PARAMS]; // minimum value
        int     max[MAX_PARAMS]; // maximum value
        bool    act[MAX_PARAMS]; // activity
        stag_t  tag[MAX_PARAMS]; // unique tag

        bool end;  // whether a loop ended.

        int npars; // the number of all parameters
};

// id manager for variables linked to particular strategies
struct st_idmgr {
        int *pid;       // ids for parameters of such variables
        int len;        // number of such parameters.
        int first;      // first index that this manager issues
        int total;      // total number of indices issued by this manager
        void (*encoder) (const int *, int *, int, const param_t *, const idmgr_t *);
        void (*decoder) (int,         int *, int, const param_t *, const idmgr_t *);
        bool (*accepted)(const int *, const data_t *);
};

struct st_runarg {
        //   If not fixed or not used, set -1;
        int  fixed_I;
        int  fixed_J;
        int  fixed_N;
        int  fixed_B;
        int  fixed_K;

        char type;   // v: variable value, r: row, c: column, b:block
        char symb;   // x: X variables, y: Y variables

        bool (*test) (const cur_t *, const testarg_t *);
        testarg_t *testarg;
};

struct st_testarg {
        int I;
        int J;
        int N;
        int B;
        int rank;
};


// functions for input/output
extern void read_input (FILE *in, data_t *data);

// functions for data
extern void init_data   (data_t *data, int rank, int bound);
extern void delete_data (data_t *data);

// functions for strategies
extern void add_strategy (data_t *data,
                        stag_t tag,
                        int  nvars,
                        const int  *pid, int len,
                        void (*encoder) (const int *, int *, int, const param_t *, const idmgr_t *),
                        void (*decoder) (int,         int *, int, const param_t *, const idmgr_t *),
                        bool (*accepted)(const int *, const data_t *),
                        void (*fprint_literals_for_x) (FILE *, const data_t *),
                        void (*fprint_literals_for_y) (FILE *, const data_t *),
                        void (*fprint_cons_for_z)     (FILE *, data_t *));

extern void default_encoder (const int *value, int *index, int rank, const param_t *p, const idmgr_t *mgr);
extern void default_decoder (int index,        int *value, int rank, const param_t *p, const idmgr_t *mgr);

// functions for variable manager
extern const idmgr_t *get_idmgr (stag_t tag, const data_t *data);
extern void  read_cur   (const param_t *p, int *to, const idmgr_t *mgr);
extern int   pos_of_pid (int pid, const idmgr_t *mgr);

// functions for parameter manipulation
extern void init_param  (param_t *p);
extern int  add_param   (int min, int max, stag_t tag, param_t *p);
extern int  get_param   (stag_t tag, const param_t *p);
extern void reset_param (param_t *p);
extern void next_param  (param_t *p);
extern int  count_combinations_of_IJNK_values (const param_t *p);

extern bool accepted_general (const int *buf, const idmgr_t *mgr, const data_t *data);

extern void make_all_inactive (param_t *p);
extern void make_IJNK_active (param_t *p);
extern void make_IJ_active   (param_t *p);
extern void make_IJN_active  (param_t *p);
extern void make_IJK_active  (param_t *p); 
extern void make_assoc_active (param_t *p, const idmgr_t *mgr);

// functions for cells and blocks
extern bool   is_clue_cell  (cell_t q, const cell_t *cs, int n);
extern bool   equal_cell    (cell_t q1, cell_t q2);
extern bool   larger_cell   (cell_t q1, cell_t q2);
extern cell_t cell_at       (int i, int j, int r);
extern cell_t cell_in_block (int m, int n, int r);
extern int    ownerblock    (cell_t q, int r);
extern bool   is_in_block   (cell_t q, int n, int r);

extern bool have_common_cell (int group_A, int group_B, int type_AB, int rank);

// variable declarations
extern void fprint_decl_for_x (FILE *out, data_t *data);
extern void fprint_decl_for_y (FILE *out, data_t *data);
extern void fprint_decl_for_z (FILE *out, data_t *data);

// state transition framework
extern void fprint_cons_for_init  (FILE *out, data_t *data);
extern void fprint_cons_for_trans (FILE *out, data_t *data);
extern void fprint_cons_for_final (FILE *out, data_t *data);
extern void fprint_cons_for_strat (FILE *out, data_t *data);

// functions for printing out boolean expressions
extern void fprint_term   (FILE *out, const param_t *p, const runarg_t *arg);
extern void fprint_clause (FILE *out, const param_t *p, const runarg_t *arg);
extern void fprint_literal(FILE *out, char symb, int i, int j, int n, int k);

extern void set_runarg (runarg_t *runarg,
                        int i, int j, int n, int b, int k,
                        char type, char symb,
                        bool (*test)(const cur_t *, const testarg_t *), testarg_t *testarg);

extern void set_testarg (testarg_t *arg, int i, int j, int n, int b, int r);

extern bool test_not_equal_number (const cur_t *cur, const testarg_t *arg);
extern bool test_not_equal_cell   (const cur_t *cur, const testarg_t *arg);
extern bool test_not_in_block     (const cur_t *cur, const testarg_t *arg);
extern bool test_not_in_row       (const cur_t *cur, const testarg_t *arg);
extern bool test_not_in_column    (const cur_t *cur, const testarg_t *arg);

#endif /*SCG_MODELER_H*/
