import argparse,json,ctypes as C,math
from pathlib import Path
import numpy as np
import torch
from torch.nn import functional as F
from baseline import DDNDGate
from integer_graph import GraphNet
from depth_labels import student_image
from gate_data import GateDataset

class Params(C.Structure):
 _fields_=[(k,C.c_int) for k in 'ih iw ic oh ow oc kh kw sy sx py px dy dx groups'.split()]
def ptr(a):return a.ctypes.data_as(C.c_void_p)
def rq(a,shift):
 a=np.asarray(a,dtype=np.int64);s=np.asarray(shift,dtype=np.int64)
 v=np.where(s>0,np.right_shift(np.abs(a)+np.left_shift(1,np.maximum(s-1,0)),np.maximum(s,0)),np.left_shift(np.abs(a),np.maximum(-s,0)))
 return np.clip(np.sign(a)*v,-127,127).astype(np.int8)
def run_integer(dest,x,save_outputs=True):
 dest=Path(dest);g=json.loads((dest/'graph.json').read_text());b=(dest/'weights.bin').read_bytes()
 lib=C.CDLL(str(Path(__file__).resolve().parents[1]/'runtime/libddnd.so'))
 def arr(off,dtype,count):return np.frombuffer(b,dtype=dtype,count=count,offset=off).copy()
 vals=[np.ascontiguousarray(x)];errs=[]
 for idx,n in enumerate(g['nodes'][1:],1):
  a=vals[n['inputs'][0]];out=np.zeros(n['shape'],np.int8);op=n['op']
  if op=='conv':
   p=n['params'];packed=n.get('weights_layout')=='OHWI'
   wb=arr(n['weights_offset'],np.int8,n['weights_bytes'])
   w=wb.reshape(p['oc'],p['kh'],p['kw'],p['ic']//p['groups']).transpose(0,3,1,2) if packed else wb.reshape(p['oc'],p['ic']//p['groups'],p['kh'],p['kw'])
   bias=arr(n['bias_offset'],'<i4',p['oc']);sh=arr(n['shifts_offset'],'<i4',p['oc'])
   pars=Params(**p);kernel=lib.ddnd_conv_i8_ohwi if packed else lib.ddnd_conv_i8
   kernel(ptr(a),ptr(out),ptr(wb),ptr(bias),C.byref(pars),ptr(sh))
   v=F.conv2d(torch.tensor(a.transpose(2,0,1)[None],dtype=torch.float64),torch.tensor(w,dtype=torch.float64),torch.tensor(bias,dtype=torch.float64),stride=(p['sy'],p['sx']),padding=(p['py'],p['px']),dilation=(p['dy'],p['dx']),groups=p['groups'])[0].numpy().transpose(1,2,0)
   ref=rq(v.astype(np.int64),sh)
  elif op=='relu':out[:]=np.maximum(a,0);ref=np.maximum(a,0)
  elif op=='prelu':
   slope=arr(n['slope_offset'],'<i4',out.shape[-1]);lib.ddnd_prelu_i8(ptr(a),ptr(out),a.size,a.shape[-1],ptr(slope))
   ref=np.where(a>=0,a,rq(a.astype(np.int64)*slope,15))
  elif op=='resize':
   ih,iw,c=a.shape;oh,ow,_=out.shape;lib.ddnd_resize_bilinear_i8(ptr(a),ptr(out),ih,iw,oh,ow,c)
   yy=np.arange(oh,dtype=np.int64)*(ih-1)*65536//max(oh-1,1);xx=np.arange(ow,dtype=np.int64)*(iw-1)*65536//max(ow-1,1)
   y0=yy//65536;x0=xx//65536;wy=(yy%65536)[:,None,None];wx=(xx%65536)[None,:,None]
   a=a.astype(np.int64)
   v=a[y0[:,None],x0[None,:]]*(65536-wy)*(65536-wx)+a[np.minimum(y0+1,ih-1)[:,None],x0[None,:]]*wy*(65536-wx)+a[y0[:,None],np.minimum(x0+1,iw-1)[None,:]]*(65536-wy)*wx+a[np.minimum(y0+1,ih-1)[:,None],np.minimum(x0+1,iw-1)[None,:]]*wy*wx
   ref=rq(v,32)
  elif op=='concat':
   ins=[vals[i] for i in n['inputs']];cs=np.array([v.shape[-1] for v in ins],np.int32);sh=arr(n['shifts_offset'],'<i4',len(ins));ps=(C.c_void_p*len(ins))(*[v.ctypes.data for v in ins])
   lib.ddnd_concat_i8(ps,ptr(out),out.shape[0],out.shape[1],len(ins),ptr(cs),ptr(sh));ref=np.concatenate([rq(v,int(s)) for v,s in zip(ins,sh)],axis=2)
  else:raise ValueError(op)
  assert np.array_equal(out,ref),(idx,n['name'],np.max(np.abs(out.astype(int)-ref.astype(int))))
  vals.append(out)
 if save_outputs:
  for name,idx in g['outputs'].items():(dest/f'golden_{name}.bin').write_bytes(vals[idx].tobytes())
  (dest/'input.bin').write_bytes(x.tobytes())
 return vals

def main():
 p=argparse.ArgumentParser();p.add_argument('--checkpoint');p.add_argument('--output',default='artifacts/export');p.add_argument('--calibration',type=int,default=32);p.add_argument('--qat-checkpoint');a=p.parse_args()
 torch.set_num_threads(2);m=DDNDGate().eval()
 if a.checkpoint:
  ck=torch.load(a.checkpoint,map_location='cpu',weights_only=False);m.load_state_dict(ck['model'],strict=True)
 else:m.load_depth_checkpoint('/home/cchen/ddnd-replication-20260906/artifacts/student_isaac/best.pt')
 g=GraphNet(m).eval();rows=[json.loads(s) for s in Path('manifests/dronetv3_train.jsonl').read_text().splitlines()]
 rows=[rows[i] for i in np.linspace(0,len(rows)-1,a.calibration,dtype=int)]
 g.calibrate=True
 with torch.no_grad():
  for r in rows:g(student_image(r)[None])
  gate=GateDataset("/home/cchen/depthgate-20260906/manifests/gates_10cm.jsonl",split="train")
  gate_indices=np.linspace(0,len(gate)-1,a.calibration,dtype=int)
  for i in gate_indices:g(gate[int(i)]["image"][None])
 g.freeze_scales()
 if a.qat_checkpoint:
  ck=torch.load(a.qat_checkpoint,map_location='cpu',weights_only=False);g.load_state_dict(ck['model']);g.nodes=ck['nodes'];g.outputs=ck['outputs']
 x=student_image(rows[0])[None]
 with torch.no_grad():
  diff=float((g(x)['disparity']-m(x)['disparity']).abs().max())
 if not a.qat_checkpoint:assert diff<1e-5,diff
 report=g.export(a.output)
 inp=torch.clamp(torch.sign(x)*torch.floor(torch.abs(x)*128+.5),-127,127)[0].permute(1,2,0).numpy().astype(np.int8)
 vals=run_integer(a.output,inp)
 gate_index=next(i for i,r in enumerate(gate.rows) if sum(r['visibility'])==4)
 gx=gate[gate_index]['image'][None]
 gate_input=torch.clamp(torch.sign(gx)*torch.floor(torch.abs(gx)*128+.5),-127,127)[0].permute(1,2,0).numpy().astype(np.int8)
 gate_vals=run_integer(a.output,gate_input,save_outputs=False)
 (Path(a.output)/'input_gate.bin').write_bytes(gate_input.tobytes())
 for name,idx in report['outputs'].items():(Path(a.output)/f'golden_gate_{name}.bin').write_bytes(gate_vals[idx].tobytes())
 with torch.no_grad():
  outputs=g(x);g.qat=True;qoutputs=g(x)
 report.update(status='host_integer_all_nodes_bit_exact',float_graph_max_error=None if a.qat_checkpoint else diff,gate_fixture_image=gate.rows[gate_index]["image"],fixture_count=2,calibration_ids=[r['id'] for r in rows],gate_calibration_images=[gate.rows[int(i)]['image'] for i in gate_indices],qat_vs_float_disparity_mae=float((qoutputs['disparity']-outputs['disparity']).abs().mean()),gap8_verified=False)
 (Path(a.output)/'report.json').write_text(json.dumps(report,indent=2))
 torch.save({'model':g.state_dict(),'nodes':g.nodes,'outputs':g.outputs},Path(a.output)/'qat_init.pt')
 print(json.dumps({k:report[k] for k in ('status','arena_bytes','max_node_weight_bytes','weights_bytes','float_graph_max_error','qat_vs_float_disparity_mae')},indent=2))
if __name__=='__main__':main()
