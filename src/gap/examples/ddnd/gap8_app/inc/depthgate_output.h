#ifndef DDND_DEPTHGATE_OUTPUT_H
#define DDND_DEPTHGATE_OUTPUT_H
#include <stdint.h>
#include <math.h>
#include <string.h>
/* Wire ABI shared with TinyMPC depthgate_packet.h: little-endian binary32. */
typedef struct __attribute__((packed)) {
 uint8_t header[4];
 uint32_t source_timestamp_ms;
 uint16_t sequence, status;
 uint32_t inference_us;
 float inverse_depth[3], corners_xy[8], visibility_logits[4];
 uint32_t checksum;
} DDNDDepthGatePacket;
_Static_assert(sizeof(DDNDDepthGatePacket)==80,"DepthGate ABI");
static inline uint32_t ddnd_crc(const void *data,unsigned n) {
 const uint8_t *p=data;uint32_t c=0xffffffffu;
 while(n--){c^=*p++;for(int k=0;k<8;k++)c=(c>>1)^((c&1)?0xedb88320u:0);}
 return c^0xffffffffu;
}
static inline void ddnd_depthgate(DDNDDepthGatePacket *p,
 const int8_t *depth,const int8_t *corners,const int8_t *visibility,
 int de,int ce,int ve,uint32_t capture_ms,uint16_t seq,uint32_t infer,int live) {
 memset(p,0,sizeof(*p));memcpy(p->header,"\x90\x19\x08\x44",4);
 p->source_timestamp_ms=capture_ms;p->sequence=seq;p->status=live?3:2;p->inference_us=infer;
 /* Nearest predicted optical depth over full-height thirds; monotonic sigmoid
  * lets us reduce logits before three floating point sigmoid evaluations. */
 for(int s=0;s<3;s++) {
  int q=-128;
  for(int y=0;y<128;y++)for(int x=s*160/3;x<(s+1)*160/3;x++)
   if(depth[y*160+x]>q)q=depth[y*160+x];
  p->inverse_depth[s]=.01f+9.99f/(1.f+expf(-ldexpf((float)q,de)));
 }
 for(int k=0;k<4;k++) {
  int best=0;
  for(int i=1;i<80;i++)if(corners[i*12+k]>corners[best*12+k])best=i;
  float dx=fminf(1.f,fmaxf(0.f,ldexpf(corners[best*12+4+2*k],ce)));
  float dy=fminf(1.f,fmaxf(0.f,ldexpf(corners[best*12+5+2*k],ce)));
  p->corners_xy[k*2]=fminf(159.f,(best%10+dx)*16.f);
  p->corners_xy[k*2+1]=fminf(127.f,(best/10+dy)*16.f);
  p->visibility_logits[k]=ldexpf(visibility[k],ve)-8.f;
 }
 p->checksum=ddnd_crc(p,76);
}
#endif
