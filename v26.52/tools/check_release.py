from pathlib import Path
from verify_payload import verify,PAYLOAD
root=Path(__file__).resolve().parents[1]
files={name:(root/('build/windows/x64/release/FullBright.dll' if name=='FullBright.dll' else name)).read_bytes() for name in PAYLOAD}
verify(files)
print('Release AMD64 DLL, exports, metadata, exact file allowlist, menu and default-config contracts verified')
