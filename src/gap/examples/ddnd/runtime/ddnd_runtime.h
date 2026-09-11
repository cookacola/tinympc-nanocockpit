#ifndef DDND_RUNTIME_H
#define DDND_RUNTIME_H
#include <stdint.h>
typedef struct {int ih,iw,ic,oh,ow,oc,kh,kw,sy,sx,py,px,dy,dx,groups;} ddnd_conv_params;
int8_t ddnd_requant_i64(int64_t value, int shift);
void ddnd_conv_i8(const int8_t *in,int8_t *out,const int8_t *weights,const int32_t *bias,const ddnd_conv_params *p,const int32_t *shifts);
void ddnd_conv_i8_ohwi(const int8_t *in,int8_t *out,const int8_t *weights,const int32_t *bias,const ddnd_conv_params *p,const int32_t *shifts);
void ddnd_conv_i8_ohwi_tiled(const int8_t *in,int8_t *out,const int8_t *weights,const int32_t *bias,const ddnd_conv_params *p,const int32_t *shifts,void *workspace,unsigned workspace_bytes);
void ddnd_prelu_i8(const int8_t *in,int8_t *out,int count,int channels,const int32_t *slopes_q15);
void ddnd_resize_bilinear_i8(const int8_t *in,int8_t *out,int ih,int iw,int oh,int ow,int channels);
void ddnd_concat_i8(const int8_t *const *inputs,int8_t *out,int height,int width,int ninputs,const int *channels,const int32_t *shifts);
void ddnd_sigmoid_lut_i8(const int8_t *in,uint8_t *out,int count,const uint8_t lut[256]);
#endif
