# FullBright（Bedrock，getGamma 路线）

本文件是中文说明。英文主文档请看 `README.md`。

## 项目简介

这是一个 Bedrock FullBright 插件（Windows DLL）。当前实现采用 `getGamma` Hook 路线，在命中目标调用路径时返回更高亮度值。

仓库内提供了两份已验证的版本配置：

- `profiles/1.26.1.01/FullBright.json`
- `profiles/1.21.132.01/FullBright.json`

这两份 JSON 可以直接作为对应版本的稳定起点。

## 编译

```cmd
cd D:\LeviLauncher_Construct\fullbright
xmake f -m release
xmake
```

输出 DLL：

`build/windows/x64/release/FullBright.dll`

## 运行目录结构

将 DLL 放到目标版本的 mod 目录。插件会读取 DLL 同目录的 `FullBright.json`（或读取 `FullBright` 子目录中的配置，取决于加载器布局）。

常见部署示例：

- `...\mods\FullBright\FullBright.dll`
- `...\mods\FullBright\FullBright.json`

## 如何使用版本配置

1. 根据游戏版本选择配置：
   - 1.26.1.01 -> `profiles/1.26.1.01/FullBright.json`
   - 1.21.132.01 -> `profiles/1.21.132.01/FullBright.json`
2. 复制到你的运行目录。
3. 确保文件名为 `FullBright.json`，并与 `FullBright.dll` 放在一起。

## 主要配置项

- `enabled`
- `hookMode`（应为 `getGamma`）
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

## 兼容性说明

当前仓库确认的稳定配置为：

- Bedrock `1.26.1.01`
- Bedrock `1.21.132.01`

其他版本通常需要重新定位 RVA 和 caller filter。
