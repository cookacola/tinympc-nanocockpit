"""Host C compact packet against recovered Python raw decoder, both fixtures."""
import ctypes, pathlib, subprocess, tempfile, struct, zlib
import numpy as np
from read_ddnd import decode, HEADER
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp)
 (p/'wrap.c').write_text('#include "depthgate_output.h"\nvoid packet(void *p,void *d,void *c,void *v){ddnd_depthgate(p,d,c,v,-4,-5,-3,12345,7,1491700,1);}\n')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I'+str(root/'gap8_app/inc'),str(p/'wrap.c'),'-lm','-o',str(p/'test.so')],check=True)
 lib=ctypes.CDLL(str(p/'test.so'))
 for prefix in ['', 'gate_']:
  values=[(root/'gap8_app/hex'/('golden_'+prefix+name+'.bin')).read_bytes() for name in ['depth_logits','corners','visibility']]
  raw=HEADER.pack(b'DDN1',1,40,7,12345000,1491700,21444,128,160,8,10,12,4,-4,-5,-3,1,0)+b''.join(values)
  expected=decode(raw+struct.pack('<I',zlib.crc32(raw)))
  out=ctypes.create_string_buffer(80)
  lib.packet(out,*[ctypes.create_string_buffer(v) for v in values])
  h=struct.unpack('<4sIHHI15fI',out.raw)
  assert h[:5]==(b'\x90\x19\x08\x44',12345,7,3,1491700)
  assert h[-1]==zlib.crc32(out.raw[:76])
  np.testing.assert_allclose(h[5:8],[expected['inverse_depth'][:,s*160//3:(s+1)*160//3].max() for s in range(3)],rtol=1e-6)
  np.testing.assert_allclose(h[8:16],np.array(expected['corners_xy']).flatten(),atol=1e-6)
  np.testing.assert_allclose(h[16:20],expected['visibility_logits'],atol=1e-6)
print('PASS: both fixtures, inverse metres, corner offsets, visibility shift, 80-byte ABI and CRC')
