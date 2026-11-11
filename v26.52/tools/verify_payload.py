"""Standalone artifact boundary checks; no DLL execution or game access."""
import json
import struct
from pathlib import Path
PAYLOAD={'FullBright.dll','config.json','manifest.json','mod.json','config.schema.json','modmenu.settings.json','README.md','TESTING.md','THIRD_PARTY_NOTICES.txt'}
VERSION='0.1.0-test3-leftalt'
def verify(files):
    try:
        def need(condition,message):
            if not condition:raise ValueError(message)
        need(set(files)==PAYLOAD,'Unexpected or missing payload file')
        need(all(0<len(v)<4*1024*1024 for v in files.values()),'Empty or oversized payload')
        b=files['FullBright.dll'];need(b[:2]==b'MZ','Not a PE image')
        pe=struct.unpack_from('<I',b,0x3c)[0];need(b[pe:pe+4]==b'PE\0\0','Invalid PE signature')
        machine,count=struct.unpack_from('<HH',b,pe+4);size,flags=struct.unpack_from('<HH',b,pe+20)
        need(machine==0x8664 and flags&0x2000 and 0<count<97 and size==240,'Not an AMD64 DLL')
        opt=pe+24;need(struct.unpack_from('<H',b,opt)[0]==0x20b,'Not a PE32+ DLL')
        rows=[]
        for i in range(count):
            at=opt+size+40*i;vs,rva,rs,raw=struct.unpack_from('<IIII',b,at+8);rows.append((rva,rs,raw))
            need(raw+rs<=len(b),'Unbounded DLL section')
        def raw(rva,n):
            for start,size,offset in rows:
                if start<=rva and rva+n<=start+size:return offset+rva-start
            raise ValueError('Unbounded DLL RVA')
        def string(rva):
            at=raw(rva,1);end=b.find(b'\0',at,at+256);need(end>=at,'Unbounded DLL string');return b[at:end].decode('ascii')
        ex,exsize=struct.unpack_from('<II',b,opt+112);need(exsize>=40,'Missing export directory')
        at=raw(ex,40);n=struct.unpack_from('<I',b,at+24)[0];names=struct.unpack_from('<I',b,at+32)[0];need(n==3,'Unexpected export count')
        names_at=raw(names,n*4)
        exports={string(struct.unpack_from('<I',b,names_at+4*i)[0]) for i in range(n)}
        need(exports=={'FB_GetStatus','FB_GetVersion','FB_Shutdown'},'Wrong diagnostic exports')
        need(VERSION.encode()+b'\0' in b,'Binary version absent')
        need(not any(s in b for s in [b'LeviLamina',b'D:\\bedrock_Tool',b'D:\\Stroll',b'inner-analysis-only.exe']),'Private path or forbidden dependency in DLL')
        def j(name):return json.loads(files[name].decode('utf-8'))
        manifest=j('manifest.json');mod=j('mod.json');config=j('config.json');schema=j('config.schema.json');menu=j('modmenu.settings.json')
        need(manifest==dict(name='FullBright',entry='FullBright.dll',version=VERSION,type='preload-native'),'Wrong loader manifest')
        need(mod['id']=='FullBright' and mod['dll']=='FullBright.dll' and mod['version']==VERSION,'Wrong display metadata')
        need(config==dict(enabled=True,brightness=12,toggleKey='NONE') and type(config['brightness']) is int,'Wrong defaults or noninteger menu value')
        need(menu['schemaVersion']==5 and menu['plugin']==dict(id='FullBright',entry='FullBright.dll',versions=[VERSION]),'Wrong menu matching contract')
        need(menu['config']==dict(file='config.json',reload='hot'),'Wrong reload declaration')
        fields={x['path']:x for x in menu['fields']};need(len(fields)==3 and set(fields)=={'/enabled','/brightness','/toggleKey'},'Wrong menu paths')
        need(fields['/enabled']['type']=='boolean' and fields['/enabled']['default'] is True,'Wrong enabled control')
        light=fields['/brightness'];need(light['type']=='integer' and light['min']==1 and light['max']==25 and light['default']==12,'Wrong brightness control')
        key=fields['/toggleKey'];need(key['type']=='binding' and key['default']=='NONE','Missing combination/unbound support')
        need(schema['properties']['brightness']['minimum']==1 and schema['properties']['brightness']['maximum']==25 and schema['additionalProperties'] is False,'Wrong JSON schema bounds')
        return exports
    except (KeyError,TypeError,UnicodeError,struct.error,json.JSONDecodeError) as e:
        raise ValueError('Malformed artifact: '+str(e)) from e
def read(directory):
    return {name:(Path(directory)/name).read_bytes() for name in PAYLOAD}
