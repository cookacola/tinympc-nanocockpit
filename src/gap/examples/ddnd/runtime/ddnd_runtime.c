#include "ddnd_runtime.h"
#include <limits.h>
#ifdef DDND_GAP8
#include "pmsis.h"
#define CORE_ID ((int)pi_core_id())
#define CORE_COUNT ((int)pi_cl_cluster_nb_cores())
#define BARRIER() pi_cl_team_barrier()
#else
#define CORE_ID 0
#define CORE_COUNT 1
#define BARRIER() ((void)0)
#endif
/* All kernels require distinct input/output except PReLU/LUT, and all cluster
 * cores must call them in the same order. Activations HWC, weights OIHW.
 * Rounding is nearest with ties away from zero, including negative values. */
static int64_t rounded_shift(int64_t x,int shift) {
 if(shift<=0) {
  if(x==0) return 0;
  if(shift < -55) return x<0?INT64_MIN:INT64_MAX;
  int64_t m=(int64_t)1<<(-shift);
  if(x>INT64_MAX/m)return INT64_MAX;
  if(x<INT64_MIN/m)return INT64_MIN;
  return x*m;
 }
 if(shift>63)return 0;
 uint64_t a=x<0?(uint64_t)(-(x+1))+1:(uint64_t)x;
 uint64_t q=(a>>shift)+((a&(((uint64_t)1<<shift)-1))>=((uint64_t)1<<(shift-1)));
 return x<0?-(int64_t)q:(int64_t)q;
}
int8_t ddnd_requant_i64(int64_t x,int shift) {
 x=rounded_shift(x,shift);
 return (int8_t)(x>127?127:x< -127?-127:x);
}
void ddnd_conv_i8(const int8_t *in,int8_t *out,const int8_t *w,const int32_t *bias,const ddnd_conv_params *p,const int32_t *shifts) {
 int n=p->oh*p->ow*p->oc, ipg=p->ic/p->groups, opg=p->oc/p->groups;
 for(int idx=CORE_ID;idx<n;idx+=CORE_COUNT) {
  int oc=idx%p->oc, ox=(idx/p->oc)%p->ow, oy=idx/(p->oc*p->ow);
  int group=oc/opg;
  int64_t sum=bias?bias[oc]:0;
  for(int ic=0;ic<ipg;ic++)for(int ky=0;ky<p->kh;ky++)for(int kx=0;kx<p->kw;kx++) {
   int iy=oy*p->sy-p->py+ky*p->dy, ix=ox*p->sx-p->px+kx*p->dx;
   if(iy>=0&&iy<p->ih&&ix>=0&&ix<p->iw) {
    int wi=((oc*ipg+ic)*p->kh+ky)*p->kw+kx;
    sum+=(int32_t)in[(iy*p->iw+ix)*p->ic+group*ipg+ic]*(int32_t)w[wi];
   }
  }
  out[idx]=ddnd_requant_i64(sum,shifts[oc]);
 }
 BARRIER();
}
void ddnd_prelu_i8(const int8_t *in,int8_t *out,int count,int channels,const int32_t *slopes) {
 for(int i=CORE_ID;i<count;i+=CORE_COUNT)out[i]=in[i]>=0?in[i]:ddnd_requant_i64((int64_t)in[i]*slopes[i%channels],15);
 BARRIER();
}
void ddnd_resize_bilinear_i8(const int8_t *in,int8_t *out,int ih,int iw,int oh,int ow,int c) {
 for(int idx=CORE_ID;idx<oh*ow*c;idx+=CORE_COUNT) {
  int ch=idx%c,x=(idx/c)%ow,y=idx/(c*ow);
  int64_t fy=oh>1?((int64_t)y*(ih-1)*65536)/(oh-1):0;
  int64_t fx=ow>1?((int64_t)x*(iw-1)*65536)/(ow-1):0;
  int y0=fy>>16,x0=fx>>16,y1=y0+1<ih?y0+1:y0,x1=x0+1<iw?x0+1:x0;
  int64_t wy=fy&65535,wx=fx&65535;
  int64_t sum=(int64_t)in[(y0*iw+x0)*c+ch]*(65536-wy)*(65536-wx)
   +(int64_t)in[(y0*iw+x1)*c+ch]*(65536-wy)*wx
   +(int64_t)in[(y1*iw+x0)*c+ch]*wy*(65536-wx)
   +(int64_t)in[(y1*iw+x1)*c+ch]*wy*wx;
  out[idx]=ddnd_requant_i64(sum,32);
 }
 BARRIER();
}
void ddnd_concat_i8(const int8_t *const *inputs,int8_t *out,int h,int w,int n,const int *cs,const int32_t *shifts) {
 int total=0;for(int j=0;j<n;j++)total+=cs[j];
 for(int pix=CORE_ID;pix<h*w;pix+=CORE_COUNT) {
  int offset=0;
  for(int j=0;j<n;j++){for(int c=0;c<cs[j];c++)out[pix*total+offset+c]=ddnd_requant_i64(inputs[j][pix*cs[j]+c],shifts[j]);offset+=cs[j];}
 }
 BARRIER();
}
void ddnd_sigmoid_lut_i8(const int8_t *in,uint8_t *out,int n,const uint8_t lut[256]) {
 for(int i=CORE_ID;i<n;i+=CORE_COUNT)out[i]=lut[(int)in[i]+128];
 BARRIER();
}

