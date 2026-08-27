#ifndef GAP8_SEQUENTIAL_NETWORK_ADAPTER_H
#define GAP8_SEQUENTIAL_NETWORK_ADAPTER_H

#include <stddef.h>
#include "pmsis.h"
#include "gap8_network.h"

#define network_initialize gap8_network_initialize
#define network_terminate gap8_network_terminate
#define network_run gap8_network_run
#define network_run_async gap8_network_run_async
#define network_run_wait gap8_network_run_wait
#define network_run_async_cl gap8_network_run_async_cl

#endif
