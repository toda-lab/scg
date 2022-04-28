#include <assert.h>

#include "sudoku_rule.h"
#include "scg_modeler.h"
#include "scg_assert.h"

#define SR_NUM (0)
#define SR_ROW (1)
#define SR_COL (2)
#define SR_BLK (3)
#define SR_MIN SR_NUM
#define SR_MAX SR_BLK

static bool accepted_SR_version (const int *buf, const data_t *data);
static void fprint_cons_for_z_in_sudoku_rule (FILE *out, data_t *data);
static void fprint_literals_for_y_in_sudoku_rule (FILE *out, const data_t *data);

void add_sudoku_rule (data_t *data)
{
        param_t *p = data->p;

        const int pid_SR = add_param(SR_MIN, SR_MAX, tag_SR, p);
        assert(pid_SR >= 0);

        int nvars = count_combinations_of_IJNK_values(p);
        nvars = nvars * (SR_MAX - SR_MIN + 1);
        // NOTE: for simplicity, the strategy requests more variables than needed.
	// The function accepted() decides whether variables really appear in constraints.

        const int len = 5;

        assert(len == 5);
        int buf[len];
        buf[0] = data->pid_I;
        buf[1] = data->pid_J;
        buf[2] = data->pid_N;
        buf[3] = data->pid_K;
        buf[4] = pid_SR;

        add_strategy(data, tag_SR, nvars, buf, len,
                        default_encoder,
                        default_decoder,
                        accepted_SR_version,
                        NULL,
                        fprint_literals_for_y_in_sudoku_rule,
                        fprint_cons_for_z_in_sudoku_rule);
}

// Print out constraints for the Sudoku Rule:
// SR_NUM: another number from n is placed in (i,j) at k.
// SR_ROW: n is placed in another cell of the same row    as (i,j) at k.
// SR_COL: n is placed in another cell of the same column as (i,j) at k.
// SR_BLK: n is placed in another cell of the same block  as (i,j) at k.
//
static void fprint_cons_for_z_in_sudoku_rule (FILE *out, data_t *data)
{
        fprintf(out, ";\n");
        fprintf(out, "; Constraints for Sudoku Rule\n");

        param_t *p = data->p;
        const int rank = data->rank;

        const idmgr_t *mgr = get_idmgr(tag_SR, data);
        assert(mgr != NULL);

        assert(mgr->len == 5);
        int buf[5];

        const int  pid_SR  = get_param(tag_SR, p);
        assert(pid_SR >= 0);

        const int pid_I = data->pid_I;
        const int pid_J = data->pid_J;
        const int pid_N = data->pid_N;
        const int pid_K = data->pid_K;

        make_all_inactive(p);
        make_assoc_active(p, mgr);

        for(reset_param(p); p->end == false; next_param(p)) {

                runarg_t  runarg;
                testarg_t testarg;

                switch (p->cur[pid_SR]) {
                        case SR_NUM:
                                set_testarg(&testarg, -1, -1, p->cur[pid_N], -1, rank);
                                set_runarg(&runarg,
                                        p->cur[pid_I],
                                        p->cur[pid_J],
                                        -1,
                                        -1,
                                        p->cur[pid_K],
                                        'v', 'x',
                                        test_not_equal_number, &testarg);
                                break;

                        case SR_ROW:
                                set_testarg(&testarg, p->cur[pid_I], p->cur[pid_J], -1, -1, rank);
                                set_runarg(&runarg,
                                        p->cur[pid_I],
                                        -1,
                                        p->cur[pid_N],
                                        -1,
                                        p->cur[pid_K],
                                        'r', 'x',
                                        test_not_equal_cell, &testarg);
                                break;

                        case SR_COL:
                                set_testarg(&testarg, p->cur[pid_I], p->cur[pid_J], -1, -1, rank);
                                set_runarg(&runarg,
                                        -1,
                                        p->cur[pid_J],
                                        p->cur[pid_N],
                                        -1,
                                        p->cur[pid_K],
                                        'c', 'x',
                                        test_not_equal_cell, &testarg);
                                break;

                        case SR_BLK:
                                {
                                set_testarg(&testarg, p->cur[pid_I], p->cur[pid_J], -1, -1, rank);

                                cell_t q = cell_at(p->cur[pid_I], p->cur[pid_J], rank);
                                const int group_B = ownerblock(q, rank);

                                set_runarg(&runarg,
                                        -1,
                                        -1,
                                        p->cur[pid_N],
                                        group_B,
                                        p->cur[pid_K],
                                        'b', 'x',
                                        test_not_equal_cell, &testarg);
                                }
                                break;

                        default:
                                assert(0);
                                exit(EXIT_FAILURE);
                }


                read_cur(p, buf, mgr);

                if (true == mgr->accepted(buf, data)) {
                        int index;
                        mgr->encoder(buf, &index, rank, p, mgr);
                        assert_variable_index(index, mgr);

                        fprintf(out, "(iff  z_%d ", index);
                        fprint_clause (out, p, &runarg);
                        fprintf(out, " )\n");

                }
        }

        make_all_inactive(p);
        make_IJK_active(p);
        for(reset_param(p); p->end == false; next_param(p)) {
          fprintf(out, "(or  ");
	        for (int n = p->min[pid_N]; n <= p->max[pid_N]; n++) {
            fprintf(out, " y_%d_%d_%d_%d ", 
			                        p->cur[pid_I], 
                              p->cur[pid_J], 
                              n, 
                              p->cur[pid_K]);
          }
          fprintf(out, " )\n");
        }


        //make_all_inactive(p);
        //make_IJNK_active(p);
        //for(reset_param(p); p->end == false; next_param(p)) {
		//fprintf(out, "(imp (= x_%d_%d_%d %d) y_%d_%d_%d_%d)\n",
			//p->cur[pid_I], p->cur[pid_J], p->cur[pid_K], p->cur[pid_N], 
			//p->cur[pid_I], p->cur[pid_J], p->cur[pid_N], p->cur[pid_K]);
        //}
}

