"""DDND dense Depth Pro distillation and 10 cm gate training (float stages).
QAT intentionally not approximated here: use verified runtime contract first.
"""
import argparse,json,random,sys,time
from pathlib import Path
import numpy as np
import torch
from torch import nn
from torch.utils.data import DataLoader
from baseline import DDNDGate
from depth_labels import DepthDataset
from gate_data import GateDataset
from train import gate_losses,decode_corners,move,batches,rng_state,restore_rng,save

ROOT=Path(__file__).resolve().parent
WEIGHTS={'dense':1.,'heatmap':1.,'offsets':1.,'visibility':.1}
def configure(model,stage):
    for p in model.parameters(): p.requires_grad_(True)
    frozen=[model.depth] if stage=='gate' else ([model.gate_adapter,model.corner_head,model.visibility_head] if stage=='dense' else [])
    for mod in frozen:
        mod.eval()
        for p in mod.parameters(): p.requires_grad_(False)

def epoch(model,sets,args,optimizer=None):
    training=optimizer is not None
    model.train(training)
    configure(model,args.stage)
    if not training:model.eval()
    steps=max(map(len,sets.values()))
    if args.smoke:steps=min(steps,args.smoke)
    iters={k:batches(v,steps) if training else iter(v) for k,v in sets.items()}
    sums={}; counts={}
    def add(k,value,n):
        sums[k]=sums.get(k,0.)+float(value);counts[k]=counts.get(k,0)+n
    for _ in range(steps):
        if training:optimizer.zero_grad(set_to_none=True)
        with torch.set_grad_enabled(training):
            for kind,it in iters.items():
                try:b=move(next(it),args.device)
                except StopIteration:continue
                out=model(b['image'])
                if kind=='dense':
                    target=b['dense_inverse'];valid=b['dense_valid'].bool()
                    if target.ndim==3:target=target[:,None];valid=valid[:,None]
                    pred=.01+9.99*out['disparity']
                    valid=valid & torch.isfinite(target)
                    if not valid.any():raise ValueError('No valid depth pixels')
                    # Physical inverse metres; no per-image scale alignment.
                    err=pred[valid]-target[valid]
                    losses={'dense':err.square().mean()}
                    if not training:
                        meters=pred[valid].reciprocal(); truth=target[valid].reciprocal()
                        add('inverse_mae',err.abs().sum(),err.numel())
                        add('depth_mae_m',(meters-truth).abs().sum(),err.numel())
                        add('depth_absrel',((meters-truth).abs()/truth).sum(),err.numel())
                        near=truth<=2.
                        add('near_depth_mae_m',(meters[near]-truth[near]).abs().sum(),int(near.sum()))
                else:
                    losses=gate_losses(out,b)
                    if not training:
                        v=b['visibility_valid'].bool()
                        add('visibility_accuracy',(((out['visibility_logits']>=0)==b['visibility'].bool())[v]).sum(),int(v.sum()))
                        v=b['heatmap_valid'].bool()
                        err=(decode_corners(out)-b['corners_xy']).norm(dim=-1)[v]
                        add('corner_error_px',err.sum(),err.numel())
                loss=sum(WEIGHTS[k]*v for k,v in losses.items())
                if not torch.isfinite(loss):raise FloatingPointError('Nonfinite loss')
                n=b['image'].shape[0]
                for k,v in losses.items():add(k,v.detach()*n,n)
                # Backward per domain releases dense activation memory before gate batch.
                if training:loss.backward()
            if training:
                nn.utils.clip_grad_norm_([p for p in model.parameters() if p.requires_grad],5.,error_if_nonfinite=True)
                optimizer.step()
    result={k:sums[k]/counts[k] if counts[k] else None for k in sums}
    result['weighted_loss']=sum(WEIGHTS[k]*result[k] for k in WEIGHTS if k in result)
    return result

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--stage',choices=['dense','gate','joint'],required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--init',default='/home/cchen/ddnd-replication-20260906/artifacts/student_isaac/best.pt')
    p.add_argument('--resume')
    p.add_argument('--epochs',type=int,default=30)
    p.add_argument('--patience',type=int,default=5)
    p.add_argument('--min-delta',type=float,default=1e-4)
    p.add_argument('--batch-size',type=int,default=16)
    p.add_argument('--workers',type=int,default=4)
    p.add_argument('--lr',type=float,default=1e-4)
    p.add_argument('--device',default='cuda')
    p.add_argument('--smoke',type=int,default=0)
    p.add_argument('--seed',type=int,default=20260909)
    p.add_argument('--cache-dir',default='/home/cchen/depthgate-20260906/data/teacher_cache')
    p.add_argument('--gate-manifest',default='/home/cchen/depthgate-20260906/manifests/gates_10cm.jsonl')
    args=p.parse_args()
    if not 1<=args.epochs<=30:raise ValueError('Epoch cap is 30')
    random.seed(args.seed);np.random.seed(args.seed);torch.manual_seed(args.seed)
    torch.set_num_threads(4)
    args.output.mkdir(parents=True,exist_ok=True)
    model=DDNDGate().to(args.device)
    ck=torch.load(args.resume or args.init,map_location='cpu',weights_only=False)
    if 'stage' in ck:model.load_state_dict(ck['model'],strict=True)
    else:model.load_depth_checkpoint(args.init)
    configure(model,args.stage)
    optimizer=torch.optim.AdamW([p for p in model.parameters() if p.requires_grad],lr=args.lr,weight_decay=1e-5)
    start=0;best=float('inf');bad=0
    if args.resume:
        if ck['stage']!=args.stage:raise ValueError('Resume stage mismatch; use --init for next stage')
        optimizer.load_state_dict(ck['optimizer']);start=ck['epoch']+1;best=ck['best'];bad=ck['bad_epochs'];restore_rng(ck['rng'])
    def loaders(validation):
        ds={}
        if args.stage in ['dense','joint']:
            ds['dense']=DepthDataset(ROOT/'manifests'/('dronetv3_val.jsonl' if validation else 'dronetv3_train.jsonl'),args.cache_dir)
        if args.stage in ['gate','joint']:
            ds['gate']=GateDataset(args.gate_manifest,'validation' if validation else 'train')
        return {k:DataLoader(v,batch_size=args.batch_size,shuffle=not validation,num_workers=args.workers,pin_memory=args.device.startswith('cuda')) for k,v in ds.items()}
    trainsets=loaders(False);valsets=loaders(True)
    print(json.dumps({'stage':args.stage,'train_samples':{k:len(v.dataset) for k,v in trainsets.items()},'val_samples':{k:len(v.dataset) for k,v in valsets.items()},'parameters':sum(p.numel() for p in model.parameters()),'quantization':'float; QAT pending verified contract'}),flush=True)
    (args.output/'config.json').write_text(json.dumps(vars(args),default=str,indent=2))
    for e in range(start,args.epochs):
        t=time.time();tr=epoch(model,trainsets,args,optimizer);va=epoch(model,valsets,args)
        score=va['weighted_loss'];improved=score<best-args.min_delta
        if improved:best=score;bad=0
        else:bad+=1
        record={'epoch':e,'train':tr,'val':va,'seconds':time.time()-t,'best':best,'bad_epochs':bad}
        print(json.dumps(record),flush=True)
        with (args.output/'metrics.jsonl').open('a') as f:f.write(json.dumps(record)+'\n')
        state={'model':model.state_dict(),'optimizer':optimizer.state_dict(),'stage':args.stage,'epoch':e,'best':best,'bad_epochs':bad,'rng':rng_state(),'args':vars(args)}
        save(args.output/'last.pt',state)
        if improved:save(args.output/'best.pt',state)
        if bad>=args.patience:
            print('Validation plateau: early stopping',flush=True);break
if __name__=='__main__':main()
