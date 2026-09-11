/* Camera mode preserves the flown NanoCockpit HM01B0 register configuration.
 * Compact DepthGate output is the default; raw tensors are diagnostic only. */
#include "pmsis.h"
#include "sdk_compat.h"
#include "bsp/bsp.h"
#include "bsp/fs.h"
#include "bsp/fs/readfs.h"
#include "bsp/flash.h"
#include "bsp/flash/hyperflash.h"
#include "camera.h"
#include "ddnd_runtime.h"
#include <stdint.h>
#ifdef DDND_LIVE_DEBUG
#include <stdio.h>
#endif
#include <string.h>
#include "graph.h"
#include "ddnd_board.h"
#ifndef DDND_UART_BAUD
#ifdef DDND_RAW_UART
#define DDND_UART_BAUD 921600
#else
#define DDND_UART_BAUD 115200
#endif
#endif
#include "depthgate_output.h"
/* UART uDMA reads L2 memory; the FC stack is not a DMA source. */
#ifdef DDND_RAW_UART
static PI_L2 uint32_t uart_crc;
#else
static PI_L2 DDNDDepthGatePacket compact __attribute__((aligned(4)));
#endif
static uint32_t capture_ms, clock_last_us, clock_remainder_us;
static int8_t *arena;
static uint8_t *payload;
static int active_node;
static struct pi_device flash,fs,cluster,uart,activity_gpio;
#define DDND_ACTIVITY_LED PI_GPIO_A2_PAD_14_A2
static himax_t camera;
static frame_t frame;
static pi_fs_file_t *weights;
static struct __attribute__((packed)) {
 char magic[4]; uint16_t version,header_bytes;
 uint32_t sequence,capture_us,inference_us,payload_bytes;
 uint16_t depth_h,depth_w;
 uint8_t corner_h,corner_w,corner_c,visibility_count;
 int8_t depth_exp,corner_exp,visibility_exp; uint8_t flags;
 uint32_t model_tag;
} packet;
_Static_assert(sizeof(packet)==40,"DDND header must be40bytes");
_Static_assert(CAMERA_CROP_WIDTH==160 && CAMERA_CROP_HEIGHT==160,"Known camera crop required");
static void fail(void) { pmsis_exit(1); }
static void read_exact(pi_fs_file_t *f,void *dst,unsigned bytes) {
 unsigned done=0;while(done<bytes){int n=pi_fs_read(f,(uint8_t *)dst+done,bytes-done);if(n<=0)fail();done+=n;}
}
static uint32_t crc_update(uint32_t c,const void *data,unsigned n) {
 const uint8_t *p=data;while(n--){c^=*p++;for(int k=0;k<8;k++)c=(c>>1)^((c&1)?0xedb88320u:0);}return c;
}
static void worker(void *arg) {ddnd_execute_node(active_node,arena,payload);}
static void cluster_entry(void *arg) {pi_cl_team_fork(8,worker,NULL);}
static void graph_run(void) {
 for(active_node=1;active_node<DDND_NODE_COUNT;active_node++) {
  unsigned n=ddnd_payload_bytes[active_node];
  if(n){pi_fs_seek(weights,ddnd_payload_offsets[active_node]);read_exact(weights,payload,n);}
  struct pi_cluster_task task;pi_cluster_task(&task,cluster_entry,NULL);
  task.stack_size=2048;task.slave_stack_size=2048;
  if(pi_cluster_send_task_to_cl(&cluster,&task)) fail();
 }
}
static void send_result(void) {
#ifdef DDND_RAW_UART
 uint32_t crc=crc_update(0xffffffffu,&packet,sizeof(packet));
 for(int k=0;k<DDND_OUTPUT_COUNT;k++)crc=crc_update(crc,arena+ddnd_output_offsets[k],ddnd_output_bytes[k]);
 crc^=0xffffffffu;
 pi_uart_write(&uart,&packet,sizeof(packet));
 for(int k=0;k<DDND_OUTPUT_COUNT;k++)pi_uart_write(&uart,arena+ddnd_output_offsets[k],ddnd_output_bytes[k]);
 uart_crc=crc;
 pi_uart_write(&uart,&uart_crc,sizeof(uart_crc));
#else
 ddnd_depthgate(&compact,arena+ddnd_output_offsets[0],arena+ddnd_output_offsets[1],
  arena+ddnd_output_offsets[2],ddnd_output_exps[0],ddnd_output_exps[1],ddnd_output_exps[2],
  capture_ms,(uint16_t)packet.sequence,packet.inference_us,packet.flags&1);
 pi_uart_write(&uart,&compact,sizeof(compact));
#endif
}
static void run(void *arg) {
#ifdef DDND_LIVE_DEBUG
 printf("DDND_LIVE_START\n");
#endif
 ddnd_board_init();
 struct pi_hyperflash_conf flash_conf;pi_hyperflash_conf_init(&flash_conf);
 pi_open_from_conf(&flash,&flash_conf);if(pi_flash_open(&flash))fail();
 struct pi_readfs_conf fs_conf;pi_readfs_conf_init(&fs_conf);fs_conf.fs.flash=&flash;
 pi_open_from_conf(&fs,&fs_conf);if(pi_fs_mount(&fs))fail();
 unsigned buffer_bytes=DDND_PAYLOAD_BYTES;
 unsigned camera_bytes=CAMERA_CAPTURE_WIDTH*CAMERA_CAPTURE_HEIGHT*CAMERA_CAPTURE_BPP;
 if(buffer_bytes<camera_bytes)buffer_bytes=camera_bytes;
 arena=pi_l2_malloc(DDND_ARENA_BYTES);payload=pi_l2_malloc(buffer_bytes);
 if(!arena||!payload)fail();
 weights=pi_fs_open(&fs,"weights.bin",0);if(!weights)fail();
 struct pi_cluster_conf cl_conf;pi_cluster_conf_init(&cl_conf);cl_conf.id=0;
 pi_open_from_conf(&cluster,&cl_conf);if(pi_cluster_open(&cluster))fail();
#ifdef DDND_TILED
 ddnd_workspace=pi_cl_l1_malloc(&cluster,DDND_WORKSPACE_BYTES);
 if(!ddnd_workspace) fail();
#endif
 struct pi_uart_conf uart_conf;pi_uart_conf_init(&uart_conf);
 uart_conf.baudrate_bps=DDND_UART_BAUD;uart_conf.enable_tx=1;uart_conf.enable_rx=0;
 pi_open_from_conf(&uart,&uart_conf);if(pi_uart_open(&uart))fail();
 if(himax_init(&camera))fail();himax_configure(&camera);
 struct pi_gpio_conf led_conf;pi_gpio_conf_init(&led_conf);
 pi_open_from_conf(&activity_gpio,&led_conf);if(pi_gpio_open(&activity_gpio))fail();
 pi_gpio_pin_configure(&activity_gpio,DDND_ACTIVITY_LED,PI_GPIO_OUTPUT);
 pi_gpio_pin_write(&activity_gpio,DDND_ACTIVITY_LED,0);
 frame.buffer=payload;frame.buffer_size=camera_bytes;
 memcpy(packet.magic,"DDN1",4);packet.version=1;packet.header_bytes=40;
 packet.payload_bytes=20480+960+4;packet.depth_h=128;packet.depth_w=160;
 packet.corner_h=8;packet.corner_w=10;packet.corner_c=12;packet.visibility_count=4;
 packet.depth_exp=ddnd_output_exps[0];packet.corner_exp=ddnd_output_exps[1];packet.visibility_exp=ddnd_output_exps[2];
 packet.model_tag=DDND_MODEL_TAG;
#ifdef DDND_LIVE_DEBUG
 printf("DDND_LIVE_READY model=%08x baud=%u arena=%u buffer=%u\n",DDND_MODEL_TAG,DDND_UART_BAUD,DDND_ARENA_BYTES,buffer_bytes);
#endif
 while(1) {
  /* CPI completes before the shared payload buffer becomes layer weights. */
  pi_task_t done;himax_capture_async(&camera,&frame,pi_task_block(&done));himax_start(&camera);
  pi_task_wait_on(&done);himax_stop(&camera);
  packet.capture_us=rt_time_get_us();
  /* Accumulate elapsed time across the 32-bit microsecond timer wrap. */
  uint32_t elapsed=packet.capture_us-clock_last_us;clock_last_us=packet.capture_us;
  capture_ms+=elapsed/1000;clock_remainder_us+=elapsed%1000;
  capture_ms+=clock_remainder_us/1000;clock_remainder_us%=1000;
  for(int y=0;y<128;y++)for(int x=0;x<160;x++){
   unsigned v=payload[(y+16+CAMERA_CROP_TOP)*CAMERA_CAPTURE_WIDTH+x+CAMERA_CROP_LEFT];
   unsigned q=(v*128u+127u)/255u;if(q>127)q=127;
   arena[DDND_INPUT_OFFSET+y*160+x]=(int8_t)q;
  }
#ifdef DDND_LIVE_DEBUG
  uint32_t input_crc=crc_update(0xffffffffu,arena+DDND_INPUT_OFFSET,DDND_INPUT_BYTES)^0xffffffffu;
#endif
  pi_gpio_pin_write(&activity_gpio,DDND_ACTIVITY_LED,1);
  unsigned start=rt_time_get_us();graph_run();packet.inference_us=rt_time_get_us()-start;
  pi_gpio_pin_write(&activity_gpio,DDND_ACTIVITY_LED,0);
  packet.flags=himax_get_i2c_error_count()?0:1;
  send_result();
#ifdef DDND_LIVE_DEBUG
  int lo=127,hi=-127;long sum=0;
  for(unsigned j=0;j<ddnd_output_bytes[0];j++){int v=arena[ddnd_output_offsets[0]+j];if(v<lo)lo=v;if(v>hi)hi=v;sum+=v;}
  printf("DDND_LIVE seq=%u capture_us=%u infer_us=%u input_crc=%08x depth_min=%d depth_max=%d depth_sum=%ld flags=%u\n",packet.sequence,packet.capture_us,packet.inference_us,input_crc,lo,hi,sum,packet.flags);
#endif
  packet.sequence++;
 }
}
int main(void){return pmsis_kickoff((void *)run);}

