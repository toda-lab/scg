#include <assert.h>

#include "locked_candidates.h"
#include "scg_assert.h"

// values of axuliary parameter (group types of A and B) for Locked Candidates strategy
#define LC_ARBB TYPE_ARBB  // A: Row    B: Block
#define LC_ACBB TYPE_ACBB  // A: Column B: Block
#define LC_ABBR TYPE_ABBR  // A: Block  B: Row
#define LC_ABBC TYPE_ABBC  // A: Block  B: Column
#define LC_MIN LC_ARBB
#define LC_MAX LC_ABBC

static void fprint_cons_for_z_in_locked_candidates (FILE *out, data_t *data);
static void fprint_literals_for_y_in_locked_candidates (FILE *out, const data_t *data);
static bool accepted_LC_version (const int *buf, const data_t *data);
static void set_index_for_NKLC123 (
        int index_N, int index_K, int index_LC_A, int index_LC_B, int index_LC_T,
        int *buf, const param_t *p, const idmgr_t *mgr);

void add_locked_candidates_strategy (data_t *data)
{
        param_t *p = data->p;

        const int pid_LC_A = add_param(0, data->size - 1, tag_LC_A, p);
        const int pid_LC_B = add_param(0, data->size - 1, tag_LC_B, p);
        const int pid_LC_T = add_param(LC_MIN, LC_MAX,    tag_LC_T, p);

        assert(pid_LC_A >= 0);
        assert(pid_LC_B >= 0);
        assert(pid_LC_T >= 0);

        const int pid_N = data->pid_N;
        const int pid_K = data->pid_K;

        int nvars = 1;
        for (int pid = 0; pid < p->npars; pid++) {
                if (pid == pid_N    || pid == pid_K
                ||  pid == pid_LC_A || pid == pid_LC_B || pid == pid_LC_T) {
                        nvars = nvars * (p->max[pid] - p->min[pid] + 1);
                }
        }
        // NOTE: for simplicity, the strategy requests more variables than needed.
	// The function accepted() decides whether variables really appear in constraints.

        const int len = 5;

        assert(len == 5);
        int buf[len];
        buf[0] = pid_N;
        buf[1] = pid_K;
        buf[2] = pid_LC_A;
        buf[3] = pid_LC_B;
        buf[4] = pid_LC_T;
        add_strategy(data, tag_LC, nvars, buf, len,
                        default_encoder,
                        default_decoder,
                        accepted_LC_version,
                        NULL,
                        fprint_literals_for_y_in_locked_candidates,
                        fprint_cons_for_z_in_locked_candidates);
}

