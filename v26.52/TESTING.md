# Manual checks / 人工測試

This is a prerelease. Offline tests do not prove actual rendering or shortcut
eligibility in every screen. Keep a rollback copy outside active mods.

1. In exact Minecraft 1.26.52.03, check a normally dark area with FullBright enabled.
2. Use JSON, or a schema-v5-capable ModMenu, to disable/re-enable and change integer
   brightness. Confirm disabled returns the vanilla value and Apply updates live.
3. Restart and check that the saved values remain. Defaults are enabled, 12, NONE.
4. With a compatible menu, check Cancel, Apply/reopen and Esc-to-Unbound. Backspace
   is a bindable key in this new binding contract, not the old clear shortcut.
5. Try Ctrl+K, Shift+K and LeftAlt+K one at a time. Complete combinations should
   toggle once; bare K, extra modifiers, held repeat and a late modifier should not.
6. Check chat, inventory, ModMenu and Alt-Tab. Return with a held key: it should
   require a release and fresh press. Do not use Windows-reserved shortcuts.
7. Confirm no other plugin was replaced. Avoid duplicate Gamma hooks. On trouble,
   exit and restore the previous DLL, manifest, sidecar and configuration together.

RTX, deferred/custom rendering and other Minecraft versions remain unverified.
The older test1's manual acceptance does not accept this new DLL. The old public
ModMenu 0.5.0-test1 does not understand this release's v5 description; use JSON
unless you already have a matching menu. No menu DLL is bundled.

本版為預發佈：離線檢查不代表已完成實際畫面驗收。請先備份到 mods 外，依序測試
亮度、關閉還原、即時調整、重啟保留、取消／套用、Esc 無綁定與 Ctrl／Shift／左 Alt
組合。Backspace 在新版可綁，不是清除鍵。持續按住、先主鍵再補修飾鍵、失焦返回
不應誤切換，並檢查聊天、背包及選單。舊公開版 ModMenu 不支援此 v5 描述檔，
可先使用 JSON；此包不包含其他插件。回退時一併還原 DLL、manifest、描述檔及設定。
