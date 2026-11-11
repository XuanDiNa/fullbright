# FullBright 0.1.0-test3-leftalt

[English / 繁體中文](https://github.com/XuanDiNa/fullbright#readme)

Windows x64 · Minecraft Bedrock **1.26.52.03** · LeviLauncher native DLL.
This folder is the modern build, not the legacy project in the repository root.

## Installation / 安裝

Exit Minecraft. Back up FullBright outside active mods, then merge the ZIP's
`FullBright` folder into your version's `mods`. Keep your existing `config.json`
when updating, preserve the launcher's enabled/disabled manifest state, and load
only one DLL. Do not reuse the legacy `FullBright.json` profiles.

完全退出遊戲，備份到 mods 以外，再合併 FullBright 資料夾。更新時保留原有
`config.json` 與啟動器啟用／停用狀態，只啟用一份 DLL，不沿用舊位址設定檔。

```json
{
  "enabled": true,
  "brightness": 12,
  "toggleKey": "NONE"
}
```

Brightness is gamma **1–25**, not a percentage. JSON reloads about every 300 ms.
Shortcut examples: `F6`, `CTRL+K`, `SHIFT+K`, `LALT+K`, `CTRL+SHIFT+LALT+K`.
Hold modifiers before a fresh main-key press. Right Alt/AltGr is not Left Alt.

ModMenu is optional. The v5 description requires a v5-capable menu, such as
`0.6.0-test5-leftalt`; the older public `0.5.0-test1` cannot open it. Use JSON
otherwise. Apply saves; Cancel discards; Esc clears capture, Backspace is bindable.
Use integer brightness in Configure. No ModMenu DLL or other plugin is included.

亮度為 Gamma 1–25，非百分比；預設無綁定。選單需要支援 v5 的 ModMenu，
舊公開版 0.5.0-test1 無法開啟此設定頁，仍可使用 JSON；不隨附或修改 ModMenu。

## Limits / 限制

Gamma-only route; RTX/deferred/custom renderers are not guaranteed. Exact image
and entry checks fail closed. Shortcut focus/hidden-cursor checks are not a full
native screen classifier. Check chat/UI/Alt-Tab and avoid conflicting keys.
Live unloading/reinjection is unsupported. Read `FullBright.log` for diagnostics.

The old test1 brightness/settings behavior was manually accepted. This new DLL
and its combination changes are offline-verified, not yet manually accepted.
Gamma 路徑不保證光追或延遲／自訂渲染；新 DLL 仍待人工驗收，不沿用 test1 的驗收結論。

## Build / 編譯

Requires Windows x64, Visual Studio 2022 C++/Windows SDK, xmake and Python 3.
nlohmann/json 3.11.3 is vendored; xmake resolves static MinHook 1.3.4.
No sibling project, game files, IDA or LeviLamina SDK is required to build.
From this folder:

```powershell
xmake f -P . -p windows -a x64 -m release -y
xmake build -P . -y
```

Output: `build/windows/x64/release/FullBright.dll`. Always use `-P .` so xmake
does not select the legacy parent. Run both modes and five standalone suites:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/verify.ps1
```

Optional `-GameImagesRoot <directory>` reads locally owned exact 1.26.52.03
`Minecraft.Windows.exe.orig` and `Minecraft.Windows.exe` to test full image and
entry-byte guards; it never executes or writes them. Without them, private-image
checks are explicitly skipped. ModMenu cross-project integration tests are not
part of this standalone source checkout. The checked-in `NativeProfile.inl`
contains the fixed safety profile; private analysis tools/data are not needed.

Source/toolchain/build paths can change DLL hashes. Clean builds are verification
artifacts, not replacements for the frozen DLL in the public download.
See [testing](TESTING.md), [release notes](https://github.com/XuanDiNa/fullbright/releases/tag/v0.1.0-test3-leftalt) and
[dependency notices](THIRD_PARTY_NOTICES.txt). This publication does not add a
new license grant for project code; dependencies retain their supplied licenses.

本資料夾可獨立編譯，不需要相鄰專案或 IDA。上方指令驗證 Debug／Release；
不提供本機遊戲映像時會略過私有映像檢查。重新編譯不是已凍結發佈 DLL 的替代品。