// Print out constraints for Locked Candidates strategy.
static void fprint_cons_for_z_in_locked_candidates (FILE *out, data_t *data)
{
        fprintf(out, ";\n");
        fprintf(out, "; Constraints for Locked Candidates\n");

        param_t *p = data->p;

        const int rank  = data->rank;
        const int pid_N = data->pid_N;
        const int pid_K = data->pid_K;

        const idmgr_t *mgr = get_idmgr(tag_LC, data);
        assert(mgr != NULL);

        const int pid_LC_A = get_param(tag_LC_A, p);
        const int pid_LC_B = get_param(tag_LC_B, p);
        const int pid_LC_T = get_param(tag_LC_T, p);
        assert(pid_LC_A >= 0);
        assert(pid_LC_B >= 0);
        assert(pid_LC_T >= 0);

        make_all_inactive(p);
        make_assoc_active(p, mgr);

        assert(mgr->len == 5);
        int buf[5];

        for(reset_param(p); p->end == false; next_param(p)) {
                if (p->cur[pid_K] == p->min[pid_K])     continue;

                const int group_A  = p->cur[pid_LC_A];
                const int group_B  = p->cur[pid_LC_B];
                const int type_AB = p->cur[pid_LC_T];

                runarg_t  runarg;
                testarg_t testarg;

                switch (type_AB) {
                        case LC_ARBB: // A: Row, B: Block
                                {
                                set_testarg(&testarg, group_A, -1, -1, -1, rank);
                                set_runarg(&runarg,
                                        -1,
                                        -1,
                                        p->cur[pid_N],
                                        group_B,  // run over all cells in the block of group_B
                                        p->cur[pid_K] - 1,
                                        'b', 'y',
                                        test_not_in_row, &testarg); // but skip all cells in the row of group_A
                                }
                                break;

                        case LC_ACBB: // A: Column, B: Block
                                {
                                set_testarg(&testarg, -1, group_A, -1, -1, rank);
                                set_runarg(&runarg,
                                        -1,
                                        -1,
                                        p->cur[pid_N],
                                        group_B,  // run over all cells in the block of group_B
                                        p->cur[pid_K] - 1,
                                        'b', 'y',
                                        test_not_in_column, &testarg); // but skip all cells in the column of group_A
                                }
                                break;

                        case LC_ABBR: // A: Block, B: Row
                                {
                                set_testarg(&testarg, -1, -1, -1, group_A, rank);
                                set_runarg(&runarg,
                                        group_B, // run over all cells in the row of group_B
                                        -1,
                                        p->cur[pid_N],
                                        -1,
                                        p->cur[pid_K] - 1,
                                        'r', 'y',
                                        test_not_in_block, &testarg); // but skip all cells in the block of group_A
                                }
                                break;

                        case LC_ABBC: // A: Block, B: Column
                                {
                                set_testarg(&testarg, -1, -1, -1, group_A, rank);
                                set_runarg(&runarg,
                                        -1,
                                        group_B, // run over all cells in the column of group_B
                                        p->cur[pid_N],
                                        -1,
                                        p->cur[pid_K] - 1,
                                        'c', 'y',
                                        test_not_in_block, &testarg); // but skip all cells in the block of group_A
                                }
                                break;

                        default:
                                assert(0);
                                exit(EXIT_FAILURE);
                }

                set_index_for_NKLC123(
                                p->cur[pid_N],
                                p->cur[pid_K],
                                group_A,
                                group_B,
                                type_AB,
                                buf,
                                p, mgr);


                if (true == mgr->accepted(buf, data)) {
                        assert(true == have_common_cell(group_A, group_B, type_AB, rank));

                        int index;
                        mgr->encoder(buf, &index, rank, p, mgr);
                        assert_variable_index(index, mgr);

                        fprintf(out, "(iff z_%d ", index);
                        fprint_term(out, p, &runarg);
                        fprintf(out, " )\n");
                }
        }

}

