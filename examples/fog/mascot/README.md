# Stanford Dragon 答辩吉祥物

用 `examples/fog` 的同一套 Vulkan / froxel 雾 / 太阳阴影管线，把场景精简到只剩 dragon，
固定机位渲染后从交换链读回带 alpha 的 TGA，再缩成透明 PNG。

## 文件

- `config.json`：1200×1200 窗口、jade 材质、默认雾参数。
- `scene.json`：只保留 dragon 材质与摆放；局部雾 `mascot-mist` 只包住龙的下半身，
  让腹部/尾部被雾部分遮住，头部保持清晰。
- `mascot_dragon.png`：纯透明背景，只有龙体上的雾，适合浅色模板。
- `mascot_dragon_mist.png`：额外带一层局部雾薄纱，适合深色答辩模板。
- `*_512.png`：对应的 512×512 缩略图。

## 重跑

在仓库 `assets` 目录下执行（`FOG_MASCOT_*` 都是一次性截帧参数）：

```bash
cd assets
FOG_CONFIG=$PWD/../examples/fog/mascot/mascot-config.json \
FOG_SCENE=$PWD/../examples/fog/mascot/scene.json \
FOG_MASCOT=1 FOG_MASCOT_HALO=1 \
FOG_MASCOT_SIZE=1600 FOG_MASCOT_FRAMES=24 \
FOG_MASCOT_ORBIT=150 FOG_MASCOT_ELEV=12 FOG_MASCOT_DIST=8.2 \
FOG_MASCOT_OUT=/tmp/mascot.tga \
../build/linux/x86_64/debug/fog
```

- `FOG_MASCOT_HALO=0`：不画龙体外围的局部雾薄纱，背景完全透明。
- `FOG_MASCOT_ORBIT/ELEV/DIST/FOV`：环绕角、抬升角、距离、视场角。
- `FOG_MASCOT_FOG/MIST`：全局雾密度（默认 0，只用 `scene.json` 的局部雾）。
- `FOG_MASCOT_EXPOSURE`：曝光 EV。
- `FOG_MASCOT_DEBUG`：`1` 透过率、`2` 散射、`3` 太阳阴影，用于调雾。

TGA 是 32 位 BGRA、top-left origin。缩 PNG 时先按预乘 alpha 缩放，再除以 alpha，
可以避免透明边缘出现黑边。