// Note: this function is supposed to be called within parameter loop.
// In order not to change parameter values, declare const for param_t*.
static void fprint_literals_for_y_in_sudoku_rule (FILE *out, const data_t *data)
{
        const param_t *p   = data->p;
        assert_IJNK_active(p);
        assert(p->end == false);

        // Do not skip the initial step (because sudoku rule does not need the previous step)!

        const int pid_SR   = get_param(tag_SR, p);
        const idmgr_t *mgr = get_idmgr(tag_SR, data);
        assert(mgr != NULL);

        const int len    = mgr->len;
        assert(len == 5);
        int buf[5];

        assert(pid_SR >= 0);
        assert(buf  != NULL);


        for (int v = p->min[pid_SR]; v <= p->max[pid_SR]; v++) {
                for (int pos = 0; pos < len; pos++) {
                        const int pid = mgr->pid[pos];

                        buf[pos] = (pid == pid_SR ? v: p->cur[pid]);

                        assert(pid == pid_SR || p->act[pid] == true);
                }

                if (true == mgr->accepted(buf, data)) {
                        int index;
                        mgr->encoder(buf, &index, data->rank, p, mgr);
                        assert_variable_index(index, mgr);

                        fprintf(out, " z_%d ", index);
                }
        }
}

// This function determines whether the combination of parameter values,
// held by the array buf, is accepted or not.
// If not accepted, the corresponding variable to the parameter values
// will not appear in constraints.
//
// Note: this function is supposed to be called within parameter loop.
// In order not to change parameter values, declare const for param_t*.
//
static bool accepted_SR_version (const int *buf, const data_t *data)
{
        const param_t *p = data->p;

        const idmgr_t *mgr = get_idmgr(tag_SR, data);
        assert(mgr != NULL);

        return accepted_general(buf, mgr, data);
}

