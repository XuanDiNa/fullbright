# FullBright

[English](#english) · [繁體中文](#繁體中文)

## English

Client-side gamma brightness for Minecraft Bedrock and LeviLauncher.
No LeviLamina API, resource pack, night-vision effect or server component is required.

**Windows x64 · Minecraft 1.26.52.03**

**0.1.0-test3-leftalt — Prerelease**

[Download](https://github.com/XuanDiNa/fullbright/releases/tag/v0.1.0-test3-leftalt) ·
[Report an issue](https://github.com/XuanDiNa/fullbright/issues)

### Features

- Enabled by default, with adjustable gamma brightness from **1 to 25**; default **12**.
- Disabling returns the original gamma value without rewriting game brightness settings.
- Hot-reloadable JSON and optional ModMenu controls for Enabled, Brightness and Toggle key.
- Optional single-key or Ctrl / Shift / Left Alt combination toggle; **unbound by default**.
- Fresh-press handling: holding a key does not repeatedly toggle.
- English, Traditional Chinese and Simplified Chinese setting labels.
- Exact game-image and native-entry checks; unsupported or conflicting images remain inactive.

### Installation

1. Exit Minecraft and back up the existing FullBright folder **outside** the active `mods`.
2. Merge the ZIP's `FullBright` folder into your 1.26.52.03 `mods` directory.
   Keep only one DLL copy and preserve your existing **config.json** when updating.
3. Preserve the launcher's enabled/disabled manifest state, then restart via LeviLauncher.

The ZIP contains **FullBright only**. It does not replace ModMenu or any other plugin.
Do not reuse legacy `FullBright.json` profiles with this version.

### Configuration

```json
{
  "enabled": true,
  "brightness": 12,
  "toggleKey": "NONE"
}
```

JSON changes are polled about every 300 ms. `NONE` means no shortcut; examples are
`F6`, `CTRL+K`, `SHIFT+K`, `LALT+K` and `CTRL+SHIFT+LALT+K`.
Required modifiers must precede a fresh main-key press. Right Alt/AltGr does not
substitute for Left Alt, and Windows-reserved shortcuts may be consumed by the OS.

The included **schema v5** description requires a matching v5-capable ModMenu
(such as `0.6.0-test5-leftalt`). The older public ModMenu `0.5.0-test1` cannot
open this new Configure page; use JSON instead until you have a matching menu.
ModMenu is optional and is not bundled. In a supported Configure page, Apply saves,
Cancel discards and Esc during capture selects Unbound; Backspace is bindable.
Use integer brightness values for the menu, e.g. `12`, not `12.0`.

Shortcut eligibility uses foreground focus and the hidden OS cursor, not a
complete native screen classifier. Check chat, inventory, menus and Alt-Tab in
your setup. Choose a shortcut that does not conflict with other plugins.

### Notes and feedback

This is a gamma override, not a lightmap or shader replacement. RTX, deferred/custom
renderers and all darkness/resource-pack combinations are not guaranteed.
Only the exact 1.26.52.03 builds represented by the included safety profile are supported.

The original `0.1.0-test1` brightness, settings and single-key controls were manually
accepted. This new combination-binding DLL has offline verification; its new
in-game acceptance is **pending**. Do not treat the old test results as approval
of the new binary.

`FullBright.log` records startup and safe refusal. Hook hits alone do not prove
visible brightness. Do not load another plugin hooking the same gamma entry.
Live unloading/reinjection is unsupported; fully restart Minecraft.

Modern source and build instructions: [v26.52](v26.52/README.md).
[Release notes](v26.52/docs/RELEASE.md) · [Testing](v26.52/TESTING.md) ·
[Dependency notices](v26.52/THIRD_PARTY_NOTICES.txt).
The root `src/`, `profiles/`, `xmake.lua` and `HOOK_PLAN.md` are legacy history,
not the build or compatibility claims for this release. No new license grant
for legacy/project code is implied; third-party files retain their supplied licenses.

---

## 繁體中文

適用於 Minecraft 基岩版／LeviLauncher 的本機 Gamma 亮度插件。
不需要 LeviLamina API、資源包、夜視效果或服務端組件。

**Windows x64 · Minecraft 1.26.52.03**

**0.1.0-test3-leftalt — 預發佈版**

[下載](https://github.com/XuanDiNa/fullbright/releases/tag/v0.1.0-test3-leftalt) ·
[回報問題](https://github.com/XuanDiNa/fullbright/issues)

### 功能

- 預設啟用，Gamma 亮度可調 **1–25**，預設 **12**，不是百分比。
- 關閉即回傳原版亮度，不改寫遊戲亮度設定。
- JSON 即時重讀，可選用 ModMenu 調整開關、亮度及快捷鍵。
- 支援單鍵、Ctrl／Shift／左 Alt 組合鍵，**預設無綁定**。
- 按住不會連續切換，須放開後重新按下。
- 設定標籤提供英文、繁體中文與簡體中文。
- 精確映像與原生入口檢查，不符合或有 Hook 衝突時不啟用。

### 安裝

1. 完全退出遊戲，將原有 FullBright 備份到有效 mods 以外。
2. 合併 ZIP 的 `FullBright` 到 1.26.52.03 的 `mods`；只啟用一份 DLL，
   更新時**保留原有 config.json**。
3. 保留啟動器啟用／停用 manifest 狀態，再透過 LeviLauncher 重啟。

此包**只有 FullBright**，不替換 ModMenu 或其他插件。
不要沿用舊版的 `FullBright.json` 位址設定檔。

### 設定

預設 JSON 如上方範例：約每 300 毫秒重讀，`NONE` 為無綁定；
可用 `F6`、`CTRL+K`、`LALT+K`、`CTRL+SHIFT+LALT+K` 等值。
先按住修飾鍵再按主鍵；右 Alt／AltGr 不代替左 Alt，系統可能攔截保留組合。

本版描述檔是 **schema v5**，需要相容的 ModMenu，例如 `0.6.0-test5-leftalt`。
較舊的公開版 `0.5.0-test1` 無法開啟這份設定頁，請先用 JSON 調整；
ModMenu 非必要依賴，也不隨附於本包。相容設定頁中 Apply 儲存、Cancel 捨棄，
擷取時 Esc 設為無綁定，Backspace 可綁。選單亮度使用整數，例如 `12`，不是 `12.0`。

快捷鍵只檢查遊戲前景及系統游標隱藏，並非完整的原生畫面辨識；
請確認自己的聊天、背包、選單與 Alt-Tab 情境，並避免與其他插件共用快捷鍵。

### 注意與回饋

本版是 Gamma 路徑，不是光照貼圖或著色器替換，不保證光追、延遲／自訂渲染及
所有無光／資源包組合；只支援內附安全描述涵蓋的精確 1.26.52.03 映像。

先前 `0.1.0-test1` 的亮度、設定及單鍵功能已人工驗收；這份組合鍵更新 DLL
有離線驗證，新的遊戲內驗收仍**待完成**，不把舊結果當成新 DLL 已驗收。

日誌位於 `FullBright.log`，Hook 命中不等於畫面效果已確認。
不要同時啟用另一個攔截相同 Gamma 入口的插件；不支援即時卸載或重複注入，請完整重啟。

新版本原始碼與編譯方式：[v26.52](v26.52/README.md)；
[發佈說明](v26.52/docs/RELEASE.md) · [測試](v26.52/TESTING.md) ·
[第三方聲明](v26.52/THIRD_PARTY_NOTICES.txt)。
根目錄 `src/`、`profiles/`、`xmake.lua` 及 `HOOK_PLAN.md` 保留為歷史資料，
不是本版的編譯入口或相容性聲明；此次公開不另行改變專案／歷史程式碼授權。
