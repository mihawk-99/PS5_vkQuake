import sys,time,importlib.util
sys.path.insert(0,'tools'); sys.path.insert(0,'../PS5_Vulkan/tools')
import ps5_console as c
from ps5_ftp import connect
s=importlib.util.spec_from_file_location('d','tools/deploy-title.py');d=importlib.util.module_from_spec(s);s.loader.exec_module(d)
cfg=c.load_settings(); path='/data/homebrew/PPSA99010/trace.txt'
assert 'count=0' in c.ps5vkctl_command(cfg,'procs')
with connect(**d.load_settings()) as f:
    base=f.size(path)
    print('trace bytes before', base, flush=True)
    t0=time.monotonic(); print('launch reply', c.ps5vkctl_command(cfg,'launch PPSA99010')[:80], f'{time.monotonic()-t0:.2f}s', flush=True)
    last=base; seen_present=False
    while time.monotonic()-t0 < float(sys.argv[1]):
        try: n=f.size(path)
        except Exception as e: n=last
        if n!=last:
            print(f'{time.monotonic()-t0:7.2f}s trace +{n-base} bytes', flush=True); last=n
        time.sleep(0.25)
print(c.ps5vkctl_command(cfg,'kill PPSA99010')[:100])
