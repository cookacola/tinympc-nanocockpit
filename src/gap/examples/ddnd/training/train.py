"""Stagewise depth/gate training. Scratch initialization and teacher output targets.

A: real depth encoder + sector + dense.
B: gate head only; encoder and its BN statistics frozen.
C: paired real/simulation fine-tuning.
Q: paired fine-tuning with native NEMO PACT fake quantization.
Q uses native PACT. Export/compiler/checksum verification remains a separate required gate.
"""
import argparse
import json
import random
from pathlib import Path
import numpy as np
import torch
from torch import nn
from torch.nn import functional as F
from torch.utils.data import DataLoader, Dataset
from models import DepthGateModel
from gate_data import GateDataset
from early_stopping import PlateauStopper

WEIGHTS = dict(sector=1.0, dense=.5, heatmap=1.0, offsets=1.0, visibility=.1)

def masked_mse(pred, target, valid):
    valid = valid.bool() & torch.isfinite(target)
    if not valid.any():
        return pred.sum()*0
    return (pred[valid]-target[valid]).square().mean()

def depth_losses(out,b):
    sector=masked_mse(out["sector_inverse"],b["sector_inverse"],b["sector_valid"])
    target=b["dense_inverse"]
    valid=b["dense_valid"].bool()
    if target.ndim==3: target=target[:,None]
    if valid.ndim==3: valid=valid[:,None]
    valid=valid & torch.isfinite(target)
    target=torch.where(valid,target,torch.zeros_like(target))
    terms=[]
    for pred in out["dense_inverse"].values():
        # Mask-normalized area downsampling: invalid values never dilute targets.
        support=F.interpolate(valid.float(),size=pred.shape[-2:],mode="area")
        numerator=F.interpolate(target,size=pred.shape[-2:],mode="area")
        resized=numerator/support.clamp_min(1e-8)
        terms.append(masked_mse(pred,resized,support>=.999))
    return dict(sector=sector,dense=torch.stack(terms).mean())

def gate_losses(out,b):
    pred,target=out["corner_heatmaps"],b["heatmaps"]
    visible=b["heatmap_valid"].bool()
    fg=target>=.1
    sq=(pred-target).square()
    # Equal mean foreground/background contributions per visible corner.
    fg_loss=(sq*fg).sum((-1,-2))/fg.sum((-1,-2)).clamp_min(1)
    bg_loss=(sq*(~fg)).sum((-1,-2))/(~fg).sum((-1,-2)).clamp_min(1)
    heatmap=((fg_loss+bg_loss)*.5)[visible].mean() if visible.any() else pred.sum()*0
    mask=b["visibility_valid"].bool()
    visibility=F.binary_cross_entropy_with_logits(
        out["visibility_logits"][mask],b["visibility"][mask]) if mask.any() else out["visibility_logits"].sum()*0
    offset_mask=b["offset_valid"].bool().repeat_interleave(2,dim=1)
    offsets=masked_mse(out["corner_offsets"],b["corner_offsets"],offset_mask)
    return dict(heatmap=heatmap,offsets=offsets,visibility=visibility)

