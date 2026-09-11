"""Frozen Depth Pro cache and source-consistent student images."""
import argparse, dataclasses, hashlib, json, os, zipfile
from pathlib import Path
import numpy as np
from PIL import Image, ImageOps
import torch
from torch.utils.data import Dataset

VERSION = "depthpro-felzenszwalb-v2"
def weight_sha256(path):
    digest=hashlib.sha256()
    with open(path,"rb") as f:
        for chunk in iter(lambda:f.read(8*1024*1024),b""):digest.update(chunk)
    return digest.hexdigest()

def validate_targets(data):
    for key,shape in (("sector_inverse",(3,)),("sector_valid",(3,)),
                      ("dense_inverse",(1,128,160)),("dense_valid",(1,128,160))):
        if np.asarray(data[key]).shape!=shape:raise ValueError(f"Invalid {key} shape")
    for key in ("sector_inverse","dense_inverse"):
        if not np.isfinite(data[key]).all():raise ValueError(f"Nonfinite {key}")
    for value,mask in (("sector_inverse","sector_valid"),("dense_inverse","dense_valid")):
        if np.asarray(data[mask]).dtype!=np.bool_:raise ValueError(f"Invalid {mask} dtype")
        if (np.asarray(data[value])[np.asarray(data[mask])]<=0).any():
            raise ValueError(f"Nonpositive valid {value}")

def read_rows(path):
    with open(path) as f: return [json.loads(x) for x in f if x.strip()]

