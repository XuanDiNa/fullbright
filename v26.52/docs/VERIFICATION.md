# Release verification / 發佈驗證

Publication candidate: `0.1.0-test3-leftalt`, checked on 2026-11-11.

- Clean standalone Debug and Release builds completed.
- Each mode passed Core 167, ConfigFile 13, Profile 218, Native 13 and Smoke 5
  checks: **832 checks total, zero failures**.
- Profile checks read both locally owned exact 1.26.52.03 images without running
  or modifying Minecraft. These images are not distributed.
- **11 payload mutation tests** passed, including wrong versions, architecture,
  exports, menu contracts, config paths and defaults.
- The existing local FullBright/ModMenu integration executable passed **35
  checks**. This cross-project suite is not part of the standalone public build.
- All 11 runtime source/header files match the existing local candidate.
- The released DLL is the frozen candidate, not the clean-build output:
  `F112642E54CB50E44BDD1B80D9E755C30E2851E4199EEC98A5BC97BC106FC251`
  (420,864 bytes).

Reproduce standalone checks with `tools/verify.ps1`; optionally provide locally
owned game images using `-GameImagesRoot`. Missing images are explicitly skipped.
Build paths/toolchains may change output hashes. See [manual checks](../TESTING.md).

本次 Debug／Release 共 832 項檢查、11 項套件測試及本機既有的 35 項選單整合
檢查皆通過。發佈仍使用原凍結 DLL。這些是離線驗證，不代表新版 DLL 已完成
遊戲內人工驗收；不沿用舊 test1 的驗收結論。
