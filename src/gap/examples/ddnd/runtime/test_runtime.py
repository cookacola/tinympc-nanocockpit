import ctypes as C
import pathlib,subprocess,numpy as np
p=pathlib.Path(__file__).parent
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-O2','-shared','-fPIC',str(p/'ddnd_runtime.c'),'-o',str(p/'libddnd.so')],check=True)
lib=C.CDLL(str(p/'libddnd.so'))
ptr=lambda a:a.ctypes.data_as(C.c_void_p)
class Params(C.Structure):
 _fields_=[(k,C.c_int) for k in 'ih iw ic oh ow oc kh kw sy sx py px dy dx groups'.split()]
def rq(v,s):
 v=int(v)
 q=((abs(v)+(1<<(s-1)))>>s) if s>0 else abs(v)<<(-s)
 return max(-127,min(127,(-q if v<0 else q)))
lib.ddnd_requant_i64.argtypes=[C.c_int64,C.c_int]
lib.ddnd_requant_i64.restype=C.c_int8
for v in [-100000,-129,-127,-5,-3,-1,0,1,3,5,127,129,100000]:
 for s in range(-4,36):assert lib.ddnd_requant_i64(v,s)==rq(v,s)
rng=np.random.default_rng(42)
for groups,ic,oc,kh,dy,sy in [(1,3,8,3,1,1),(4,4,4,3,2,1),(2,4,6,3,1,2),(1,8,4,1,1,1),(2,10,6,3,1,1),(1,104,4,3,1,1)]:
 ih,iw=7,9;pad=dy*(kh-1)//2;oh=(ih+2*pad-dy*(kh-1)-1)//sy+1;ow=(iw+2*pad-dy*(kh-1)-1)//sy+1
 a=rng.integers(-127,128,(ih,iw,ic),dtype=np.int8)
 w=rng.integers(-20,21,(oc,ic//groups,kh,kh),dtype=np.int8)
 bias=rng.integers(-200,201,oc,dtype=np.int32)
 shifts=rng.integers(5,12,oc,dtype=np.int32);out=np.zeros((oh,ow,oc),np.int8);ref=out.copy()
 pars=Params(ih,iw,ic,oh,ow,oc,kh,kh,sy,sy,pad,pad,dy,dy,groups)
 lib.ddnd_conv_i8(ptr(a),ptr(out),ptr(w),ptr(bias),C.byref(pars),ptr(shifts))
 for y in range(oh):
  for x in range(ow):
   for o in range(oc):
    acc=int(bias[o]);g=o//(oc//groups)
    for c in range(ic//groups):
     for ky in range(kh):
      for kx in range(kh):
       iy=y*sy-pad+ky*dy;ix=x*sy-pad+kx*dy
       if 0<=iy<ih and 0<=ix<iw:acc+=int(a[iy,ix,g*(ic//groups)+c])*int(w[o,c,ky,kx])
    ref[y,x,o]=rq(acc,int(shifts[o]))
 assert np.array_equal(out,ref),(groups,'conv')
 wh=np.ascontiguousarray(w.transpose(0,2,3,1));out2=out.copy()
 lib.ddnd_conv_i8_ohwi(ptr(a),ptr(out2),ptr(wh),ptr(bias),C.byref(pars),ptr(shifts))
 assert np.array_equal(out2,ref),(groups,'OHWI conv')
 for workspace_bytes in [49152,4]:
  ws=np.zeros(49152,np.uint8);out3=out.copy()
  lib.ddnd_conv_i8_ohwi_tiled(ptr(a),ptr(out3),ptr(wh),ptr(bias),C.byref(pars),ptr(shifts),ptr(ws),workspace_bytes)
  assert np.array_equal(out3,ref),(groups,'tiled',workspace_bytes)
 bias[:]=np.array([2147483647 if o%2 else -2147483648 for o in range(oc)],np.int32)
 lib.ddnd_conv_i8(ptr(a),ptr(out),ptr(w),ptr(bias),C.byref(pars),ptr(shifts))
 lib.ddnd_conv_i8_ohwi(ptr(a),ptr(out2),ptr(wh),ptr(bias),C.byref(pars),ptr(shifts))
 assert np.array_equal(out2,out),(groups,'OHWI int64 fallback')
for ih,iw,oh,ow in [(3,4,7,9),(1,4,1,9),(3,4,1,1),(7,9,3,4)]:
 a=rng.integers(-127,128,(ih,iw,3),dtype=np.int8);out=np.zeros((oh,ow,3),np.int8);ref=out.copy()
 lib.ddnd_resize_bilinear_i8(ptr(a),ptr(out),ih,iw,oh,ow,3)
 for y in range(oh):
  for x in range(ow):
   fy=y*(ih-1)*65536//(oh-1) if oh>1 else 0;fx=x*(iw-1)*65536//(ow-1) if ow>1 else 0
   y0=fy//65536;x0=fx//65536;wy=fy%65536;wx=fx%65536
   for c in range(3):
    val=0
    for yy,weighty in [(y0,65536-wy),(min(y0+1,ih-1),wy)]:
     for xx,weightx in [(x0,65536-wx),(min(x0+1,iw-1),wx)]:val+=int(a[yy,xx,c])*weighty*weightx
    ref[y,x,c]=rq(val,32)
 assert np.array_equal(out,ref),'resize'
a=rng.integers(-127,128,(100,3),dtype=np.int8);o=a.copy();s=np.array([8192,16384,40000],np.int32)
lib.ddnd_prelu_i8(ptr(a),ptr(o),a.size,3,ptr(s))
assert np.array_equal(o,np.array([[v if v>=0 else rq(int(v)*int(s[c]),15) for c,v in enumerate(row)] for row in a],np.int8))
a=rng.integers(-127,128,(3,4,2),dtype=np.int8);b=rng.integers(-127,128,(3,4,3),dtype=np.int8);o=np.zeros((3,4,5),np.int8)
ins=(C.c_void_p*2)(a.ctypes.data,b.ctypes.data);cs=np.array([2,3],np.int32);sh=np.array([1,-1],np.int32)
lib.ddnd_concat_i8(ins,ptr(o),3,4,2,ptr(cs),ptr(sh))
ref=np.concatenate((np.vectorize(lambda v:rq(v,1))(a),np.vectorize(lambda v:rq(v,-1))(b)),axis=2).astype(np.int8)
assert np.array_equal(o,ref)
lut=np.arange(256,dtype=np.uint8);o=np.zeros(a.shape,np.uint8)
lib.ddnd_sigmoid_lut_i8(ptr(a),ptr(o),a.size,ptr(lut))
assert np.array_equal(o,a.astype(np.int16)+128)
print('PASS: requant, standard/grouped/depthwise/dilated/strided conv, align-corners resize, PReLU, concat, LUT')

