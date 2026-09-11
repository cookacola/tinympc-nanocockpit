"""Explicit faithful DDND graph, signed power-of-two fake quantization/export.
No ONNX importer silently drops operators. C kernels execute every listed node.
"""
import copy,math,json,ctypes,subprocess
from pathlib import Path
import numpy as np
import torch
from torch import nn
from torch.nn import functional as F
from torch.nn.utils.fusion import fuse_conv_bn_eval
from baseline import DDNDGate

def round_away(x):return torch.sign(x)*torch.floor(torch.abs(x)+.5)
def fake(x,eps):
    q=round_away(x/eps).clamp(-127,127)*eps
    return x+(q-x).detach()
def exponent(x):
    v=float(x.detach().abs().max())
    return math.ceil(math.log2(max(v/127,2**-24)))

class GraphNet(nn.Module):
    def __init__(self,model):
        super().__init__();model=copy.deepcopy(model).eval()
        self.layers=nn.ModuleList();self.nodes=[dict(name='input',op='input',inputs=[],shape=[128,160,1],scale_exp=-7)]
        self.qat=False;self.calibrate=False
        def add(name,op,inputs,module=None,**kw):
            n=dict(name=name,op=op,inputs=inputs,**kw)
            if module is not None:n['module']=len(self.layers);self.layers.append(module)
            self.nodes.append(n);return len(self.nodes)-1
        def conv(name,i,c,bn=None,act=None):
            c=copy.deepcopy(c)
            if bn is not None:c=fuse_conv_bn_eval(c,copy.deepcopy(bn))
            i=add(name,'conv',[i],c)
            if isinstance(act,nn.ReLU):i=add(name+'.relu','relu',[i])
            elif isinstance(act,nn.PReLU):i=add(name+'.prelu','prelu',[i],copy.deepcopy(act))
            elif act is not None:raise ValueError(type(act))
            return i
        def seq(name,i,s):
            return conv(name,i,s[0],s[1],s[2])
        d=model.depth;i=0
        for k,c in enumerate(d.downsample_layers[0]):
            i=conv('stem'+str(k),i,c.conv,c.bn_gelu.bn,c.bn_gelu.act)
        i=conv('stem2',i,d.stem2[0].conv)
        features=[]
        for k,stage in enumerate(d.stages):
            if k:i=conv('down'+str(k),i,d.downsample_layers[k][0].conv)
            for j,c in enumerate(stage):i=conv(f'cdc{k}_{j}',i,c.ddwconv.conv,c.bn1)
            features.append(i)
        deepest=i
        for k in (2,1,0):
            c=d.decoder.convs[('upconv',k,0)];i=conv(f'up{k}_0',i,c.conv.conv,act=c.nonlin)
            size=[16,20] if k==2 else [32,40] if k==1 else [64,80]
            i=add(f'resize{k}','resize',[i],size=size)
            if k:i=add(f'skip{k}','concat',[i,features[k-1]])
            c=d.decoder.convs[('upconv',k,1)];i=conv(f'up{k}_1',i,c.conv.conv,act=c.nonlin)
        i=conv('disp',i,d.decoder.convs[('dispconv',0)].conv)
        depth=add('depth_logits','resize',[i],size=[128,160])
        g=seq('gate_adapter',deepest,model.gate_adapter)
        c=g
        for j,s in enumerate(model.corner_head.refine):c=seq('corner'+str(j),c,s)
        c=seq('corner_output',c,model.corner_head.output)
        v=seq('visibility_compress',g,model.visibility_head.compress)
        v=seq('visibility_spatial',v,model.visibility_head.spatial)
        v=seq('visibility_output',v,model.visibility_head.output)
        self.outputs=dict(depth_logits=depth,corners=c,visibility=v)
    def run(self,x):
        vals=[fake(x,2.**self.nodes[0]['scale_exp']) if self.qat else x]
        for n in self.nodes[1:]:
            a=vals[n['inputs'][0]];op=n['op']
            if op=='conv':
                m=self.layers[n['module']]
                if self.qat:
                    we=torch.tensor(n['weight_exps'],device=a.device,dtype=a.dtype).exp2().view(-1,1,1,1)
                    w=fake(m.weight,we);be=(we.flatten()*2.**self.nodes[n['inputs'][0]]['scale_exp'])
                    b=m.bias
                    if b is not None:b=b+(round_away(b/be)*be-b).detach()
                    y=F.conv2d(a,w,b,m.stride,m.padding,m.dilation,m.groups)
                else:y=m(a)
            elif op=='prelu':
                m=self.layers[n['module']];w=m.weight
                if self.qat:w=w+(round_away(w*32768)/32768-w).detach()
                y=F.prelu(a,w)
            elif op=='relu':y=F.relu(a)
            elif op=='resize':y=F.interpolate(a,size=n['size'],mode='bilinear',align_corners=True)
            elif op=='concat':y=torch.cat([vals[i] for i in n['inputs']],1)
            else:raise ValueError(op)
            n['shape']=list(y.shape[2:])+[y.shape[1]]
            if self.calibrate:
                n['scale_exp']=max(n.get('scale_exp',-100),exponent(y))
            if self.qat:y=fake(y,2.**n['scale_exp'])
            vals.append(y)
        return vals
    def forward(self,x):
        v=self.run(x);c=v[self.outputs['corners']]
        return dict(disparity=torch.sigmoid(v[self.outputs['depth_logits']]),corner_heatmaps=c[:,:4],corner_offsets=c[:,4:],visibility_logits=v[self.outputs['visibility']].flatten(1)-8.)
    def freeze_scales(self):
        for n in self.nodes[1:]:
            if n['op'] in ('relu','prelu','resize'):n['scale_exp']=self.nodes[n['inputs'][0]]['scale_exp']
            if n['op']=='concat':n['scale_exp']=max(self.nodes[i]['scale_exp'] for i in n['inputs'])
            if n['op']=='conv':
                w=self.layers[n['module']].weight
                n['weight_exps']=[exponent(w[k]) for k in range(w.shape[0])]
        self.calibrate=False
    def export(self,dest):
        dest=Path(dest);dest.mkdir(parents=True,exist_ok=True)
        nodes=copy.deepcopy(self.nodes);blob=bytearray()
        def array(a):
            while len(blob)%4:blob.append(0)
            off=len(blob);blob.extend(a.tobytes());return off
        for n in nodes[1:]:
            start=len(blob)
            if n['op']=='conv':
                m=self.layers[n['module']];we=np.exp2(n['weight_exps'])
                w=m.weight.detach().cpu().numpy()/we[:,None,None,None]
                w=np.clip(np.sign(w)*np.floor(np.abs(w)+.5),-127,127).astype(np.int8)
                n['weights_offset']=array(np.ascontiguousarray(w.transpose(0,2,3,1)));n['weights_bytes']=w.size;n['weights_layout']='OHWI'
                b=m.bias.detach().cpu().numpy() if m.bias is not None else np.zeros(m.out_channels)
                b=b/(we*2.**nodes[n['inputs'][0]]['scale_exp']);b=np.sign(b)*np.floor(np.abs(b)+.5)
                assert (np.abs(b)<=2147483647).all()
                n['bias_offset']=array(b.astype('<i4'))
                shifts=np.array([n['scale_exp']-nodes[n['inputs'][0]]['scale_exp']-e for e in n['weight_exps']],dtype='<i4')
                n['shifts_offset']=array(shifts)
                ih,iw,ic=nodes[n['inputs'][0]]['shape'];oh,ow,oc=n['shape']
                n['params']=dict(ih=ih,iw=iw,ic=ic,oh=oh,ow=ow,oc=oc,kh=m.kernel_size[0],kw=m.kernel_size[1],sy=m.stride[0],sx=m.stride[1],py=m.padding[0],px=m.padding[1],dy=m.dilation[0],dx=m.dilation[1],groups=m.groups)
            elif n['op']=='prelu':
                w=self.layers[n['module']].weight.detach().cpu().numpy()
                if len(w)==1:w=np.repeat(w,n['shape'][-1])
                slope=np.sign(w)*np.floor(np.abs(w)*32768+.5)
                assert np.isfinite(slope).all() and (np.abs(slope)<=2147483647).all()
                n['slope_offset']=array(slope.astype('<i4'))
            elif n['op']=='concat':n['shifts_offset']=array(np.array([n['scale_exp']-nodes[i]['scale_exp'] for i in n['inputs']],dtype='<i4'))
            n['payload_offset']=start;n['payload_bytes']=len(blob)-start
        # First-fit lifetime allocation; no input/output alias inside kernels.
        last=list(range(len(nodes)))
        for idx,n in enumerate(nodes):
            for i in n['inputs']:last[i]=max(last[i],idx)
        for i in self.outputs.values():last[i]=len(nodes)
        live=[];peak=0
        for idx,n in enumerate(nodes):
            live=[r for r in live if r[2]>=idx]
            size=int(np.prod(n['shape']));off=0
            for start,end,_ in sorted(live):
                if off+size<=start:break
                off=max(off,end)
            off=(off+3)//4*4
            # Alignment can shift into a gap's next block; retry correctly.
            while any(off<end and off+size>start for start,end,_ in live):
                off=max(end for start,end,_ in live if off<end and off+size>start);off=(off+3)//4*4
            n['offset']=off;n['bytes']=size;live.append((off,off+size,last[idx]));peak=max(peak,off+size)
        report=dict(format='ddnd-signed-pow2-v1',nodes=nodes,outputs=self.outputs,arena_bytes=peak,max_node_weight_bytes=max(n.get('payload_bytes',0) for n in nodes),weights_bytes=len(blob),rounding='nearest ties away from zero',status='exported_not_verified')
        (dest/'weights.bin').write_bytes(blob);(dest/'graph.json').write_text(json.dumps(report,indent=2))
        return report
