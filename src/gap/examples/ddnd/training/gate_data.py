"""Gate adapter for audited IsaacSim gate9 labels. All coordinates are pixel-center coordinates."""
import json
from pathlib import Path
import numpy as np
import torch
from PIL import Image
from torch.utils.data import Dataset

CORNER_ORDER = ("LT", "RT", "LB", "RB")
HEIGHT, WIDTH = 128, 160
HEATMAP_HEIGHT, HEATMAP_WIDTH = 8, 10
CORNER_STRIDE = 16

def adapt_record(row, margin=3):
    """Source has 160-square image and corner coordinates normalized by 159."""
    if "visible" not in row:
        raise ValueError("Per-corner occlusion labels required; cannot infer from gate_present")
    xy = np.asarray(row["corners"], dtype=np.float64)
    visible = np.asarray(row["visible"], dtype=bool)
    if xy.shape != (4, 2) or visible.shape != (4,):
        raise ValueError("Expected four corners and four visibility labels")
    xy = xy * 159.0 - np.array([0.0, 16.0])
    in_frame = (np.isfinite(xy).all(axis=1) &
                (xy[:, 0] >= margin) & (xy[:, 0] <= WIDTH-1-margin) &
                (xy[:, 1] >= margin) & (xy[:, 1] <= HEIGHT-1-margin))
    visible &= in_frame
    if row.get("gate_rendered") is False:
        visible[:] = False
    # Coordinates of masked corners are not used, and may be extremely large.
    xy[~visible] = 0
    return dict(image=row["image"], split=row["split"], group=row["group"],
                corners_xy=xy.tolist(), visibility=visible.tolist(),
                visibility_valid=[True]*4, crop_xyxy=[0,16,160,144],
                source_gate_rendered=row.get("gate_rendered"),
                corner_order=list(CORNER_ORDER))

def make_corner_targets(corners_xy, visible, sigma=0.75):
    """Coarse cell heatmaps and offsets in exact source pixel coordinates."""
    xy=np.asarray(corners_xy,dtype=np.float32)
    visible=np.asarray(visible,dtype=bool)
    if xy.shape!=(4,2) or visible.shape!=(4,):
        raise ValueError("Expected four corners")
    heat=np.zeros((4,HEATMAP_HEIGHT,HEATMAP_WIDTH),np.float32)
    offsets=np.zeros((8,HEATMAP_HEIGHT,HEATMAP_WIDTH),np.float32)
    valid=np.zeros((4,HEATMAP_HEIGHT,HEATMAP_WIDTH),bool)
    yy,xx=np.mgrid[:HEATMAP_HEIGHT,:HEATMAP_WIDTH]
    for k in np.flatnonzero(visible):
        x,y=xy[k]
        if not np.isfinite([x,y]).all() or not (0<=x<=WIDTH-1 and 0<=y<=HEIGHT-1):
            raise ValueError("Visible corner outside crop")
        u,v=float(x)/CORNER_STRIDE,float(y)/CORNER_STRIDE
        col,row=int(np.floor(u)),int(np.floor(v))
        heat[k]=np.exp(-((xx-col)**2+(yy-row)**2)/(2*sigma**2))
        offsets[2*k,row,col]=u-col
        offsets[2*k+1,row,col]=v-row
        valid[k,row,col]=True
    return heat,offsets,valid

def make_heatmaps(corners_xy,visible,sigma=0.75):
    return make_corner_targets(corners_xy,visible,sigma)[0]

class GateDataset(Dataset):
    def __init__(self, manifest_path, split="train", sigma=0.75):
        self.rows = []
        with open(manifest_path) as f:
            for line in f:
                row = json.loads(line)
                if row["split"] == split:
                    self.rows.append(row)
        if not self.rows:
            raise ValueError(f"No gate records for split {split}")
        self.sigma = sigma

    def __len__(self):
        return len(self.rows)

    def __getitem__(self, index):
        row = self.rows[index]
        with Image.open(row["image"]) as im:
            if im.size != (160,160):
                raise ValueError(f"Unexpected source image size {im.size}: {row['image']}")
            image = np.asarray(im.convert("L").crop(tuple(row["crop_xyxy"])),
                               dtype=np.float32).copy()[None] / 255.0
        visible = np.asarray(row["visibility"], dtype=bool)
        xy = np.asarray(row["corners_xy"], dtype=np.float32)
        heat,offsets,offset_valid=make_corner_targets(xy,visible,self.sigma)
        return dict(image=torch.from_numpy(image),
                    heatmaps=torch.from_numpy(heat),
                    corner_offsets=torch.from_numpy(offsets),
                    offset_valid=torch.from_numpy(offset_valid),
                    visibility=torch.from_numpy(visible.astype(np.float32)),
                    visibility_valid=torch.tensor(row["visibility_valid"],dtype=torch.bool),
                    heatmap_valid=torch.from_numpy(visible.copy()),
                    corners_xy=torch.from_numpy(xy),
                    image_path=row["image"])
