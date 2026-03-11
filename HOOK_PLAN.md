# FullBright Hook Plan

This document defines the first real implementation priority for the new `fullbright` branch.

## Goal

Replace the old `FullNight.cpp` production line with a focused runtime hook path centered on the light-texture update family.

## Priority chain

### 1. `LightTextureImageBuilderFactory::registerLightImageBuilders` / LightTexture builder family

Why first:

- Stronger Bedrock-specific clue than generic texture names
- Public Bedrock SDK headers expose `Level::getLightTextureImageBuilderFactory()` and `LightTextureImageBuilderFactory::registerLightImageBuilders(...)`
- Open Bedrock SDK notes show `registerLightImageBuilders` as a concrete client-side symbol family in 1.21.x
- This is closer to the actual light-texture build path than broad texture fetch hooks

Expected strategy:

```text
pattern scan
-> locate LightTextureImageBuilderFactory / registerLightImageBuilders family
-> prove runtime hit
-> inspect concrete builder/update path
```

Current first concrete live candidate:

- Bedrock SDK clue: `registerLightImageBuilders` noted around `0x3FE9B30` on 1.21.0.3
- Current live Minecraft IDA: `0x3FE9B30` resolves to `sub_3FE9AC0`
- This is the first family member worth seeding with a real scan pattern before falling back to broader texture routes
- Current runtime result: a scan-derived live candidate (`0x3FEA730`) was attachable but produced no runtime hit, so the branch now moves to the next fallback route instead of continuing to rotate builder-family candidates blindly

### 2. `mce::TextureGroup::getTexture`

Why first:

- Strong Bedrock-side evidence that this is a practical texture interception point
- Suitable for replacing or forcing the specific light/lightmap-like texture data path
- Keeps the implementation close to the texture consumer side without depending on external shader systems

Expected strategy:

```text
pattern scan
-> hook TextureGroup::getTexture
-> identify light/lightmap-like texture request
-> replace or clamp returned texture data
```

Current best live family anchors for this route:

- Primary family anchor: `mce::TextureGroup::loadTextureAsync`-related string cluster (`0x94B3494`, `0x94B3564`)
- Fallback family anchor: `mce::TextureGroup` + `ResourceLocation` / atlas resolver cluster (`0x94D24B2`, `0x94D273B`, `0x94D2C23`)

Reason for this refinement:

- The visible `getTexture` string hits in the current live binary map to unrelated systems and are not a safe first pattern source.
- The async load / resource-location resolver family is still inside the same `TextureGroup` texture acquisition path but appears more stable for first pattern work.

### 3. `TickingTextureStage::render`

Why second:

- A strong per-update choke point for texture/light update timing
- Useful when texture acquisition is too broad but update/render-stage timing is stable

Expected strategy:

```text
pattern scan
-> hook TickingTextureStage::render
-> apply fullbright state once per update stage
```

### 4. Visibility/light coordinator fallback

Why third:

- Useful when newer texture-stage hooks are unavailable or drift badly
- More version-sensitive than the first two routes

Expected strategy:

```text
pattern scan
-> hook visibility/light update coordinator
-> force light update result before render consumption
```

### 5. Gamma-only fallback

Why last:

- Easy to implement but usually weakest and least authoritative
- Should remain only a debug/fallback path, not the primary route

## Minimal implementation milestones

### Milestone A

- Add signature profile and hook install path for the `LightTextureImageBuilderFactory::registerLightImageBuilders` family
- Add one debug log proving the hook is hit

### Milestone B

- Add runtime filtering or follow-up hook selection to identify the concrete light-texture update path beneath that family
- Add one minimal force path (replace or clamp light texture result)

### Milestone C

- Add `mce::TextureGroup::getTexture` and `TickingTextureStage::render` as fallback hooks
- Add runtime selection/fallback logic between the three routes

## Non-goals for the first implementation

- Do not bring the old `lightmapPatch` frontier tuning into this branch
- Do not make RenderDragon file overrides part of the branch core
- Do not reintroduce speculative helper/stage/core-loop detour rotation from `FullNight`

## Current branch status

`fullbright` is currently a compile-ready scaffold with config + logging only.

The next real implementation step is:

```text
pattern scan
-> hook LightTextureImageBuilderFactory / registerLightImageBuilders family
-> seed first live candidate from 0x3FE9B30 / sub_3FE9AC0
-> prove runtime hit
```
