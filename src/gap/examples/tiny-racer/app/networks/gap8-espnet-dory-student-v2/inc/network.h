#ifndef ESPNET_DORY_NETWORK_H
#define ESPNET_DORY_NETWORK_H
#include <stddef.h>
#include "pmsis.h"
void network_initialize(void);
void network_terminate(void);
void network_run_async_cl(void *l2_buffer, size_t l2_buffer_size,
                          void *l2_final_output, int exec, int initial_dir,
                          pi_device_t *cluster, pi_task_t *network_done);
#endif
