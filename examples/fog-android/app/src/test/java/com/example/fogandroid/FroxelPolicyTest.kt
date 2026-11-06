package com.example.fogandroid

import org.junit.Assert.*
import org.junit.Test

class FroxelPolicyTest {
    private fun level(index: Int) = FroxelPolicy.levelFor(index)

    @Test fun levelsRefineMonotonicallyAndMatchDesktopPresets() {
        // 桌面端当前默认即像素步长 8 / 深度 64 / 光源 4 步，实测性能刚刚好。
        val desktop = level(4)
        assertEquals(8, desktop.pixelSize)
        assertEquals(64, desktop.depthSlices)
        assertEquals(4, desktop.lightSteps)
        // 桌面最高预设（4/96/8）作为最高档，不提供超过桌面的档位。
        val top = level(5)
        assertEquals(4, top.pixelSize)
        assertEquals(96, top.depthSlices)
        assertEquals(8, top.lightSteps)
        // 桌面最低预设（16/32）在第三档，下方还有两档更粗的移动端专用档。
        assertEquals(16, level(2).pixelSize)
        assertEquals(32, level(2).depthSlices)
    }

    @Test fun costGrowsMonotonicallyAcrossEveryLevel() {
        var previousVoxels = 0L
        var previousPixel = Int.MAX_VALUE
        var previousSteps = -1
        for (option in FroxelPolicy.optionsFor(1280, 720)) {
            assertTrue("voxels must grow: ${option.grid}", option.grid.voxels > previousVoxels)
            assertTrue("pixel step must shrink: ${option.level}", option.level.pixelSize < previousPixel)
            assertTrue("light steps must not shrink: ${option.level}",
                option.level.lightSteps >= previousSteps)
            previousVoxels = option.grid.voxels
            previousPixel = option.level.pixelSize
            previousSteps = option.level.lightSteps
        }
    }

    @Test fun theTwoCheapestLevelsDisableMediumAttenuation() {
        assertTrue(level(0).disablesMediumAttenuation)
        assertTrue(level(1).disablesMediumAttenuation)
        assertFalse(level(2).disablesMediumAttenuation)
        assertFalse(level(5).disablesMediumAttenuation)
        // 0 是 lighting.glsl 认可的合法值（整体跳过 medium_visibility）。
        assertEquals(0, level(0).lightSteps)
        assertEquals(FroxelPolicy.MIN_LIGHT_STEPS, 0)
        assertEquals(FroxelPolicy.MAX_LIGHT_STEPS, 8)
    }

    @Test fun unknownLevelFallsBackToTheDefault() {
        assertEquals(FroxelPolicy.DEFAULT_INDEX, level(-1).index)
        assertEquals(FroxelPolicy.DEFAULT_INDEX, level(6).index)
        assertEquals(FroxelPolicy.DEFAULT_INDEX, level(99).index)
    }

    @Test fun gridMirrorsTheCppCeilAndClamps() {
        // 1280x720 buffer, pixel step 8 -> 160x90
        assertEquals(FroxelGrid(160, 90, 64), FroxelPolicy.gridFor(1280, 720, level(4)))
        // Odd sizes round up, not down.
        assertEquals(FroxelGrid(161, 91, 64), FroxelPolicy.gridFor(1281, 721, level(4)))
        // Zero/negative sizes behave like max(size,1) in C++.
        assertEquals(FroxelGrid(1, 1, 16), FroxelPolicy.gridFor(0, 0, level(0)))
        assertEquals(FroxelGrid(1, 1, 16), FroxelPolicy.gridFor(-100, -100, level(0)))
    }

    @Test fun gridIsCappedAtTheDeviceBudget() {
        // 1920x1080 at pixel step 4 would be 480x270; the C++ clamp caps it at 192x128.
        assertEquals(FroxelGrid(192, 128, 96), FroxelPolicy.gridFor(1920, 1080, level(5)))
        // The cap is per-axis: a very wide buffer saturates x while y stays below it.
        assertEquals(FroxelGrid(192, 90, 96), FroxelPolicy.gridFor(4000, 360, level(5)))
        // The coarsest step stays far below the cap on a 720p buffer.
        assertEquals(FroxelGrid(40, 23, 16), FroxelPolicy.gridFor(1280, 720, level(0)))
    }

