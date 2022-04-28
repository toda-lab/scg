#ifndef SCG_ASSERT_H
#define SCG_ASSERT_H

#include "scg_modeler.h"

extern void assert_cell_index (int i, int r);
extern void assert_variable_index (int i, const idmgr_t *mgr);
extern void assert_encoder_decoder (idmgr_t *mgr, data_t *data);
extern void assert_IJNK_active(const param_t *p);

#endif /*SCG_ASSERT_H*/
