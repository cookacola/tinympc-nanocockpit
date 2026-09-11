#include "../pulp-frontnet/config.h"
#if CAMERA_CROP_WIDTH != 160 || CAMERA_CROP_HEIGHT != 160
#error DepthGate requires160x160 camera frame before center crop
#endif

/* Standalone deployment must not depend on semihosted debug output. */
#undef VERBOSE
