import subprocess,shlex,re,json,time
from pathlib import Path
asm=Path('/home/vboxuser/tinympc-nanocockpit/src/gap/examples/ddnd-final-11174/gap8_app/BUILD/GAP8_V2/GCC_RISCV/ddnd_live.s').read_text()
addr=re.search(r'^([0-9a-f]+) .*\bpacket$',asm,re.M)[1]
script='adapter speed 300; tap_select gap8_adv_debug_itf; du_select adv_dbg_unit 1; init; resume; '
for i in range(3):
 script+=f'sleep 5000; mem2array p 32 0x{addr} 10; echo SAMPLE{i}; echo [array get p]; '
script+='shutdown'
cmd='source /gap_sdk/configs/ai_deck.sh >/dev/null 2>&1; timeout -k 2 28 gap8-openocd -c "gdb_port disabled; telnet_port disabled; tcl_port disabled" -f interface/ftdi/olimex-arm-usb-tiny-h.cfg -f target/gap8revb.tcl -c '+shlex.quote(script)
r=subprocess.run(['docker','run','--rm','--privileged','-v','/dev/bus/usb:/dev/bus/usb','registry.gitlab.com/eliacereda/gapsdk:22.04-3.8.1','bash','-c',cmd],timeout=33,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
print(r.stdout)
Path('/home/vboxuser/tinympc-nanocockpit/releases/ddnd-final-led-live-verification.log').write_text(r.stdout)
