#include <assert.h>
#include "scg_assert.h"


void assert_cell_index (int i, int r)
{
        assert(0 <= i);
        assert(i < (r * r));
}

void assert_variable_index (int i, const idmgr_t *data)
{
        assert((data->first) <= i);
        assert(i < (data->first + data->total));
}

void assert_IJNK_active(const param_t *p)
{
#ifndef NDEBUG
        const int pid_I = get_param(tag_I, p);
        const int pid_J = get_param(tag_J, p);
        const int pid_N = get_param(tag_N, p);
        const int pid_K = get_param(tag_K, p);

        assert(p->act[pid_I] == true);
        assert(p->act[pid_J] == true);
        assert(p->act[pid_N] == true);
        assert(p->act[pid_K] == true);
#endif
}

void assert_encoder_decoder(idmgr_t *mgr, data_t *data)
{
#ifndef NDEBUG
        param_t *p = data->p;
        const int rank = data->rank;

        const int len = mgr->len;

        make_all_inactive(p);
        make_assoc_active(p, mgr);

        int *cur_value = (int*)malloc(sizeof(int) * len);
        int *new_value = (int*)malloc(sizeof(int) * len);
        if (cur_value == NULL || new_value == NULL) {
                fprintf(stderr, "EEROR: Memory allocation failed.\n");
                exit(EXIT_FAILURE);
        }

        // check whether the encoder is one-to-one
        for (reset_param(p); p->end == false; next_param(p)) {
                int index;
                read_cur(p, cur_value, mgr);

                mgr->encoder(cur_value, &index,    rank, p, mgr);
                mgr->decoder(index,     new_value, rank, p, mgr);

                assert_variable_index(index, mgr);

                for (int pos = 0; pos < len; pos++) {
                        assert(cur_value[pos] == new_value[pos]);
                }
        }

        // check whether the encoder is on-to
        for (int index = mgr->first; index < (mgr->first + mgr->total); index++) {
                int new_index;

                mgr->decoder(index,     cur_value,  rank, p, mgr);
                mgr->encoder(cur_value, &new_index, rank, p, mgr);

                assert(index == new_index);
        }

        free(cur_value);
        free(new_value);
#endif
}