def crop_rgb(row):
    im = ImageOps.exif_transpose(Image.open(row["image_path"])).convert("RGB")
    if row.get("source_aspect"):
        im = im.resize((round(im.height * row["source_aspect"]), im.height), Image.Resampling.BICUBIC)
    w,h=im.size
    if w/h > 1.25:
        cw=round(h*1.25); box=((w-cw)//2,0,(w-cw)//2+cw,h)
    else:
        ch=round(w/1.25); box=(0,(h-ch)//2,w,(h-ch)//2+ch)
    return im.crop(box)

def student_image(row):
    a=np.asarray(crop_rgb(row).convert("L").resize((160,128),Image.Resampling.BILINEAR),dtype=np.float32).copy()/255.
    return torch.from_numpy(a[None])

def regional_targets(depth):
    """Closest region mean; no variance rejection. Operates on metric teacher depth."""
    from skimage.segmentation import felzenszwalb
    z=np.asarray(depth,dtype=np.float32)
    if z.shape!=(128,160):raise ValueError("Teacher depth must have shape (128,160)")
    valid=np.isfinite(z)&(z>0)
    safe=np.clip(np.where(valid,z,20.),.2,20.)
    # Global fixed normalization: never per-image contrast/stretch.
    segments=felzenszwalb(np.log(safe)/np.log(100),scale=100,sigma=.5,min_size=32,channel_axis=None)
    q=np.zeros(3,np.float32); mask=np.zeros(3,bool); stats=np.zeros((3,3),np.float32)
    # Full-image connected regions are intersected with L/C/R.
    for sector,(lo,hi) in enumerate(((0,53),(53,107),(107,160))):
        sub=segments[:,lo:hi]; zv=safe[:,lo:hi]; vv=valid[:,lo:hi]
        candidates=[]
        for label in np.unique(sub):
            selected=(sub==label)&vv
            n=int(selected.sum())
            if n>=16:
                vals=zv[selected]
                candidates.append((float(vals.mean()),float(vals.std()),n))
        if candidates:
            mean,std,n=min(candidates,key=lambda t:t[0])
            q[sector]=1/mean; mask[sector]=True; stats[sector]=mean,std,n
    return dict(sector_inverse=q,sector_valid=mask,region_stats=stats,
                dense_inverse=(1/safe)[None].astype(np.float32),dense_valid=valid[None])

def cache_path(root,row):
    key=str(row["id"])
    if Path(key).name!=key: raise ValueError("Unsafe manifest id")
    return Path(root)/(key+".npz")

class DepthDataset(Dataset):
    def __init__(self,manifest_path,cache_dir):
        self.rows=read_rows(manifest_path); self.cache_dir=Path(cache_dir)
        if not self.rows: raise ValueError("Empty depth manifest")
    def __len__(self):return len(self.rows)
    def __getitem__(self,index):
        row=self.rows[index]
        with np.load(cache_path(self.cache_dir,row),allow_pickle=False) as data:
            if str(data["version"])!=VERSION:raise ValueError("Stale teacher cache")
            if str(data["source_hash"])!=row.get("sha256_pixels",""):raise ValueError("Source/cache hash mismatch")
            validate_targets(data)
            out={k:torch.from_numpy(data[k].copy()) for k in ("sector_inverse","sector_valid","dense_inverse","dense_valid")}
        out["image"]=student_image(row)
        return out

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--manifests",nargs="+",required=True)
    p.add_argument("--cache-dir",required=True)
    p.add_argument("--weights",required=True)
    p.add_argument("--shard",type=int,default=0);p.add_argument("--shards",type=int,default=1)
    p.add_argument("--limit",type=int);p.add_argument("--verify-only",action="store_true")
    args=p.parse_args()
    rows=[]
    for m in args.manifests:rows.extend(read_rows(m))
    ids=[r["id"] for r in rows]
    if len(ids)!=len(set(ids)):raise ValueError("Duplicate manifest IDs across splits")
    if args.shards<1 or not 0<=args.shard<args.shards:raise ValueError("Invalid shard selection")
    rows=rows[args.shard::args.shards]
    if args.limit:rows=rows[:args.limit]
    root=Path(args.cache_dir);root.mkdir(parents=True,exist_ok=True)
    weight_hash=weight_sha256(args.weights)
    if args.verify_only:
        for row in rows:
            with np.load(cache_path(root,row)) as d:
                assert str(d["version"])==VERSION
                assert str(d["source_hash"])==row.get("sha256_pixels","")
                assert str(d["teacher_weight_sha256"])==weight_hash,"Teacher weight/cache hash mismatch"
                validate_targets(d)
                assert d["sector_valid"].all(),row["id"]
        print(json.dumps({"verified":len(rows)}));return
    import depth_pro
    from depth_pro.depth_pro import DEFAULT_MONODEPTH_CONFIG_DICT
    if not torch.cuda.is_available():raise RuntimeError("Depth Pro requires allocated GPU")
    config=dataclasses.replace(DEFAULT_MONODEPTH_CONFIG_DICT,checkpoint_uri=args.weights)
    model,transform=depth_pro.create_model_and_transforms(config,device=torch.device("cuda"),precision=torch.float16)
    model.eval()
    for i,row in enumerate(rows):
        target=cache_path(root,row)
        if target.exists():
            try:
                with np.load(target,allow_pickle=False) as d:
                    matches=(str(d["version"])==VERSION
                             and str(d["source_hash"])==row.get("sha256_pixels","")
                             and str(d["teacher_weight_sha256"])==weight_hash)
                    if matches:
                        validate_targets(d)
                        continue
            except (KeyError,ValueError,OSError,zipfile.BadZipFile):
                pass # Rebuild stale or incomplete per-image caches.
        rgb=crop_rgb(row)
        # Preserve source information for teacher; matching field of view for student.
        with torch.inference_mode():
            prediction=model.infer(transform(rgb))
            z=prediction["depth"].float()[None,None]
            z=torch.nn.functional.interpolate(z,size=(128,160),mode="bilinear",align_corners=False)[0,0].cpu().numpy()
        result=regional_targets(z)
        validate_targets(result)
        result.update(teacher_weight_sha256=np.array(weight_hash),version=np.array(VERSION),source_hash=np.array(row.get("sha256_pixels","")),
                      teacher_focal_px=prediction["focallength_px"].float().cpu().numpy())
        temp=target.with_suffix(".tmp.npz")
        np.savez_compressed(temp,**result);os.replace(temp,target)
        if i%100==0:print(json.dumps({"processed":i+1,"total":len(rows),"shard":args.shard}),flush=True)
    (root/("shard-%02d.done.json"%args.shard)).write_text(json.dumps({"count":len(rows),"version":VERSION,"weight_sha256":weight_hash}))
if __name__=="__main__":main()