def decode_corners(out):
    """Decode native v4 heatmap peaks and fractional cell offsets to crop pixels."""
    heat=out["corner_heatmaps"]
    batch,corners,height,width=heat.shape
    flat=heat.flatten(2).argmax(2)
    offsets=out["corner_offsets"].reshape(batch,corners,2,height*width)
    selected=offsets.gather(3,flat[:,:,None,None].expand(-1,-1,2,1)).squeeze(3).clamp(0,1)
    cells=torch.stack((flat%width,flat//width),dim=-1).to(selected.dtype)
    xy=(cells+selected)*16
    return torch.stack((xy[:,:,0].clamp(0,159),xy[:,:,1].clamp(0,127)),dim=-1)

class Synthetic(Dataset):
    def __init__(self,kind,size=8):self.kind,self.size=kind,size
    def __len__(self):return self.size
    def __getitem__(self,i):
        generator=torch.Generator().manual_seed(i)
        image=torch.rand(1,128,160,generator=generator)
        if self.kind=="depth":
            return dict(image=image,sector_inverse=torch.tensor([.5,1.,.25]),
                sector_valid=torch.ones(3,dtype=torch.bool),
                dense_inverse=torch.full((1,128,160),.5),
                dense_valid=torch.ones(1,128,160,dtype=torch.bool))
        from gate_data import make_corner_targets
        xy=np.array([[40,24],[120,24],[40,104],[120,104]],np.float32)
        heat,offsets,valid=make_corner_targets(xy,[True]*4)
        return dict(image=image,heatmaps=torch.from_numpy(heat),
            corner_offsets=torch.from_numpy(offsets),offset_valid=torch.from_numpy(valid),
            heatmap_valid=torch.ones(4,dtype=torch.bool),visibility=torch.ones(4),
            visibility_valid=torch.ones(4,dtype=torch.bool),corners_xy=torch.from_numpy(xy))

def loaders(args,validation=False):
    split="validation" if validation else "train"
    result={}
    if args.stage in "ACQ":
        if args.synthetic: dataset=Synthetic("depth")
        else:
            from depth_labels import DepthDataset
            manifest=args.val_manifest if validation else args.train_manifest
            if not manifest or not args.cache_dir:raise ValueError("Depth manifest and cache required")
            dataset=DepthDataset(manifest,args.cache_dir)
        result["depth"]=DataLoader(dataset,batch_size=args.batch_size,
            shuffle=not validation,num_workers=args.workers,pin_memory=args.device.startswith("cuda"),
            drop_last=not validation and len(dataset)>=args.batch_size)
    if args.stage in "BCQ":
        dataset=Synthetic("gate") if args.synthetic else GateDataset(args.gate_manifest,split=split)
        result["gate"]=DataLoader(dataset,batch_size=args.batch_size,
            shuffle=not validation,num_workers=args.workers,pin_memory=args.device.startswith("cuda"),
            drop_last=not validation and len(dataset)>=args.batch_size)
    return result

def move(batch,device):
    return {k:v.to(device,non_blocking=True) if isinstance(v,torch.Tensor) else v for k,v in batch.items()}

def batches(loader,steps):
    it=iter(loader)
    for _ in range(steps):
        try:yield next(it)
        except StopIteration:
            it=iter(loader)
            yield next(it)

def run_epoch(model,sets,args,optimizer=None):
    training=optimizer is not None
    model.train(training)
    if args.stage=="A":
        model.corner_head.eval();model.visibility_head.eval()
    if args.stage=="B":
        model.encoder.eval()
        model.sector_head.eval()
        if model.dense_decoder is not None:model.dense_decoder.eval()
        if getattr(model,"dense_pyramid",None) is not None:model.dense_pyramid.eval()
    if args.stage=="Q":
        from qat import enable_qat_observers, freeze_qat_observers
        (enable_qat_observers if training else freeze_qat_observers)(model)
        from qat import freeze_qat_batchnorm
        freeze_qat_batchnorm(model)
    # During training, paired domains get one independently averaged loss each.
    # Validation evaluates each dataset exactly once (without cycling short sets).
    steps=max(len(v) for v in sets.values())
    if args.smoke:steps=min(steps,args.smoke)
    iters={k:batches(v,steps) if training else iter(v) for k,v in sets.items()}
    sums,counts={},{}
    for step in range(steps):
        if training:optimizer.zero_grad(set_to_none=True)
        total=None
        with torch.set_grad_enabled(training):
            for kind,it in iters.items():
                try:b=move(next(it),args.device)
                except StopIteration:continue
                out=model(b["image"],dense=kind=="depth")
                losses=depth_losses(out,b) if kind=="depth" else gate_losses(out,b)
                value=sum(WEIGHTS[k]*v for k,v in losses.items())
                total=value if total is None else total+value
                n=b["image"].shape[0]
                for key,val in losses.items():
                    sums[key]=sums.get(key,0)+float(val.detach())*n
                    counts[key]=counts.get(key,0)+n
                if not training and kind=="depth":
                    valid=b["sector_valid"].bool()
                    if valid.any():
                        error=(out["sector_inverse"][valid]-b["sector_inverse"][valid]).abs()
                        sums["sector_inverse_mae"]=sums.get("sector_inverse_mae",0)+float(error.sum())
                        counts["sector_inverse_mae"]=counts.get("sector_inverse_mae",0)+error.numel()
                        meter_error=(out["sector_inverse"][valid].clamp(.05,5).reciprocal()-b["sector_inverse"][valid].clamp(.05,5).reciprocal()).abs()
                        sums["sector_meter_mae_capped_02_20m"]=sums.get("sector_meter_mae_capped_02_20m",0)+float(meter_error.sum())
                        counts["sector_meter_mae_capped_02_20m"]=counts.get("sector_meter_mae_capped_02_20m",0)+meter_error.numel()
                if not training and kind=="gate":
                    valid=b["visibility_valid"].bool()
                    good=((out["visibility_logits"]>=0)==b["visibility"].bool())[valid]
                    sums["visibility_accuracy"]=sums.get("visibility_accuracy",0)+float(good.sum())
                    counts["visibility_accuracy"]=counts.get("visibility_accuracy",0)+good.numel()
                    vis=b["heatmap_valid"].bool()
                    if vis.any():
                        xy=decode_corners(out)
                        error=(xy-b["corners_xy"]).norm(dim=-1)[vis]
                        sums["corner_error_px"]=sums.get("corner_error_px",0)+float(error.sum())
                        counts["corner_error_px"]=counts.get("corner_error_px",0)+error.numel()
            if total is None:continue
            if not torch.isfinite(total):raise FloatingPointError("Non-finite training loss")
            if training:
                total.backward()
                nn.utils.clip_grad_norm_([p for p in model.parameters() if p.requires_grad],5.,error_if_nonfinite=True)
                optimizer.step()
    metrics={k:v/max(counts[k],1) for k,v in sums.items()}
    metrics["weighted_loss"]=sum(WEIGHTS[k]*metrics[k] for k in WEIGHTS if k in metrics)
    return metrics

def rng_state():
    return dict(torch=torch.get_rng_state(),numpy=np.random.get_state(),python=random.getstate(),
                cuda=torch.cuda.get_rng_state_all() if torch.cuda.is_available() else None)

def restore_rng(state):
    torch.set_rng_state(state["torch"]);np.random.set_state(state["numpy"]);random.setstate(state["python"])
    if torch.cuda.is_available() and state["cuda"] is not None:torch.cuda.set_rng_state_all(state["cuda"])

def save(path,value):
    tmp=path.with_suffix(".partial");torch.save(value,tmp);tmp.replace(path)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--stage",choices=list("ABCQ"),required=True)
    p.add_argument("--train-manifest");p.add_argument("--val-manifest");p.add_argument("--cache-dir")
    p.add_argument("--gate-manifest",default="manifests/gates.jsonl")
    p.add_argument("--output",required=True);p.add_argument("--init");p.add_argument("--resume")
    p.add_argument("--epochs",type=int);p.add_argument("--batch-size",type=int,default=16)
    p.add_argument("--workers",type=int,default=4);p.add_argument("--seed",type=int,default=20260906)
    p.add_argument("--lr",type=float);p.add_argument("--smoke",type=int,nargs="?",const=2,default=0)
    p.add_argument("--synthetic",action="store_true")
    p.add_argument("--device",default="cuda" if torch.cuda.is_available() else "cpu")
    args=p.parse_args()
    if args.init and args.resume:raise ValueError("Use --init OR --resume")
    if args.batch_size<2:raise ValueError("Training batch must contain >=2 samples for scalar BN")
    if args.synthetic and not args.smoke:raise ValueError("Synthetic data is only permitted with --smoke")
    args.epochs=min(args.epochs or dict(A=30,B=30,C=20,Q=10)[args.stage],30)
    if args.epochs<1:raise ValueError("Epochs must be positive")
    if args.smoke:args.epochs=1
    args.lr=args.lr or dict(A=1e-3,B=1e-3,C=1e-4,Q=2e-5)[args.stage]
    torch.manual_seed(args.seed);np.random.seed(args.seed);random.seed(args.seed)
    output=Path(args.output);output.mkdir(parents=True,exist_ok=True)
    model=DepthGateModel(auxiliary_dense=True)
    previous=torch.load(args.resume or args.init,map_location="cpu",weights_only=False) if (args.resume or args.init) else None
    if previous is not None and previous.get("architecture",model.architecture)!=model.architecture:
        raise ValueError("Checkpoint architecture does not match current model")
    # Previous-stage FP32 load happens before parametrization.
    if previous is not None and not args.resume:model.load_state_dict(previous["model"],strict=True)
    if args.stage=="Q":
        from qat import prepare_qat
        model=prepare_qat(model)
    if args.resume:
        if previous["stage"]!=args.stage:raise ValueError("Resume stage mismatch; use --init for stage transfer")
        model.load_state_dict(previous["model"],strict=True)
    if args.stage=="A":
        model.corner_head.requires_grad_(False);model.visibility_head.requires_grad_(False)
    if args.stage=="B":
        model.requires_grad_(False);model.corner_head.requires_grad_(True);model.visibility_head.requires_grad_(True);model.set_encoder_frozen(True)
    model.to(args.device)
    optimizer=torch.optim.AdamW([v for v in model.parameters() if v.requires_grad],lr=args.lr,weight_decay=1e-4)
    start,best=0,float("inf")
    if args.resume:
        optimizer.load_state_dict(previous["optimizer"])
        start=previous["epoch"]+1;best=previous["best"]
        restore_rng(previous["rng"])
    stopper=PlateauStopper()
    if args.resume:
        if "early_stopping" in previous:stopper.load_state_dict(previous["early_stopping"])
        elif (output/"metrics.jsonl").exists():
            for line in (output/"metrics.jsonl").read_text().splitlines():
                record=json.loads(line)
                if record["epoch"]<start:stopper.update(record["validation"]["weighted_loss"],record["epoch"]+1)
    training,validation=loaders(args),loaders(args,True)
    if args.stage=="Q" and not args.resume:
        from qat import calibrate_qat
        from itertools import chain,islice
        calibration=chain.from_iterable(islice(loader,4) for loader in training.values())
        calibrate_qat(model,calibration,args.device,max_batches=8)
    (output/"config.json").write_text(json.dumps(dict(vars(args),loss_weights=WEIGHTS,architecture=model.architecture),indent=2))
    completed=start
    stop_reason="epoch_cap"
    already_stopped=args.resume and previous.get("stop_reason")=="validation_plateau"
    if already_stopped:stop_reason="validation_plateau"
    for epoch in range(start,args.epochs) if not already_stopped else []:
        train=run_epoch(model,training,args,optimizer)
        val=run_epoch(model,validation,args)
        improved=val["weighted_loss"]<best
        best=min(best,val["weighted_loss"])
        completed=epoch+1
        plateau=stopper.update(val["weighted_loss"],completed)
        if plateau:stop_reason="validation_plateau"
        checkpoint=dict(early_stopping=stopper.state_dict(),stop_reason=stop_reason if plateau else None,model=model.state_dict(),architecture=model.architecture,optimizer=optimizer.state_dict(),
            epoch=epoch,stage=args.stage,best=best,rng=rng_state(),config=vars(args),
            validation=val,qat_kind="native NEMO PACT fake quant; integer export separate" if args.stage=="Q" else None)
        save(output/"last.pt",checkpoint)
        if improved:save(output/"best.pt",checkpoint)
        if args.stage=="Q":
            from qat import export_qat_npz
            export_qat_npz(model,output/"last_native.npz")
            if improved:export_qat_npz(model,output/"best_native.npz")
        record=dict(epoch=epoch,stage=args.stage,train=train,validation=val)
        with (output/"metrics.jsonl").open("a") as f:f.write(json.dumps(record)+"\n")
        print(json.dumps(record),flush=True)
        if plateau:break
    (output/"COMPLETE.json").write_text(json.dumps(dict(stage=args.stage,epochs=completed,epoch_cap=args.epochs,stop_reason=stop_reason,best=best,synthetic=args.synthetic,architecture=model.architecture)))
if __name__=="__main__":main()
