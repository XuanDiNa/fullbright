# FullBright (Bedrock, getGamma route)

English is the primary documentation. For Chinese, see `README.zh-CN.md`.

## Overview

This project is a Bedrock FullBright plugin built as a Windows DLL. The current implementation uses a `getGamma` hook route and forces a higher runtime gamma value when the target call path is detected.

The repository includes two validated profile JSON files:

- `profiles/1.26.1.01/FullBright.json`
- `profiles/1.21.132.01/FullBright.json`

These profiles are intended as stable starting points for the corresponding game versions.

## Build

```cmd
cd D:\LeviLauncher_Construct\fullbright
xmake f -m release
xmake
```

Output DLL:

`build/windows/x64/release/FullBright.dll`

## Runtime layout

Deploy the built DLL to your mod folder for the target Bedrock version. The plugin reads `FullBright.json` next to the DLL (or inside a `FullBright` subfolder, depending on loader layout).

Typical deployment examples:

- `...\mods\FullBright\FullBright.dll`
- `...\mods\FullBright\FullBright.json`

## Using the versioned profiles

1. Choose the profile that matches your game version:
   - 1.26.1.01 -> `profiles/1.26.1.01/FullBright.json`
   - 1.21.132.01 -> `profiles/1.21.132.01/FullBright.json`
2. Copy that file to your runtime mod directory.
3. Rename/copy it as `FullBright.json` beside `FullBright.dll`.

## Config keys

Main keys used by current implementation:

- `enabled`
- `hookMode` (should be `getGamma`)
- `hookTargetRva`
- `hookPattern`
- `hookPatternOffset`
- `candidateTrialIndex`
- `callerFilterRva`
- `phaseGateEnabled`
- `phaseGateFallbackToCaller`
- `phaseScreenPattern`
- `phaseLevelPattern`
- `patternScanEnabled`
- `forceBrightness`

## Compatibility note

This repository currently ships validated profiles for:

- Bedrock `1.26.1.01`
- Bedrock `1.21.132.01`

Other versions may require new RVAs and caller filters.
