#!/usr/bin/env python3
"""Decode CRC-protected DDND depth+gate UART packets. Never sends commands."""
import argparse,json,struct,zlib
from pathlib import Path
import numpy as np
HEADER=struct.Struct('<4sHHIIIIHHBBBBbbbBI')
MAGIC=b'DDN1';SHAPES=(128,160,8,10,12,4);PAYLOAD=21444

def decode(packet):
 if len(packet)<HEADER.size+4:raise ValueError('Truncated packet')
 h=HEADER.unpack_from(packet)
 magic,version,hs,seq,capture,infer,payload,dh,dw,ch,cw,cc,vc,de,ce,ve,flags,tag=h
 if magic!=MAGIC or version!=1 or hs!=40:raise ValueError('Unsupported header')
 if (dh,dw,ch,cw,cc,vc)!=SHAPES or payload!=PAYLOAD:raise ValueError('Invalid tensor shapes')
 if len(packet)!=40+payload+4:raise ValueError('Invalid packet size')
 if not flags&1 or any(e< -32 or e>24 for e in (de,ce,ve)):raise ValueError('Invalid outputs')
 crc=struct.unpack_from('<I',packet,len(packet)-4)[0]
 if zlib.crc32(packet[:-4])!=crc:raise ValueError('CRC mismatch')
 a=np.frombuffer(packet,dtype=np.int8,offset=40,count=payload)
 logits=a[:dh*dw].reshape(dh,dw).astype(np.float32)*2.**de
 disp=1/(1+np.exp(-np.clip(logits,-80,80)))
 inv=.01+9.99*disp;depth=1/inv
 corners=a[dh*dw:dh*dw+ch*cw*cc].reshape(ch,cw,cc).astype(np.float32)*2.**ce
 vis=a[-vc:].astype(np.float32)*2.**ve-8.
 xy=[]
 for k in range(4):
  row,col=np.unravel_index(corners[:,:,k].argmax(),(ch,cw))
  dx,dy=np.clip(corners[row,col,4+2*k:6+2*k],0,1)
  xy.append([float(np.clip((col+dx)*16,0,159)),float(np.clip((row+dy)*16,0,127))])
 return dict(sequence=seq,capture_us=capture,inference_us=infer,model_tag=f'{tag:08x}',depth_m=depth,inverse_depth=inv,corners_xy=xy,visibility_logits=vis,visibility_probability=1/(1+np.exp(-np.clip(vis,-80,80))))

def packets(stream):
 buf=bytearray()
 while True:
  chunk=stream.read(4096)
  if not chunk:return
  buf.extend(chunk)
  while True:
   pos=buf.find(MAGIC)
   if pos<0:
    if len(buf)>3:del buf[:-3]
    break
   if pos:del buf[:pos]
   if len(buf)<40:break
   h=HEADER.unpack_from(buf)
   if h[1]!=1 or h[2]!=40 or h[6]!=PAYLOAD:
    del buf[0];continue
   size=40+PAYLOAD+4
   if len(buf)<size:break
   frame=bytes(buf[:size])
   try:decoded=decode(frame)
   except ValueError:
    del buf[0];continue
   del buf[:size];yield frame,decoded

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--file',type=Path);p.add_argument('--port');p.add_argument('--baud',type=int,default=921600);p.add_argument('--output',type=Path,required=True);p.add_argument('--count',type=int,default=10);a=p.parse_args()
 if bool(a.file)==bool(a.port):p.error('Specify exactly one of --file or --port')
 a.output.mkdir(parents=True,exist_ok=False)
 if a.file:s=a.file.open('rb')
 else:
  import serial
  s=serial.Serial(a.port,a.baud,timeout=30)
 with s:
  for i,(raw,d) in enumerate(packets(s)):
   (a.output/f'{i:05d}.bin').write_bytes(raw)
   np.savez_compressed(a.output/f'{i:05d}.npz',**d)
   print(json.dumps({k:d[k] for k in ('sequence','capture_us','inference_us','model_tag','corners_xy')}),flush=True)
   if i+1>=a.count:break
if __name__=='__main__':main()
