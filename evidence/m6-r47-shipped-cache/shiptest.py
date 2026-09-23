import sys,io,importlib.util
from pathlib import Path
sys.path.insert(0,'tools')
from ps5_ftp import connect, upload_atomic, remove_if_present
s=importlib.util.spec_from_file_location('d','tools/deploy-title.py');d=importlib.util.module_from_spec(s);s.loader.exec_module(d)
tag=sys.argv[2]; base='/data/homebrew/PPSA99010'; test=base+'/ps5vk-shader-cache-shiptest'
with connect(**d.load_settings()) as f:
    if sys.argv[1]=='stage':
        for p in (test, test+'/'+tag):
            try: f.mkd(p)
            except Exception as e: assert str(e).startswith('550'),e
        files=sorted(Path('build/shader-cache/'+tag).glob('*.bin'))
        for p in files:
            upload_atomic(f,p,test+'/'+tag+'/'+p.name); assert d.remote_bytes(f,test+'/'+tag+'/'+p.name)==p.read_bytes()
        f.storbinary('STOR '+base+'/ps5vk-shader-cache-dir.txt', io.BytesIO(b'/app0/ps5vk-shader-cache-shiptest\n'))
        print('staged',len(files),'entries into a fresh test base, flag set')
    else:
        remove_if_present(f, base+'/ps5vk-shader-cache-dir.txt')
        lines=[]; f.retrlines('LIST '+test+'/'+tag, lines.append)
        for l in lines:
            n=l.split()[-1]
            if n not in ('.','..'): remove_if_present(f,test+'/'+tag+'/'+n)
        for p in (test+'/'+tag, test):
            try: f.rmd(p)
            except Exception as e: print('rmd',p,e)
        names=[]; f.retrlines('LIST '+base, names.append)
        print('removed flag and test base; remaining shiptest/flag entries:', [l for l in names if 'shiptest' in l or 'cache-dir' in l])
