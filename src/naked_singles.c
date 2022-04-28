#include<assert.h>

#include "naked_singles.h"
#include "scg_assert.h"

static void fprint_cons_for_z_in_naked_singles (FILE *out, data_t *data);
static void fprint_literals_for_x_in_naked_singles (FILE *out, const data_t *data);
static bool accepted_NS_version (const int *buf, const data_t *data);

void add_naked_singles_strategy (data_t *data)
{
        param_t *p = data->p;
        const int nvars = count_combinations_of_IJNK_values(p);
        // NOTE: for simplicity, the strategy requests more variables than needed.
	// The function accepted() decides whether variables really appear in constraints.

        const int len = 4;

        assert(len == 4);
        int buf[len];
        buf[0] = data->pid_I;
        buf[1] = data->pid_J;
        buf[2] = data->pid_N;
        buf[3] = data->pid_K;

        add_strategy(data, tag_NS, nvars, buf, len,
                        default_encoder,
                        default_decoder,
                        accepted_NS_version,
                        fprint_literals_for_x_in_naked_singles,
                        NULL,
                        fprint_cons_for_z_in_naked_singles);

}

// Print out constraints for Naked Singles:
// all numbers but n are not candidates at (i,j) in step k-1.
//
// Note: this condition does not request for n being a candidate, but
// this is not necessary because otherwise, contradiction follows 
// from the condition and sudoku rule.
//
static void fprint_cons_for_z_in_naked_singles (FILE *out, data_t *data)
{
        fprintf(out, ";\n");
        fprintf(out, "; Constraints for Naked Singles\n");

        param_t *p = data->p;

        const int rank = data->rank;

        const int pid_I = data->pid_I;
        const int pid_J = data->pid_J;
        const int pid_N = data->pid_N;
        const int pid_K = data->pid_K;

        const idmgr_t *mgr = get_idmgr(tag_NS, data);
        assert(mgr != NULL);

        assert(mgr->len == 4);
        int buf[4];

        make_all_inactive(p);
        make_assoc_active(p, mgr);

        for(reset_param(p); p->end == false; next_param(p)) {
                if (p->cur[pid_K] == p->min[pid_K])     continue;

                runarg_t  runarg;
                testarg_t testarg;
                set_testarg(&testarg, -1, -1, p->cur[pid_N], -1, rank);

                set_runarg(&runarg,
                        p->cur[pid_I],
                        p->cur[pid_J],
                        -1,
                        -1,
                        p->cur[pid_K] - 1,
                        'v', 'y',
                        test_not_equal_number, &testarg);

                read_cur(p, buf, mgr);

                if (true == mgr->accepted(buf, data)) {
                        int index;
                        mgr->encoder(buf, &index, rank, p, mgr);
                        assert_variable_index(index, mgr);

                        fprintf(out, "(iff  z_%d ", index);
                        fprint_term (out, p, &runarg);
                        fprintf(out, " )\n");
                }
        }

}

// Note: this function is supposed to be called within parameter loop.
// In order not to change parameter values, declare const for param_t*.
static void fprint_literals_for_x_in_naked_singles (FILE *out, const data_t *data)
{
        const param_t *p = data->p;
        assert_IJNK_active(p);
        assert(p->end == false);

        // skip the initial step (because there is no previous step)!
        if (p->cur[data->pid_K] == p->min[data->pid_K]) return;

        const idmgr_t *mgr = get_idmgr(tag_NS, data);
        assert(mgr != NULL);

        assert(mgr->len == 4);
        int buf[4];

        read_cur(p, buf, mgr);

        if (true == mgr->accepted(buf, data)) {
                int index;
                mgr->encoder(buf, &index, data->rank, p, mgr);
                assert_variable_index(index, mgr);

                fprintf(out, " z_%d ", index);
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
static bool accepted_NS_version (const int *buf, const data_t *data)
{
        const param_t *p = data->p;

        const idmgr_t *mgr = get_idmgr(tag_NS, data);
        assert(mgr != NULL);

        const int pid_K = data->pid_K;
        const int pos_K = pos_of_pid(pid_K, mgr);
        assert(0 <= pos_K);

        if (buf[pos_K] == p->min[pid_K]) return false; // no naked single strategy for the initial step.

        return accepted_general(buf, mgr, data);
}
