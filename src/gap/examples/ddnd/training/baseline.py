"""Checkpoint-preserving DDND plus a low-resolution gate branch.
This is a float baseline, NOT a verified DORY export.
"""
import torch
from torch import nn
from model.student import DDNDStudent
from models import ConvBNReLU,DORYCornerHead,DORYVisibilityHead

class DDNDGate(nn.Module):
    def __init__(self):
        super().__init__()
        self.depth=DDNDStudent()
        # Existing tested gate architecture; adapter accepts original 80-channel feature.
        self.gate_adapter=ConvBNReLU(80,96,1)
        self.corner_head=DORYCornerHead()
        self.visibility_head=DORYVisibilityHead()
    def forward(self,x):
        f,_=self.depth.forward_features(x)
        depth,_=self.depth.decoder(f,x.shape[-2:],final_only=True)
        g=self.gate_adapter(f[-1]);packed=self.corner_head(g)
        return {'disparity':depth[0], 'corner_heatmaps':packed[:,:4],
                'corner_offsets':packed[:,4:],
                'visibility_logits':self.visibility_head(g).flatten(1)-8.0}
    def load_depth_checkpoint(self,path):
        c=torch.load(path,map_location='cpu',weights_only=False)
        for key in ('student','model','state_dict'):
            if key in c:
                c=c[key];break
        self.depth.load_state_dict(c,strict=True)
