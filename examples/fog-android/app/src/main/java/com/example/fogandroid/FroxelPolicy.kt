package com.example.fogandroid

import kotlin.math.max
import java.util.Locale

/**
 * 单档 Froxel（体积雾）精度。
 *
 * 取值与桌面端 `examples/fog/app_ui.cpp` 的低/中/高预设对齐，并在其下方继续延伸：
 * 桌面当前默认即像素步长 8 / 深度 64 / 光源 4 步，实测性能刚刚好，故以它为"高"档基准。
 * 移动端 GPU 远弱于桌面，因此向下追加 极低/很低/低/中 四档，最高档与桌面最高预设齐平，
 * 不提供超过桌面的档位。
 *
 * 两个最低档把 `light_steps` 设为 0：`lighting.glsl` 的 `medium_visibility` 会整体跳过，
 * 局部光不再被介质自身衰减（可见差异：灯光会"穿透"雾）。这是比降网格更有效的省钱手段，
 * 界面摘要里会明确标注，不做静默降质。
 *
 * 修改本表时请同步 `examples/fog/app_ui.cpp` 的预设数组，两侧保持一致。
 */
data class FroxelLevel(
    val index: Int,
    val name: String,
    /// 像素步长：一个 Froxel 覆盖多少屏幕像素，越小网格越细
    val pixelSize: Int,
    /// 深度层数（对数深度切片）
    val depthSlices: Int,
    /// 光源介质衰减步数；0 = 关闭（跳过 medium_visibility 内层循环）
    val lightSteps: Int
) {
    /// 关闭光源介质衰减时需向用户明示的质量取舍
    val disablesMediumAttenuation: Boolean get() = lightSteps <= 0
}

/** 由渲染缓冲尺寸与精度档位推导出的实际 Froxel 网格。 */
data class FroxelGrid(val x: Int, val y: Int, val depth: Int) {
    val voxels: Long get() = x.toLong() * y.toLong() * depth.toLong()

    val label: String get() = "$x × $y × $depth"
}

/** 供启动界面展示的一档：精度 + 在当前缓冲尺寸下的实际网格。 */
data class FroxelOption(val level: FroxelLevel, val grid: FroxelGrid) {
    val label: String
        get() = buildString {
            append(level.name)
            append("  ·  ")
            append(grid.label)
            append("  ·  ")
            append(formatVoxels(grid.voxels))
            // 关闭光源介质衰减是可见的质量取舍，必须写在选项上而不是只写在文档里。
            if (level.disablesMediumAttenuation) append("（关闭光衰减）")
        }

    companion object {
        /// 中文习惯的万进制；低于 1 万直接给精确值，避免小数噪音。
        /// 固定 Locale.US：小数点不随设备地区变成逗号，标签与单测才稳定。
        fun formatVoxels(voxels: Long): String = when {
            voxels < 10_000 -> "$voxels 体素"
            else -> String.format(Locale.US, "%.1f 万体素", voxels / 10_000.0)
        }
    }
}

/** Pure Froxel precision policy: mirrors the clamps in examples/fog/app_setup.cpp. */
object FroxelPolicy {
    /// 六档，成本大致按 2.5~3 倍递进；后三档与桌面预设逐值一致。
    val levels: List<FroxelLevel> = listOf(
        FroxelLevel(0, "极低", 32, 16, 0),
        FroxelLevel(1, "很低", 24, 24, 0),
        FroxelLevel(2, "低", 16, 32, 1),
        FroxelLevel(3, "中", 12, 48, 2),
        FroxelLevel(4, "高", 8, 64, 4),
        FroxelLevel(5, "极高", 4, 96, 8)
    )

    /// 设备实测帧率远低于桌面，默认给最省算力的一档；用户可自行调高。
    const val DEFAULT_INDEX = 0

    // 与 examples/fog/app_setup.cpp / app_systems.cpp 的 clamp 完全一致
    const val MIN_PIXEL_SIZE = 4
    const val MAX_PIXEL_SIZE = 32
    const val MIN_DEPTH_SLICES = 16
    const val MAX_DEPTH_SLICES = 96
    // lighting.glsl: clamp(int(ubo.volume_params.w), 0, 8)；0 表示跳过内层循环
    const val MIN_LIGHT_STEPS = 0
    const val MAX_LIGHT_STEPS = 8
    const val MAX_GRID_X = 192
    const val MAX_GRID_Y = 128

    fun levelFor(index: Int): FroxelLevel = levels.getOrElse(index) { levels[DEFAULT_INDEX] }

    fun optionsFor(width: Int, height: Int): List<FroxelOption> =
        levels.map { FroxelOption(it, gridFor(width, height, it)) }

    /**
     * 按缓冲尺寸推导网格，逐字复刻 C++ 侧：
     * `grid = clamp(ceil(max(size,1) / pixel_size), 1, 192|128)`，深度取 clamp 后的层数。
     */
    fun gridFor(width: Int, height: Int, level: FroxelLevel): FroxelGrid {
        val pixel = level.pixelSize.coerceIn(MIN_PIXEL_SIZE, MAX_PIXEL_SIZE)
        val depth = level.depthSlices.coerceIn(MIN_DEPTH_SLICES, MAX_DEPTH_SLICES)
        return FroxelGrid(
            ceilDiv(max(width, 1), pixel).coerceIn(1, MAX_GRID_X),
            ceilDiv(max(height, 1), pixel).coerceIn(1, MAX_GRID_Y),
            depth
        )
    }

    fun ceilDiv(value: Int, divisor: Int): Int {
        require(divisor > 0)
        return (value + divisor - 1) / divisor
    }
}
