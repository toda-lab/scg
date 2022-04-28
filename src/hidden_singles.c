#include <assert.h>

#include "hidden_singles.h"
#include "scg_modeler.h"
#include "scg_assert.h"

// values of auxiliary parameter for Hidden Singles
#define HS_ROW (0)
#define HS_COL (1)
#define HS_BLK (2)
#define HS_MIN HS_ROW
#define HS_MAX HS_BLK

static void fprint_cons_for_z_in_hidden_singles (FILE *out, data_t *data);
static void fprint_literals_for_x_in_hidden_singles (FILE *out, const data_t *data);
static bool accepted_HS_version (const int *buf, const data_t *data);

void add_hidden_singles_strategy (data_t *data)
{
        param_t *p = data->p;

        const int pid_HS = add_param(HS_MIN, HS_MAX, tag_HS, p);
        assert(pid_HS >= 0);

        int nvars = count_combinations_of_IJNK_values(p);
        nvars = nvars * (HS_MAX - HS_MIN + 1);
        // NOTE: for simplicity, the strategy requests more variables than needed.
	// The function accepted() decides whether variables really appear in constraints.

        const int len = 5;

        assert(len == 5);
        int buf[len];
        buf[0] = data->pid_I;
        buf[1] = data->pid_J;
        buf[2] = data->pid_N;
        buf[3] = data->pid_K;
        buf[4] = pid_HS;

        add_strategy(data, tag_HS, nvars, buf, len,
                        default_encoder,
                        default_decoder,
                        accepted_HS_version,
                        fprint_literals_for_x_in_hidden_singles,
                        NULL,
                        fprint_cons_for_z_in_hidden_singles);
}

// Print out constraints for Hidden Singles
// Hidden Single (row)   : none of the cells except (i,j) in the same row    has n as a candidate in step k-1.
// Hidden Single (column): none of the cells except (i,j) in the same column has n as a candidate in step k-1.
// Hidden Single (block) : none of the cells except (i,j) in the same block  has n as a candidate in step k-1.
//
static void fprint_cons_for_z_in_hidden_singles (FILE *out, data_t *data)
{
        fprintf(out, ";\n");
        fprintf(out, "; Constraints for Hidden Singles\n");

        param_t *p = data->p;

        const int rank = data->rank;
        const int pid_I = data->pid_I;
        const int pid_J = data->pid_J;
        const int pid_N = data->pid_N;
        const int pid_K = data->pid_K;

        const idmgr_t *mgr = get_idmgr(tag_HS, data);
        assert(mgr != NULL);

        assert(mgr->len == 5);
        int buf[5];

        const int pid_HS = get_param(tag_HS, p);
        assert(pid_HS >= 0);

        make_all_inactive(p);
        make_assoc_active(p, mgr);

        for(reset_param(p); p->end == false; next_param(p)) {
                if (p->cur[pid_K] == p->min[pid_K])     continue;

                runarg_t runarg;
                testarg_t testarg;

                switch (p->cur[pid_HS]) {
                        case HS_ROW:
                                set_testarg(&testarg, p->cur[pid_I], p->cur[pid_J], -1, -1, rank);
                                set_runarg(&runarg,
                                        p->cur[pid_I],
                                        -1,
                                        p->cur[pid_N],
                                        -1,
                                        p->cur[pid_K] - 1,
                                        'r', 'y',
                                        test_not_equal_cell, &testarg);
                                break;

                        case HS_COL:
                                set_testarg(&testarg, p->cur[pid_I], p->cur[pid_J], -1, -1, rank);
                                set_runarg(&runarg,
                                        -1,
                                        p->cur[pid_J],
                                        p->cur[pid_N],
                                        -1,
                                        p->cur[pid_K] - 1,
                                        'c', 'y',
                                        test_not_equal_cell, &testarg);
                                break;

                        case HS_BLK:
                                {
                                set_testarg(&testarg, p->cur[pid_I], p->cur[pid_J], -1, -1, rank);

                                cell_t q = cell_at(p->cur[pid_I], p->cur[pid_J], rank);
                                const int group_B = ownerblock(q, rank);

                                set_runarg(&runarg,
                                        -1,
                                        -1,
                                        p->cur[pid_N],
                                        group_B,
                                        p->cur[pid_K] - 1,
                                        'b', 'y',
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
                        mgr->encoder(buf, &index, data->rank, p, mgr);
                        assert_variable_index(index, mgr);

                        fprintf(out, "(iff  z_%d ", index);
                        fprint_term(out, p, &runarg);
                        fprintf(out, " )\n");
                }
        }

}

// Note: this function is supposed to be called within parameter loop.
// In order not to change parameter values, declare const for param_t*.
static void fprint_literals_for_x_in_hidden_singles (FILE *out, const data_t *data)
{
        const param_t *p = data->p;
        assert_IJNK_active(p);
        assert(p->end == false);

        // skip the initial step (because there is no previous step)!
        if (p->cur[data->pid_K] == p->min[data->pid_K]) return;

        const int pid_HS = get_param(tag_HS, p);
        const idmgr_t *mgr = get_idmgr(tag_HS, data);
        assert(mgr != NULL);

        const int len = mgr->len;
        assert(len == 5);
        int buf[5];

        assert(pid_HS >= 0);

        for (int v = p->min[pid_HS]; v <= p->max[pid_HS]; v++) {

                for (int pos = 0; pos < len; pos++) {
                        const int pid = mgr->pid[pos];
                        buf[pos] = (pid == pid_HS ? v: p->cur[pid]);

                        assert(pid == pid_HS || p->act[pid] == true);
                }

                if (true == mgr->accepted(buf, data)) {
                        int index;
                        mgr->encoder(buf, &index, data->rank, p, mgr);
                        assert_variable_index(index, mgr);

                        fprintf(out, " z_%d ", index);
                }
        }

#ifndef NDEBUG
{
        bool exists = false;
        for (int pos = 0; pos < len; pos++) {
                if (mgr->pid[pos] == pid_HS) {
                        exists = true;
                }
        }
        assert(exists == true);
}
#endif
}

// This function determines whether the combination of parameter values,
// held by the array buf, is accepted or not.
// If not accepted, the corresponding variable to the parameter values
// will not appear in constraints.
//
// Note: this function is supposed to be called within parameter loop.
// In order not to change parameter values, declare const for param_t*.
//
static bool accepted_HS_version (const int *buf, const data_t *data)
{
        const param_t *p = data->p;

        const idmgr_t *mgr = get_idmgr(tag_HS, data);
        assert(mgr != NULL);

        const int pid_K = data->pid_K;
        const int pos_K = pos_of_pid(pid_K, mgr);
        assert(0 <= pos_K);

        if (buf[pos_K] == p->min[pid_K]) return false;

        return accepted_general(buf, mgr, data);
}

