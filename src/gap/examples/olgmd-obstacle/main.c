#include "camera.h"
#include "cluster.h"
#include "config.h"
#include "coroutine.h"
#include "olgmd1.h"
#include "olgmd_uart.h"
#include "soc.h"
#include "trace.h"
#include "uart.h"
#include "uart_protocol.h"

#include <pmsis.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define REQUIRED_FREE_L2_MARGIN (32u * 1024u)

static uart_t uart;
static uart_protocol_t uart_protocol;
static camera_t camera;
static pi_device_t cluster;
static PI_FC_L1 olgmd1_config_t olgmd_config;
static PI_FC_L1 olgmd1_state_t *olgmd_state;
static PI_CL_L1 olgmd1_scratch_t olgmd_scratch;
static struct pi_cluster_task olgmd_cluster_task;
static PI_FC_L1 state_msg_t latest_state;
static PI_FC_L1 co_fn_ctx_t inference_ctx;

typedef struct {
  frame_t *frame;
  pi_task_t *done;
} inference_args_t;

typedef struct {
  const uint8_t *frame;
  uint16_t stride;
  uint16_t sequence;
  olgmd1_result_t result;
} olgmd_cluster_args_t;

static PI_L2 olgmd_cluster_args_t olgmd_args;

static void olgmd_cluster_entry(void *opaque) {
  olgmd_cluster_args_t *args = (olgmd_cluster_args_t *)opaque;
  if (args->sequence == 1u) {
    olgmd1_prime(olgmd_state, &olgmd_scratch, args->frame, args->stride);
    args->result = (olgmd1_result_t){0};
    return;
  }
  olgmd1_step(olgmd_state, &olgmd_scratch, args->frame, args->stride,
              &olgmd_config, &args->result);
}

static uint16_t wire_sequence(const frame_t *frame) {
  uint16_t sequence = (uint16_t)(frame->sequence_id + 1u);
  return sequence == 0u ? 1u : sequence;
}

CO_FN_DECLARE(inference_task);

CO_FN_BEGIN(camera_callback, frame_t *, frame)
{
  static PI_FC_L1 inference_args_t args;
  static PI_FC_L1 co_event_t done;
  args.frame = frame;
  args.done = co_event_init(&done);
  co_fn_push_start(&inference_ctx, inference_task, &args, NULL);
  CO_WAIT(&done);
}
CO_FN_END()

CO_FN_BEGIN(inference_task, inference_args_t *, args)
{
  static PI_FC_L1 co_event_t tx_done;
  static PI_FC_L1 co_event_t olgmd_done;
  static PI_FC_L1 olgmd_threat_payload_t threat_payload;
  static PI_FC_L1 olgmd_diagnostic_payload_t diagnostic_payload;
  static PI_FC_L1 uint16_t sequence;
  static PI_FC_L1 uint32_t timestamp;

  sequence = wire_sequence(args->frame);
  timestamp = latest_state.timestamp != 0u
      ? latest_state.timestamp : args->frame->frame_timestamp / 1000u;
  olgmd_args = (olgmd_cluster_args_t) {
      .frame = args->frame->buffer,
      .stride = CAMERA_CROP_WIDTH,
      .sequence = sequence,
  };
  pi_cluster_task(&olgmd_cluster_task, olgmd_cluster_entry, &olgmd_args);
  olgmd_cluster_task.nb_cores = 1;
  olgmd_cluster_task.stack_size = 2048;
  olgmd_cluster_task.slave_stack_size = 0;
  pi_cluster_send_task_to_cl_async(
      &cluster, &olgmd_cluster_task, co_event_init(&olgmd_done));
  CO_WAIT(&olgmd_done);

  threat_payload = (olgmd_threat_payload_t) {
      .source_timestamp_ms = timestamp,
      .frame_sequence = sequence,
      .imminent_threat =
          olgmd_args.result.valid && olgmd_args.result.threat ? 1u : 0u,
      .reserved = 0u,
  };
  olgmd_send_threat_async(
      &uart, &threat_payload, co_event_init(&tx_done));
  CO_WAIT(&tx_done);
  diagnostic_payload = (olgmd_diagnostic_payload_t) {
      .source_timestamp_ms = timestamp,
      .frame_sequence = sequence,
      .membrane_q15 = olgmd_args.result.membrane_q15,
      .spike_count = olgmd_args.result.spike_count,
      .imminent_threat = threat_payload.imminent_threat,
      .valid = olgmd_args.result.valid ? 1u : 0u,
      .reserved = 0u,
  };
  olgmd_send_diagnostic_async(
      &uart, &diagnostic_payload, co_event_init(&tx_done));
  CO_WAIT(&tx_done);
  if (sequence <= 5u || (sequence % 30u) == 0u ||
      threat_payload.imminent_threat || !olgmd_args.result.valid) {
    printf("olgmd-obstacle: UART TX complete seq=%u threat=%u valid=%u\n",
           (unsigned)sequence,
           (unsigned)threat_payload.imminent_threat,
           (unsigned)olgmd_args.result.valid);
  }
  pi_task_push(args->done);
}
CO_FN_END()

CO_FN_BEGIN(uart_callback, uart_msg_t *, message)
{
  if (memcmp(message->header, UART_STATE_MSG_HEADER,
             UART_HEADER_LENGTH) == 0) {
    latest_state = message->state;
  }
}
CO_FN_END()

static void main_task(void) {
  soc_init();
  uart_init(&uart);
  uart_protocol_init(&uart_protocol, &uart, uart_callback);
  camera_init(&camera, camera_callback);
  camera_init_frames_alloc(&camera);
  cluster_init(&cluster);

  olgmd_state = pi_l2_malloc(sizeof(*olgmd_state));
  if (!olgmd_state) {
    printf("olgmd-obstacle: unable to allocate %u-byte L2 state\n",
           (unsigned)sizeof(*olgmd_state));
    pmsis_exit(-2);
  }

  void *margin_probe = pi_l2_malloc(REQUIRED_FREE_L2_MARGIN);
  if (!margin_probe) {
    printf("olgmd-obstacle: less than %u bytes of free contiguous L2\n",
           (unsigned)REQUIRED_FREE_L2_MARGIN);
    pmsis_exit(-1);
  }
  pi_l2_free(margin_probe, REQUIRED_FREE_L2_MARGIN);
  printf("olgmd-obstacle: verified %u-byte contiguous L2 margin\n",
         (unsigned)REQUIRED_FREE_L2_MARGIN);

  olgmd_config = olgmd1_default_config();
  olgmd1_init(olgmd_state);
  trace_init();
  uart_protocol_start(&uart_protocol);
  camera_start(&camera);
  while (true) {
    camera_watchdog_poll(&camera);
    pi_yield();
  }
}

int main(void) {
  return pmsis_kickoff((void *)main_task);
}
