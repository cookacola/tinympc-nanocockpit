"""DORY Conv/BN/ReLU depth-and-gate model; default dory-stride16-v4.

The DDND-compatible reference classes remain for comparison only. Native
QAT and compiled integer parity are recorded in separate export reports.
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
                outputs[i] = F.softplus(F.interpolate(logits,size=size,mode="bilinear",align_corners=True)) + 1e-6
        return outputs, decoded

class DDNDEncoder(nn.Module):
    def __init__(self):
        super().__init__()
        self.num_ch_enc = [32,64,80]
        self.dims = [32,64,80]
        self.depth = [3,3,6]
        self.dilation = [[1,2,3],[1,2,3],[1,2,3,2,4,6]]
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

    def forward(self, x):
        return self.forward_features(x)[0]

class SeparableRefine(nn.Sequential):
    def __init__(self, channels=16):
        super().__init__(
            nn.Conv2d(channels, channels, 3, padding=1, groups=channels, bias=False),
            nn.Conv2d(channels, channels, 1, bias=True),
            nn.ReLU(inplace=False))

class SectorHead(nn.Module):
    # Exact area-overlap integration over the three image-equivalent columns.
    def __init__(self, channels=80, hidden=16):
        super().__init__()
        self.compress = nn.Sequential(nn.Conv2d(channels, hidden, 1), nn.ReLU())
        self.mlp = nn.Sequential(nn.Linear(hidden*2, hidden), nn.ReLU(), nn.Linear(hidden, 1))
        self.output_activation = nn.ReLU()
        nn.init.constant_(self.mlp[-1].bias, 0.5)

    @staticmethod
    def regional_pool(x):
        width = x.shape[-1]
        col = torch.arange(width, device=x.device, dtype=x.dtype)
        pooled = []
        for sector in range(3):
            left, right = width*sector/3, width*(sector+1)/3
            weights = (torch.minimum(col+1, x.new_tensor(right)) -
                       torch.maximum(col, x.new_tensor(left))).clamp_min(0)
            avg = (x.mean(dim=2) * weights).sum(dim=-1) / weights.sum()
            # Max includes any column overlapping the sector.
            maximum = x.masked_fill((weights == 0)[None,None,None,:], -torch.inf).amax(dim=(2,3))
            pooled.append(torch.cat((avg, maximum),dim=1))
        return torch.stack(pooled, dim=1)

    def forward(self, features):
        pooled = self.regional_pool(self.compress(features))
        return self.output_activation(self.mlp(pooled).squeeze(-1))

class GateHead(nn.Module):
    def __init__(self, channels=(32,64,80), hidden=16):
        super().__init__()
        self.projections = nn.ModuleList([nn.Conv2d(c,hidden,1) for c in channels])
        self.refine8 = SeparableRefine(hidden)
        self.refine4 = SeparableRefine(hidden)
        self.heatmaps = nn.Conv2d(hidden,4,1)
        self.visibility = nn.Linear(hidden*2,4)

    def forward(self, features):
        f4,f8,f16 = [p(f) for p,f in zip(self.projections,features)]
        x = self.refine8(F.interpolate(f16,size=f8.shape[-2:],mode='nearest')+f8)
        x = self.refine4(F.interpolate(x,size=f4.shape[-2:],mode='nearest')+f4)
        pooled = torch.cat((x.mean(dim=(2,3)),x.amax(dim=(2,3))),dim=1)
        return self.heatmaps(x),self.visibility(pooled)

class ReferenceDepthGateModel(nn.Module):
    def __init__(self, auxiliary_dense=True):
        super().__init__()
        self.encoder = DDNDEncoder()
        self.sector_head = SectorHead()
        self.gate_head = GateHead()
        self.dense_decoder = DroneDepthDecoder() if auxiliary_dense else None
        self._encoder_frozen = False

    def set_encoder_frozen(self, frozen=True):
        self._encoder_frozen = bool(frozen)
        self.encoder.requires_grad_(not frozen)
        self.encoder.train(self.training and not frozen)

    def train(self, mode=True):
        super().train(mode)
        if self._encoder_frozen:
            self.encoder.eval()
        return self

    def forward(self, x, dense=True):
        if x.ndim != 4 or x.shape[1] != 1:
            raise ValueError('Expected grayscale BCHW input with one channel')
        features = self.encoder(x)
        heatmaps,visibility = self.gate_head(features)
        result = {'sector_inverse':self.sector_head(features[-1]),
                  'corner_heatmaps':heatmaps, 'visibility_logits':visibility}
        if dense and self.dense_decoder is not None:
            result['dense_inverse'] = self.dense_decoder(features,x.shape[-2:])[0]
        return result

    def load_ddnd_checkpoint(self, path, require_encoder=True):
        # Only load trusted locally produced checkpoints.
        checkpoint = torch.load(path,map_location='cpu',weights_only=False)
        source = checkpoint
        for key in ('model','state_dict','student','model_state_dict'):
            if isinstance(source,dict) and key in source and isinstance(source[key],dict):
                source = source[key]
                break
        own = self.state_dict()
        matched, mismatched, unmatched, mapped = [],[],[],{}
        for key,value in source.items():
            if not torch.is_tensor(value):
                continue
            key = key.removeprefix('module.').removeprefix('student.')
            if key.startswith(('downsample_layers.','stem2.','stages.')):
                destination = 'encoder.'+key
            elif key.startswith('decoder.'):
                destination = 'dense_decoder.'+key[len('decoder.'):]
            else:
                destination = key
            if destination not in own:
                unmatched.append(key)
            elif own[destination].shape != value.shape:
                mismatched.append({'source':key,'target':destination,'source_shape':list(value.shape),'target_shape':list(own[destination].shape)})
            else:
                matched.append({'source':key,'target':destination,'shape':list(value.shape)})
                mapped[destination] = value
        missing = [key for key in own if key not in mapped]
        report = {'matched':matched,'mismatched':mismatched,'unmatched':unmatched,
                  'missing':missing,'matched_parameters':sum(own[k].numel() for k in mapped)}
        missing_encoder = [k for k in missing if k.startswith('encoder.')]
        if require_encoder and missing_encoder:
            raise ValueError('Incomplete DDND encoder initialization: '+str(missing_encoder))
        self.load_state_dict(mapped,strict=False)
        return report


class ConvBNReLU(nn.Sequential):
    def __init__(self,ci,co,kernel=3,stride=1,groups=1):
        super().__init__(
            nn.Conv2d(ci,co,kernel,stride,kernel//2,groups=groups,bias=False),
            nn.BatchNorm2d(co),
            nn.ReLU(inplace=False))




class DORYEncoder(nn.Module):
    input_shape = (1,128,160)
    def __init__(self):
        super().__init__()
        self.stem=nn.Sequential(ConvBNReLU(1,16,3,2),ConvBNReLU(16,16,3))
        self.down4=nn.Sequential(ConvBNReLU(16,16,3,2,16),ConvBNReLU(16,32,1))
        self.down8=nn.Sequential(ConvBNReLU(32,32,3,2,32),ConvBNReLU(32,64,1))
        self.down16=nn.Sequential(ConvBNReLU(64,64,3,2,64),ConvBNReLU(64,96,1))
        self.context=nn.Sequential(
            ConvBNReLU(96,96,3),ConvBNReLU(96,96,3),ConvBNReLU(96,96,3),
            ConvBNReLU(96,96,3,1,96),ConvBNReLU(96,96,3,1,96))
    def forward_features(self,x):
        f4=self.down4(self.stem(x))
        f8=self.down8(f4)
        f16=self.context(self.down16(f8))
        return f4,f8,f16
    def forward(self,x):
        return self.forward_features(x)[-1]

class DORYSectorHead(nn.Module):
    input_shape = (96,8,10)
    def __init__(self):
        super().__init__()
        self.pool=nn.AvgPool2d((8,1))
        self.hidden=nn.Sequential(nn.Conv2d(96,16,(1,10),bias=False),
            nn.BatchNorm2d(16),nn.ReLU(inplace=False))
        self.output=ConvBNReLU(16,3,1)
        nn.init.constant_(self.output[1].bias,0.5)
    def forward(self,x):
        return self.output(self.hidden(self.pool(x)))

class DORYCornerHead(nn.Module):
    input_shape = (96,8,10)
    def __init__(self):
        super().__init__()
        self.refine=nn.Sequential(ConvBNReLU(96,32,1),ConvBNReLU(32,32,3))
        self.output=ConvBNReLU(32,12,1)
        with torch.no_grad():
            self.output[1].bias[:4].fill_(0.05)
            self.output[1].bias[4:].fill_(0.5)
    def forward(self,x):
        # Channels: heatmap0..3, dx0,dy0,dx1,dy1,dx2,dy2,dx3,dy3.
        return self.output(self.refine(x))

class DORYVisibilityHead(nn.Module):
    input_shape = (96,8,10)
    logit_offset = 8.0
    def __init__(self):
        super().__init__()
        self.compress=ConvBNReLU(96,8,1)
        self.spatial=nn.Sequential(nn.Conv2d(8,8,(8,10),bias=False),
            nn.BatchNorm2d(8),nn.ReLU(inplace=False))
        self.output=ConvBNReLU(8,4,1)
        nn.init.constant_(self.output[1].bias,self.logit_offset)
    def forward(self,x):
        return self.output(self.spatial(self.compress(x)))

class DepthGateModel(nn.Module):
    architecture = 'dory-stride16-v4'
    corner_stride = 16
    def __init__(self,auxiliary_dense=True):
        super().__init__()
        self.encoder=DORYEncoder()
        self.sector_head=DORYSectorHead()
        self.corner_head=DORYCornerHead()
        self.visibility_head=DORYVisibilityHead()
        self.dense_decoder=DroneDepthDecoder((32,64,96)) if auxiliary_dense else None
        self._encoder_frozen=False

    def set_encoder_frozen(self,frozen=True):
        self._encoder_frozen=bool(frozen)
        self.encoder.requires_grad_(not frozen)
        self.encoder.train(self.training and not frozen)

    def train(self,mode=True):
        super().train(mode)
        if self._encoder_frozen:
            self.encoder.eval()
        return self

    def forward(self,x,dense=True):
        if x.ndim!=4 or x.shape[1]!=1 or tuple(x.shape[-2:])!=(128,160):
            raise ValueError('Expected grayscale BCHW input at128x160')
        if dense and self.dense_decoder is not None:
            features=self.encoder.forward_features(x)
            feature=features[-1]
        else:
            feature=self.encoder(x)
        packed=self.corner_head(feature)
        result={'sector_inverse':self.sector_head(feature).flatten(1),
                'corner_heatmaps':packed[:,:4],
                'corner_offsets':packed[:,4:],
                'visibility_logits':self.visibility_head(feature).flatten(1)-8.0}
        if dense and self.dense_decoder is not None:
            result['dense_inverse']=self.dense_decoder(features,x.shape[-2:])[0]
        return result

    def deployment_partitions(self):
        return {'encoder':self.encoder,'sector_head':self.sector_head,
                'corner_head':self.corner_head,'visibility_head':self.visibility_head}

def audit_model(model=None):
    # Convolution/linear MACs only; excludes activation/pooling/interpolation/additions.
    model = model if model is not None else DepthGateModel()
    model.eval()
    counts, handles = {}, []
    def register(name,module):
        def count(mod, inputs, output):
            if isinstance(mod,nn.Conv2d):
                cost = output.numel() * (mod.in_channels//mod.groups) * mod.kernel_size[0]*mod.kernel_size[1]
            else:
                cost = output.numel() * mod.in_features
            group=name.split('.')[0]
            counts[group]=counts.get(group,0)+int(cost)
        handles.append(module.register_forward_hook(count))
    for name,module in model.named_modules():
        if isinstance(module,(nn.Conv2d,nn.Linear)):
            register(name,module)
    with torch.no_grad():
        model(torch.zeros(1,1,128,160))
    for h in handles:
        h.remove()
    parameters={name:sum(p.numel() for p in module.parameters())
                for name,module in model.named_children()}
    deployed_names=('encoder','sector_head','gate_head','corner_head','visibility_head')
    return {'input':[1,1,128,160],'macs':counts,'parameters':parameters,
            'deployed_macs':sum(counts.get(k,0) for k in deployed_names),
            'deployed_parameters':sum(parameters.get(k,0) for k in deployed_names),
            'exclusions':['BN','activation','pooling','interpolation','residual additions','bias adds'],
            'note':'MACs are algorithmic counts, not measured hardware latency.'}

if __name__ == '__main__':
    import json
    print(json.dumps(audit_model(),indent=2))