/* Optimized OHWI path. Tensor value contract excludes -128. Per-output bound
 * protects every partial sum, not only the final sum, from signed overflow. */
#ifdef DDND_GAP8
typedef signed char ddnd_v4s __attribute__((vector_size(4)));
static inline int32_t ddnd_dot4(const int8_t *a,const int8_t *b,int32_t acc) {
 ddnd_v4s av,bv;
 /* memcpy permits arbitrary HWC/group alignment without aliasing UB. */
 __builtin_memcpy(&av,a,4);__builtin_memcpy(&bv,b,4);
 return __builtin_pulp_sdotsp4(av,bv,acc);
}
#else
static inline int32_t ddnd_dot4(const int8_t *a,const int8_t *b,int32_t acc) {
 for(int j=0;j<4;j++)acc+=(int32_t)a[j]*(int32_t)b[j];
 return acc;
}
#endif
void ddnd_conv_i8_ohwi(const int8_t *in,int8_t *out,const int8_t *w,const int32_t *bias,const ddnd_conv_params *p,const int32_t *shifts) {
 int n=p->oh*p->ow*p->oc,ipg=p->ic/p->groups,opg=p->oc/p->groups;
 int64_t mac_bound=(int64_t)ipg*p->kh*p->kw*16129;
 for(int idx=CORE_ID;idx<n;idx+=CORE_COUNT) {
  int oc=idx%p->oc,ox=(idx/p->oc)%p->ow,oy=idx/(p->oc*p->ow),group=oc/opg;
  int32_t b=bias?bias[oc]:0;
  int64_t ab=b<0?-(int64_t)b:(int64_t)b;
  if(ab+mac_bound<=INT32_MAX) {
   int32_t sum=b;
   for(int ky=0;ky<p->kh;ky++) {
    int iy=oy*p->sy-p->py+ky*p->dy;
    if(iy<0||iy>=p->ih)continue;
    for(int kx=0;kx<p->kw;kx++) {
     int ix=ox*p->sx-p->px+kx*p->dx;
     if(ix<0||ix>=p->iw)continue;
     const int8_t *a=in+(iy*p->iw+ix)*p->ic+group*ipg;
     const int8_t *ww=w+((oc*p->kh+ky)*p->kw+kx)*ipg;
     int ic=0;
     for(;ic+3<ipg;ic+=4)sum=ddnd_dot4(a+ic,ww+ic,sum);
     for(;ic<ipg;ic++)sum+=(int32_t)a[ic]*(int32_t)ww[ic];
    }
   }
   out[idx]=ddnd_requant_i64(sum,shifts[oc]);
  } else {
   int64_t sum=b;
   for(int ky=0;ky<p->kh;ky++)for(int kx=0;kx<p->kw;kx++) {
    int iy=oy*p->sy-p->py+ky*p->dy,ix=ox*p->sx-p->px+kx*p->dx;
    if(iy<0||iy>=p->ih||ix<0||ix>=p->iw)continue;
    const int8_t *a=in+(iy*p->iw+ix)*p->ic+group*ipg;
    const int8_t *ww=w+((oc*p->kh+ky)*p->kw+kx)*ipg;
    for(int ic=0;ic<ipg;ic++)sum+=(int32_t)a[ic]*(int32_t)ww[ic];
   }
   out[idx]=ddnd_requant_i64(sum,shifts[oc]);
  }
 }
 BARRIER();
}

