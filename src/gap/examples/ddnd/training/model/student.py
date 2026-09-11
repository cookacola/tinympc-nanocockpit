"""Faithful public DDND DroneMono2 student, with resolution-independent resizing.

Source: vendor/DDND/CNN/networks/my_encoder.py and CNN/layers.py.
No transformer/teacher/PoseNet is part of inference. Input float grayscale [0,1].
The public code disables input normalization, CDC residual/MLP/nonlinearity.
"""
from collections import OrderedDict
import torch
from torch import nn
from torch.nn import functional as F

class BNGELU(nn.Module):
    def __init__(self, channels):
        super().__init__()
        self.bn = nn.BatchNorm2d(channels, eps=1e-5)
        self.act = nn.ReLU()
    def forward(self, x):
        return self.act(self.bn(x))

class Conv(nn.Module):
    def __init__(self, ci, co, stride=1, bn_act=False):
        super().__init__()
        self.bn_act = bn_act
        self.conv = nn.Conv2d(ci, co, 3, stride, 1, bias=False)
        if bn_act:
            self.bn_gelu = BNGELU(co)
    def forward(self, x):
        x = self.conv(x)
        return self.bn_gelu(x) if self.bn_act else x

class CDilated(nn.Module):
    def __init__(self, channels, dilation):
        super().__init__()
        self.conv = nn.Conv2d(channels, channels, 3, padding=dilation,
                              dilation=dilation, groups=channels, bias=False)
    def forward(self, x):
        return self.conv(x)

class SimpleDilatedConv(nn.Module):
    def __init__(self, channels, dilation):
        super().__init__()
        self.ddwconv = CDilated(channels, dilation)
        self.bn1 = nn.BatchNorm2d(channels)
    def forward(self, x):
        return self.bn1(self.ddwconv(x))

class Conv3x3(nn.Module):
    def __init__(self, ci, co):
        super().__init__()
        self.conv = nn.Conv2d(ci, co, 3, padding=1, bias=False)
    def forward(self, x):
        return self.conv(x)

class ConvBlock(nn.Module):
    def __init__(self, ci, co):
        super().__init__()
        self.conv = Conv3x3(ci, co)
        self.nonlin = nn.PReLU()
    def forward(self, x):
        return self.nonlin(self.conv(x))

class DroneDepthDecoder(nn.Module):
    def __init__(self, num_ch_enc=(32,64,80)):
        super().__init__()
        self.num_ch_enc = list(num_ch_enc)
        self.num_ch_dec = [16,32,40]
        # Plain OrderedDict plus ModuleList matches upstream state_dict keys.
        self.convs = OrderedDict()
        for i in range(2,-1,-1):
            ci = self.num_ch_enc[-1] if i == 2 else self.num_ch_dec[i+1]
            co = self.num_ch_dec[i]
            self.convs[("upconv",i,0)] = ConvBlock(ci,co)
            ci = co + (self.num_ch_enc[i-1] if i > 0 else 0)
            self.convs[("upconv",i,1)] = ConvBlock(ci,co)
        for i in range(3):
            self.convs[("dispconv",i)] = Conv3x3(self.num_ch_dec[i],1)
        self.decoder = nn.ModuleList(list(self.convs.values()))
        # Match upstream decoder initialization, the encoder uses Torch defaults.
        for m in self.modules():
            if isinstance(m, nn.Conv2d):
                nn.init.trunc_normal_(m.weight,std=.02)
    def forward(self, features, image_size, final_only=False):
        outputs, decoded = {}, []
        x = features[-1]
        for i in range(2,-1,-1):
            x = self.convs[("upconv",i,0)](x)
            size = features[i-1].shape[-2:] if i > 0 else (image_size[0]//2,image_size[1]//2)
            x = F.interpolate(x,size=size,mode="bilinear",align_corners=True)
            if i > 0:
                x = torch.cat([x,features[i-1]],dim=1)
            x = self.convs[("upconv",i,1)](x)
            decoded.append(x)
            if not final_only or i == 0:
                logits = self.convs[("dispconv",i)](x)
                size = (image_size[0]//(2**i),image_size[1]//(2**i))
                outputs[i] = torch.sigmoid(F.interpolate(logits,size=size,mode="bilinear",align_corners=True))
        return outputs, decoded

class DDNDStudent(nn.Module):
    def __init__(self):
        super().__init__()
        self.num_ch_enc = [32,64,80]
        self.dims = [32,64,80]
        self.depth = [3,3,6]
        self.dilation = [[1,2,3],[1,2,3],[1,2,3,2,4,6]]
        self.decoder = DroneDepthDecoder(self.num_ch_enc)
        self.downsample_layers = nn.ModuleList([
            nn.Sequential(Conv(1,32,2,True),Conv(32,32,1,True),Conv(32,32,1,True)),
            nn.Sequential(Conv(32,64,2)),
            nn.Sequential(Conv(64,80,2)),
        ])
        self.stem2 = nn.Sequential(Conv(32,32,2))
        self.stages = nn.ModuleList([
            nn.Sequential(*[SimpleDilatedConv(c,d) for d in ds])
            for c,ds in zip(self.dims,self.dilation)
        ])
    def forward_features(self,x):
        features, distill = [], []
        x = self.downsample_layers[0](x)
        distill.append(x)
        x = self.stages[0](self.stem2(x))
        features.append(x)
        distill.append(x)
        for i in range(1,3):
            x = self.stages[i](self.downsample_layers[i](x))
            features.append(x)
            distill.append(x)
        return features, distill
    def forward_train(self,x):
        features, distill = self.forward_features(x)
        outputs, decoded = self.decoder(features,x.shape[-2:])
        return {"disparities":outputs,"encoder_features":distill,
                "decoder_features":decoded,"features":features}
    def forward(self,x):
        features,_ = self.forward_features(x)
        outputs,_ = self.decoder(features,x.shape[-2:],final_only=True)
        return outputs[0]

# Explicit alias, not a different architecture.
DroneMono2 = DDNDStudent

class CADiT(nn.Module):
    """Paper Eqs.7-9: channel correlation and residual feature matching.

    Uses mean squared error for L2 aggregation; sum/mean normalization is not
    fully specified by the paper. CCM uses softmax over teacher channels.
    Float32 correlation avoids half precision overflow, teacher is detached.
    """
    def __init__(self,student_channels=(32,32,64,80),teacher_channels=(48,48,80,128)):
        super().__init__()
        self.align = nn.ModuleList([
            nn.Conv2d(cs,ct,1,bias=False) for cs,ct in zip(student_channels,teacher_channels)
        ])
    def forward(self,student_features,teacher_features):
        losses = []
        for align,s,t in zip(self.align,student_features,teacher_features):
            s = align(s)
            if s.shape[-2:] != t.shape[-2:]:
                s = F.interpolate(s,size=t.shape[-2:],mode="bilinear",align_corners=True)
            with torch.autocast(device_type=s.device.type,enabled=False):
                s = s.float().flatten(2).transpose(1,2)
                t = t.detach().float().flatten(2).transpose(1,2)
                ccm = torch.softmax(s.transpose(1,2) @ t,dim=-1)
                losses.append(F.mse_loss(s+s@ccm,t))
        return torch.stack(losses).mean()
