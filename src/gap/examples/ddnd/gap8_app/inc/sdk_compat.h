#include "pmsis.h"
#ifdef __PULP_OS__
typedef struct pi_device pi_device_t;
#define pi_time_wait_us rt_time_wait_us
#define pi_time_get_us rt_time_get_us
#endif
