"""Read-only numerical equivalence and size audit against pinned upstream source."""
import ast,json,sys
from pathlib import Path
import numpy as np
import torch
from torch import nn
from collections import OrderedDict
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from model.student import DDNDStudent,CADiT
torch.set_num_threads(2)
env={"nn":nn,"torch":torch,"np":np,"OrderedDict":OrderedDict,
     "trunc_normal_":nn.init.trunc_normal_}
for rel,names in [
 ("CNN/layers.py",{"ConvBlock","Conv3x3"}),
 ("CNN/networks/my_encoder.py",{"BNGELU","Conv","CDilated","SimpleDilatedConv","DroneDepthDecoder","DroneMono2"}),
]:
    tree=ast.parse((ROOT/"vendor/DDND"/rel).read_text())
    tree.body=[n for n in tree.body if isinstance(n,ast.ClassDef) and n.name in names]
    exec(compile(tree,rel,"exec"),env)
torch.manual_seed(42)
upstream=env["DroneMono2"]().eval()
replica=DDNDStudent().eval()
replica.load_state_dict(upstream.state_dict(),strict=True)
x=torch.rand(1,1,128,160)
with torch.no_grad():
    u=upstream(x)
    r=replica.forward_train(x)
    diffs={str(i):float((u[2][("disp",i)]-r["disparities"][i]).abs().max()) for i in range(3)}
    assert max(diffs.values())==0,diffs
    assert all(torch.equal(a,b) for a,b in zip(u[1],r["encoder_features"]))
    assert torch.equal(replica(x),r["disparities"][0])
    macs=[]
    handles=[]
    def hook(m,args,out):
        macs.append(out.numel()*(m.in_channels//m.groups)*m.kernel_size[0]*m.kernel_size[1])
    for m in replica.modules():
        if isinstance(m,nn.Conv2d): handles.append(m.register_forward_hook(hook))
    replica(x)
    for h in handles:h.remove()
    shapes={str(i):list(t.shape) for i,t in replica.forward_train(torch.rand(1,1,192,640))["disparities"].items()}
replica.train()
r=replica.forward_train(x)
cadit=CADiT()
teacher=[torch.rand(1,c,*f.shape[-2:]) for c,f in zip([48,48,80,128],r["encoder_features"])]
loss=cadit(r["encoder_features"],teacher)+sum(t.mean() for t in r["disparities"].values())
loss.backward()
assert all(torch.isfinite(p.grad).all() for p in replica.parameters() if p.grad is not None)
report={"upstream_state_dict_strict":True,"max_abs_error_by_scale":diffs,
        "encoder_features_bit_exact":True,"parameters_total":sum(p.numel() for p in replica.parameters()),
        "parameters_encoder":sum(p.numel() for n,p in replica.named_parameters() if not n.startswith("decoder.")),
        "parameters_decoder":sum(p.numel() for p in replica.decoder.parameters()),
        "conv_weights":sum(m.weight.numel() for m in replica.modules() if isinstance(m,nn.Conv2d)),
        "inference_conv_MACs_128x160":sum(macs),
        "dynamic_192x640_shapes":shapes,"training_gradient_finite":True,
        "paper_reported_parameters":310000,
        "paper_weight_bytes_fp32":747600,"paper_weight_bytes_int8":201300,
        "discrepancy":"Public DroneMono2 size differs from paper 310K claim. Exact released graph reproduced; numerical mapping to author flashed graph/checkpoint unavailable."}
print(json.dumps(report,indent=2))
(ROOT/"model/audit.json").write_text(json.dumps(report,indent=2)+"\n")