/* L1 row tiling. Workspace must be shared by all participating cores and
 * 4-byte aligned. Copies are cooperative; weights persist across output rows.
 * Full effective dilated row span is copied, including unused interior rows.
 * If any allocation cannot fit, use the direct OHWI kernel unchanged. */
void ddnd_conv_i8_ohwi_tiled(const int8_t *in,int8_t *out,const int8_t *w,const int32_t *bias,const ddnd_conv_params *p,const int32_t *shifts,void *workspace,unsigned workspace_bytes) {
 int weight_bytes=p->oc*p->kh*p->kw*(p->ic/p->groups);
 unsigned weight_aligned=((unsigned)weight_bytes+3u)&~3u;
 int max_rows=(p->kh-1)*p->dy+1;
 if(max_rows>p->ih)max_rows=p->ih;
 unsigned input_bytes=(unsigned)max_rows*p->iw*p->ic;
 unsigned output_bytes=(unsigned)p->ow*p->oc;
 uint64_t required=(uint64_t)weight_aligned+8u*p->oc+input_bytes+output_bytes;
 if(!workspace||((uintptr_t)workspace&3u)||required>workspace_bytes) {
  ddnd_conv_i8_ohwi(in,out,w,bias,p,shifts);
  return;
 }
 int8_t *wl=(int8_t*)workspace;
 int32_t *bl=(int32_t*)(wl+weight_aligned);
 int32_t *sl=bl+p->oc;
 int8_t *il=(int8_t*)(sl+p->oc);
 int8_t *ol=il+input_bytes;
 for(int i=CORE_ID;i<weight_bytes;i+=CORE_COUNT)wl[i]=w[i];
 for(int i=CORE_ID;i<p->oc;i+=CORE_COUNT){bl[i]=bias?bias[i]:0;sl[i]=shifts[i];}
 BARRIER();
 for(int oy=0;oy<p->oh;oy++) {
  int first=oy*p->sy-p->py;
  int end=first+(p->kh-1)*p->dy+1;
  if(first<0)first=0;
  if(first>p->ih)first=p->ih;
  if(end<first)end=first;
  if(end>p->ih)end=p->ih;
  int bytes=(end-first)*p->iw*p->ic;
  const int8_t *source=in+first*p->iw*p->ic;
  for(int i=CORE_ID;i<bytes;i+=CORE_COUNT)il[i]=source[i];
  BARRIER();
  ddnd_conv_params row=*p;
  row.ih=end-first;row.oh=1;row.py=p->py-oy*p->sy+first;
  ddnd_conv_i8_ohwi(il,ol,wl,bl,&row,sl);
  int8_t *dest=out+oy*p->ow*p->oc;
  for(unsigned i=CORE_ID;i<output_bytes;i+=CORE_COUNT)dest[i]=ol[i];
  BARRIER();
 }
}
