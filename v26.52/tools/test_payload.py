import copy,json,sys,unittest
from pathlib import Path
from verify_payload import verify,read
ROOT=Path(__file__).resolve().parents[1]
class PayloadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.payload={n:(ROOT/n).read_bytes() for n in ['config.json','manifest.json','mod.json','config.schema.json','modmenu.settings.json','README.md','TESTING.md','THIRD_PARTY_NOTICES.txt']}
        cls.payload['FullBright.dll']=(ROOT/'build/windows/x64/debug/FullBright.dll').read_bytes()
    def test_valid_payload(self):
        try:verify(self.payload)
        except ValueError as e:self.fail('valid payload was refused: '+str(e))
    def refuses(self,change):
        verify(self.payload)
        files=copy.deepcopy(self.payload);change(files)
        self.assertNotEqual(files,self.payload,'mutation must change the payload')
        with self.assertRaises(ValueError):verify(files)
    def test_missing_dll(self):self.refuses(lambda f:f.pop('FullBright.dll'))
    def test_private_analysis(self):self.refuses(lambda f:f.update({'analysis.json':b'{}'}))
    def test_wrong_architecture(self):
        def change(f):
            import struct
            b=bytearray(f['FullBright.dll']);pe=struct.unpack_from('<I',b,0x3c)[0];b[pe+4:pe+6]=b'\x4c\x01';f['FullBright.dll']=b
        self.refuses(change)
    def test_missing_export(self):self.refuses(lambda f:f.update({'FullBright.dll':f['FullBright.dll'].replace(b'FB_GetVersion\0',b'FB_NotVersion\0')}))
    def test_wrong_manifest(self):
        self.refuses(lambda f:f.update({'manifest.json':f['manifest.json'].replace(b'0.1.0-test3-leftalt',b'0.1.0-test0')}))
    def test_old_menu_contract(self):
        self.refuses(lambda f:f.update({'modmenu.settings.json':f['modmenu.settings.json'].replace(b'"schemaVersion": 5',b'"schemaVersion": 1')}))
    def test_nonbinding_key_control(self):
        self.refuses(lambda f:f.update({'modmenu.settings.json':f['modmenu.settings.json'].replace(b'"type": "binding"',b'"type": "key"')}))
    def test_sidecar_wrong_path(self):self.refuses(lambda f:f.update({'modmenu.settings.json':f['modmenu.settings.json'].replace(b'/brightness',b'/otherValue')}))
    def test_bound_default(self):self.refuses(lambda f:f.update({'config.json':b'{"enabled":true,"brightness":12,"toggleKey":"F8"}'}))
    def test_float_menu_default(self):self.refuses(lambda f:f.update({'config.json':b'{"enabled":true,"brightness":12.0,"toggleKey":"NONE"}'}))
if __name__=='__main__':unittest.main()