    @Test fun outOfRangeLevelParametersAreClampedLikeTheRenderer() {
        // pixel step 64 -> clamped to 32 -> ceil(1280/32)=40, ceil(720/32)=23
        assertEquals(FroxelGrid(40, 23, 64),
            FroxelPolicy.gridFor(1280, 720, FroxelLevel(9, "x", 64, 64, 4)))
        // pixel step 1 -> clamped to 4 -> ceil(1280/4)=320 and ceil(720/4)=180,
        // both above their per-axis grid cap, so both saturate.
        assertEquals(FroxelGrid(192, 128, 64),
            FroxelPolicy.gridFor(1280, 720, FroxelLevel(9, "x", 1, 64, 4)))
        // depth 1 -> clamped to 16; depth 999 -> clamped to 96
        assertEquals(FroxelGrid(80, 45, 16),
            FroxelPolicy.gridFor(1280, 720, FroxelLevel(9, "x", 16, 1, 4)))
        assertEquals(FroxelGrid(80, 45, 96),
            FroxelPolicy.gridFor(1280, 720, FroxelLevel(9, "x", 16, 999, 4)))
    }

    @Test fun voxelCountsStepRoughlyGeometrically() {
        val grids = FroxelPolicy.levels.map { FroxelPolicy.gridFor(1280, 720, it) }
        assertEquals(6, grids.size)
        assertEquals(40L * 23 * 16, grids[0].voxels)
        assertEquals(54L * 30 * 24, grids[1].voxels)
        assertEquals(80L * 45 * 32, grids[2].voxels)
        assertEquals(107L * 60 * 48, grids[3].voxels)
        assertEquals(160L * 90 * 64, grids[4].voxels)
        assertEquals(192L * 128 * 96, grids[5].voxels)
        // Every step is between 2x and 4x the previous one: no cliff, no near-duplicate.
        for (i in 1 until grids.size) {
            val ratio = grids[i].voxels.toDouble() / grids[i - 1].voxels
            assertTrue("step $i ratio=$ratio", ratio > 2.0 && ratio < 4.0)
        }
        // C++ sizes the SSBO with VkDeviceSize casts; the count itself still fits an int.
        assertTrue(grids[5].voxels <= Int.MAX_VALUE.toLong())
    }

    @Test fun optionLabelsCarryTheGridVoxelCountAndQualityTradeOff() {
        val options = FroxelPolicy.optionsFor(1280, 720)
        assertEquals(6, options.size)
        assertEquals("极低  ·  40 × 23 × 16  ·  1.5 万体素（关闭光衰减）", options[0].label)
        assertEquals("很低  ·  54 × 30 × 24  ·  3.9 万体素（关闭光衰减）", options[1].label)
        assertEquals("低  ·  80 × 45 × 32  ·  11.5 万体素", options[2].label)
        assertEquals("中  ·  107 × 60 × 48  ·  30.8 万体素", options[3].label)
        assertEquals("高  ·  160 × 90 × 64  ·  92.2 万体素", options[4].label)
        assertEquals("极高  ·  192 × 128 × 96  ·  235.9 万体素", options[5].label)
        // Below 10k voxels the exact count is more useful than a rounded 万 value.
        assertEquals("1 体素", FroxelOption.formatVoxels(1))
        assertEquals("9999 体素", FroxelOption.formatVoxels(9999))
        assertEquals("1.0 万体素", FroxelOption.formatVoxels(10_000))
        assertEquals("100.0 万体素", FroxelOption.formatVoxels(1_000_000))
    }

    @Test fun ceilDivMatchesIntegerArithmetic() {
        assertEquals(1, FroxelPolicy.ceilDiv(1, 8))
        assertEquals(1, FroxelPolicy.ceilDiv(8, 8))
        assertEquals(2, FroxelPolicy.ceilDiv(9, 8))
        assertEquals(0, FroxelPolicy.ceilDiv(0, 8))
        assertThrows(IllegalArgumentException::class.java) { FroxelPolicy.ceilDiv(8, 0) }
    }
}
