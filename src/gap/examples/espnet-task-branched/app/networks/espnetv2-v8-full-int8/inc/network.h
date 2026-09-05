#ifndef ESPNET_NETWORK_H
#define ESPNET_NETWORK_H
#include <stddef.h>
#include "pmsis.h"
/* One in-flight inference; input/output may alias workspace. */
void network_initialize(void);
void network_terminate(void);
void network_run_async_cl(void *, size_t, void *, int, int, pi_device_t *, pi_task_t *);
#endif
