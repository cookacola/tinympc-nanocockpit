#include "camera.h"
#include "cluster.h"
#include "coroutine.h"
#include "soc.h"
#include "trace.h"
#include "uart.h"
#include "uart_protocol.h"
#include "crc32.h"
#include "mem.h"
#include "depthgate_postprocess.h"
#include "stdc_encoder_network.h"
#include "stdc_sector_head_network.h"
#include "stdc_corner_head_network.h"
#include "stdc_visibility_head_network.h"
#include <string.h>
#include <math.h>
static uart_t uart;
static uart_protocol_t protocol;
static camera_t camera;
static struct pi_device cluster;
static state_msg_t latest_state;
static uint8_t *arena;
static uint8_t shared[7680], sector[3], corners[960], visibility[4];
static struct __attribute__((packed)) {
 uint8_t header[4]; uint32_t timestamp; uint16_t sequence,status; uint32_t inference_us;
 float inverse[3],xy[8],logits[4]; uint32_t crc;
} packet;
_Static_assert(sizeof(packet)==80,"DepthGate wire size");
CO_FN_BEGIN(uart_callback, uart_msg_t *, message) {
 if(!memcmp(message->header,UART_STATE_MSG_HEADER,UART_HEADER_LENGTH))latest_state=message->state;
} CO_FN_END()
CO_FN_BEGIN(camera_callback, frame_t *, frame) {
 static co_event_t done;
 static uint32_t start;
 static depthgate_result decoded;
 start=pi_time_get_us();
 packet.timestamp=frame->frame_timestamp/1000;
 packet.status=3; /* valid, GAP-local capture timestamp */
 packet.sequence=(uint16_t)frame->sequence_id;
 /* Same center crop as training: rows16..143, all160columns. */
 memcpy(arena,frame->buffer+16*160,128*160);
 stdc_encoder_network_run_async_cl(arena,260000,shared,0,1,&cluster,co_event_init(&done)); CO_WAIT(&done);
 memcpy(arena,shared,7680);
 stdc_sector_head_network_run_async_cl(arena,260000,sector,0,1,&cluster,co_event_init(&done)); CO_WAIT(&done);
 memcpy(arena,shared,7680);
 stdc_corner_head_network_run_async_cl(arena,260000,corners,0,1,&cluster,co_event_init(&done)); CO_WAIT(&done);
 memcpy(arena,shared,7680);
 stdc_visibility_head_network_run_async_cl(arena,260000,visibility,0,1,&cluster,co_event_init(&done)); CO_WAIT(&done);
 packet.inference_us=pi_time_get_us()-start;
 if(depthgate_decode(sector,corners,visibility,8.237687870860e-03f,6.930632051080e-03f,7.458378374577e-02f,&decoded))packet.status &= ~1u;
 for(int i=0;i<3;i++)packet.inverse[i]=sector[i]*8.237687870860e-03f;
 for(int i=0;i<4;i++) {
  packet.xy[2*i]=decoded.corner_x[i];packet.xy[2*i+1]=decoded.corner_y[i];
  packet.logits[i]=visibility[i]*7.458378374577e-02f-8.0f;
 }
 packet.header[0]=0x90;packet.header[1]=0x19;packet.header[2]=0x08;packet.header[3]=0x44;
 packet.crc=crc32CalculateBuffer(&packet,76);
 uart_write_async(&uart,&packet,sizeof(packet),co_event_init(&done));CO_WAIT(&done);
} CO_FN_END()
static void application(void) {
 soc_init();mem_init();
 uart_init(&uart);uart_protocol_init(&protocol,&uart,uart_callback);
 camera_init(&camera,camera_callback);camera_init_frames_alloc(&camera);cluster_init(&cluster);
 arena=pi_l2_malloc(260000);if(!arena){printf("DG ARENA_ALLOCATION_FAILED\n");pmsis_exit(1);}
 stdc_encoder_network_initialize();stdc_sector_head_network_initialize();
 stdc_corner_head_network_initialize();stdc_visibility_head_network_initialize();
 trace_init();uart_protocol_start(&protocol);
 camera_start(&camera);
 while(1){camera_watchdog_poll(&camera);pi_time_wait_us(1000);}

}
int main(void){return pmsis_kickoff((void*)application);}