// Note: this function is supposed to be called within parameter loop.
// In order not to change parameter values, declare const for param_t*.
static void fprint_literals_for_y_in_locked_candidates (FILE *out, const data_t *data)
{
        const param_t *p = data->p;
        assert_IJNK_active(p);
        assert(p->end == false);

        const int rank = data->rank;

        // skip the initial step (because there is no previous step)!
        if (p->cur[data->pid_K] == p->min[data->pid_K]) return;

        const int pid_LC_A = get_param(tag_LC_A, p);
        const int pid_LC_B = get_param(tag_LC_B, p);
        const int pid_LC_T = get_param(tag_LC_T, p);
        assert(pid_LC_A >= 0);
        assert(pid_LC_B >= 0);
        assert(pid_LC_T >= 0);

        const idmgr_t *mgr = get_idmgr(tag_LC, data);
        assert(mgr    != NULL);

        const int pid_I = data->pid_I;
        const int pid_J = data->pid_J;
        const int pid_N = data->pid_N;
        const int pid_K = data->pid_K;

        int group_A, group_B;
        const cell_t cur_q = cell_at (p->cur[pid_I], p->cur[pid_J], rank);

        assert(mgr->len == 5);
        int buf[5];

        // Run over all possible pairs of groups A and B such that
        // 1) A and B have a common cell, and
        // 2) cur_q is in A but not in B.

        int type_AB;
        { // A: Row, B: Block
                type_AB = LC_ARBB;
                group_A  = cur_q.I;

                for(group_B = 0; group_B < (data->size); group_B++) {

                        if (true  == is_in_block(cur_q, group_B, rank)) continue;

                        set_index_for_NKLC123(
                                        p->cur[pid_N],
                                        p->cur[pid_K],
                                        group_A,
                                        group_B,
                                        LC_ARBB,
                                        buf,
                                        p, mgr);

                        if (true == mgr->accepted(buf, data)) {
                                assert(true == have_common_cell(group_A, group_B, type_AB, rank));

                                int index;
                                mgr->encoder(buf, &index, data->rank, p, mgr);
                                assert_variable_index(index, mgr);

                                fprintf(out, " z_%d ", index);
                        }
                }
        }

        { // A: Column, B: Block
                type_AB = LC_ACBB;
                group_A  = cur_q.J;

                for(group_B = 0; group_B < (data->size); group_B++) {

                        if (true  == is_in_block(cur_q, group_B, rank)) continue;

                        set_index_for_NKLC123(
                                        p->cur[pid_N],
                                        p->cur[pid_K],
                                        group_A,
                                        group_B,
                                        LC_ACBB,
                                        buf,
                                        p, mgr);

                        if (true == mgr->accepted(buf, data)) {
                                assert(true == have_common_cell(group_A, group_B, type_AB, rank));

                                int index;
                                mgr->encoder(buf, &index, data->rank, p, mgr);
                                assert_variable_index(index, mgr);

                                fprintf(out, " z_%d ", index);
                        }
                }
        }

        { // A: Block, B: Row
                type_AB = LC_ABBR;
                group_A  = ownerblock(cur_q, rank);

                for(group_B = p->min[pid_I]; group_B <= p->max[pid_I]; group_B++) {

                        if (group_B == cur_q.I) continue;

                        set_index_for_NKLC123(
                                        p->cur[pid_N],
                                        p->cur[pid_K],
                                        group_A,
                                        group_B,
                                        LC_ABBR,
                                        buf,
                                        p, mgr);

                        if (true == mgr->accepted(buf, data)) {
                                assert(true == have_common_cell(group_A, group_B, type_AB, rank));

                                int index;
                                mgr->encoder(buf, &index, data->rank, p, mgr);
                                assert_variable_index(index, mgr);

                                fprintf(out, " z_%d ", index);
                        }
                }
        }


        { // A: Block, B: Column
                type_AB = LC_ABBC;
                group_A  = ownerblock(cur_q, rank);

                for(group_B = p->min[pid_J]; group_B <= p->max[pid_J]; group_B++) {

                        if (group_B == cur_q.J) continue;

                        set_index_for_NKLC123(
                                        p->cur[pid_N],
                                        p->cur[pid_K],
                                        group_A,
                                        group_B,
                                        LC_ABBC,
                                        buf,
                                        p, mgr);

                        if (true == mgr->accepted(buf, data)) {
                                assert(true == have_common_cell(group_A, group_B, type_AB, rank));

                                int index;
                                mgr->encoder(buf, &index, data->rank, p, mgr);
                                assert_variable_index(index, mgr);

                                fprintf(out, " z_%d ", index);
                        }
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
static bool accepted_LC_version (const int *buf, const data_t *data)
{
        const param_t *p = data->p;

        const idmgr_t *mgr = get_idmgr(tag_LC, data);
        assert(mgr != NULL);

        const int pid_K  = data->pid_K;
        const int pos_K = pos_of_pid(pid_K, mgr);
        assert(0 <= pos_K);

        if (buf[pid_K] == p->min[pid_K]) return false;


        if (false == accepted_general(buf, mgr, data)) {
                return false;
        }

        const int pid_LC_A = get_param(tag_LC_A, p);
        const int pid_LC_B = get_param(tag_LC_B, p);
        const int pid_LC_T = get_param(tag_LC_T, p);

        const int pos_LC_A = pos_of_pid(pid_LC_A, mgr);
        const int pos_LC_B = pos_of_pid(pid_LC_B, mgr);
        const int pos_LC_T = pos_of_pid(pid_LC_T, mgr);

        assert(pos_LC_A >= 0);
        assert(pos_LC_B >= 0);
        assert(pos_LC_T >= 0);

        return have_common_cell(
                        buf[pos_LC_A],
                        buf[pos_LC_B],
                        buf[pos_LC_T],
                        data->rank);
}


// This function simultaneously sets the array buf to the parameter values.
static void set_index_for_NKLC123 (
        int index_N, int index_K, int index_LC_A, int index_LC_B, int index_LC_T,
        int *buf, const param_t *p, const idmgr_t *mgr)
{
        const int pid_N    = get_param(tag_N,    p);
        const int pid_K    = get_param(tag_K,    p);
        const int pid_LC_A = get_param(tag_LC_A, p);
        const int pid_LC_B = get_param(tag_LC_B, p);
        const int pid_LC_T = get_param(tag_LC_T, p);

        const int len = mgr->len;
        assert(len == 5);
        for (int pos = 0; pos < len; pos++) {
                if      (mgr->pid[pos] == pid_N   ) buf[pos] = index_N;
                else if (mgr->pid[pos] == pid_K   ) buf[pos] = index_K;
                else if (mgr->pid[pos] == pid_LC_A) buf[pos] = index_LC_A;
                else if (mgr->pid[pos] == pid_LC_B) buf[pos] = index_LC_B;
                else if (mgr->pid[pos] == pid_LC_T) buf[pos] = index_LC_T;
                else {assert(0); exit(EXIT_FAILURE);}
        }
}

