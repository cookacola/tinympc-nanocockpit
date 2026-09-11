"""Signed fixed-scale graph QAT; full validation, independent held-out evaluation."""
import argparse,copy,json,time
from pathlib import Path
import torch
from torch.utils.data import DataLoader
from baseline import DDNDGate
from integer_graph import GraphNet
from depth_labels import DepthDataset
from gate_data import GateDataset
import train_dense_gate as training
from train import rng_state,restore_rng,save
ROOT=Path(__file__).resolve().parent

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--init',default='artifacts/export/qat_init.pt')
 p.add_argument('--resume')
 p.add_argument('--output',type=Path,default=Path('artifacts/qat'))
 p.add_argument('--epochs',type=int,default=10)
 p.add_argument('--patience',type=int,default=5)
 p.add_argument('--lr',type=float,default=2e-5)
 p.add_argument('--batch-size',type=int,default=16)
 p.add_argument('--workers',type=int,default=4)
 p.add_argument('--device',default='cuda')
 p.add_argument('--smoke',type=int,default=0)
 p.add_argument('--evaluate',choices=['val','test'])
 a=p.parse_args()
 if not 1<=a.epochs<=10:raise ValueError('QAT capped at 10 epochs')
 torch.set_num_threads(4);torch.manual_seed(20260909)
 import random,numpy as np
 random.seed(20260909);np.random.seed(20260909)
 a.stage='joint'
 a.output.mkdir(exist_ok=True,parents=True)
 ck=torch.load(a.resume or a.init,map_location='cpu',weights_only=False)
 g=GraphNet(DDNDGate());g.load_state_dict(ck['model'],strict=True);g.nodes=copy.deepcopy(ck['nodes']);g.outputs=copy.deepcopy(ck['outputs'])
 g.qat=True;g.calibrate=False;g.to(a.device)
 assert not any(isinstance(m,torch.nn.modules.batchnorm._BatchNorm) for m in g.modules())
 fixed=[(n.get('scale_exp'),copy.deepcopy(n.get('weight_exps'))) for n in g.nodes]
 # Graph is folded, all weights trainable; original stage freezer is inapplicable.
 training.configure=lambda model,stage:None
 def loaders(split):
  gate_split={'train':'train','val':'validation','test':'test'}[split]
  ds={'dense':DepthDataset(ROOT/'manifests'/f'dronetv3_{split}.jsonl','/home/cchen/depthgate-20260906/data/teacher_cache'),
      'gate':GateDataset('/home/cchen/depthgate-20260906/manifests/gates_10cm.jsonl',gate_split)}
  return {k:DataLoader(v,batch_size=a.batch_size,shuffle=split=='train',num_workers=a.workers,pin_memory=a.device.startswith('cuda')) for k,v in ds.items()}
 if a.evaluate:
  result=training.epoch(g,loaders(a.evaluate),a)
  (a.output/f'{a.evaluate}_metrics.json').write_text(json.dumps(result,indent=2))
  print(json.dumps({'evaluation_split':a.evaluate,'metrics':result}),flush=True);return
 optimizer=torch.optim.AdamW(g.parameters(),lr=a.lr,weight_decay=1e-5)
 start=0;best=float('inf');bad=0
 if a.resume:
  optimizer.load_state_dict(ck['optimizer']);start=ck['epoch']+1;best=ck['best'];bad=ck['bad_epochs'];restore_rng(ck['rng'])
 trainsets=loaders('train');valsets=loaders('val')
 (a.output/'config.json').write_text(json.dumps(vars(a),default=str,indent=2))
 print(json.dumps({'stage':'qat','train_samples':{k:len(v.dataset) for k,v in trainsets.items()},'scales':'fixed signed pow2','epochs':a.epochs}),flush=True)
 for e in range(start,a.epochs):
  t=time.time();tr=training.epoch(g,trainsets,a,optimizer);va=training.epoch(g,valsets,a)
  assert fixed==[(n.get('scale_exp'),n.get('weight_exps')) for n in g.nodes]
  clipped=0;weight_count=0
  for n in g.nodes:
   if n['op']=='conv':
    m=g.layers[n['module']]
    eps=torch.tensor(n['weight_exps'],device=m.weight.device).exp2()[:,None,None,None]
    clipped+=int((m.weight.detach().abs()>127*eps).sum());weight_count+=m.weight.numel()
   if n['op']=='prelu':
    slope=g.layers[n['module']].weight.detach()
    if not torch.isfinite(slope).all() or (slope.abs()*32768>2147483647).any():raise FloatingPointError('PReLU slope not representable as int32 Q15')
  va['weight_clipping_fraction']=clipped/max(weight_count,1)
  score=va['weighted_loss'];improved=score<best-1e-4
  if improved:best=score;bad=0
  else:bad+=1
  record={'epoch':e,'train':tr,'val':va,'seconds':time.time()-t,'best':best,'bad_epochs':bad}
  print(json.dumps(record),flush=True)
  with (a.output/'metrics.jsonl').open('a') as f:f.write(json.dumps(record)+'\n')
  state=dict(model=g.state_dict(),nodes=g.nodes,outputs=g.outputs,optimizer=optimizer.state_dict(),rng=rng_state(),epoch=e,best=best,bad_epochs=bad,stage='qat',args=vars(a))
  save(a.output/'last.pt',state)
  if improved:save(a.output/'best.pt',state)
  if bad>=a.patience:break
if __name__=='__main__':main()
