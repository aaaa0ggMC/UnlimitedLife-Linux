package com.example.fogandroid

import kotlin.math.min
import kotlin.math.roundToInt

data class RenderResolution(val width: Int, val height: Int)
data class RenderConfiguration(val view: RenderResolution, val buffer: RenderResolution)

data class ResolutionOption(val shortEdge: Int, val size: RenderResolution) {
    val label: String
        get() = "${if (shortEdge == 0) "原生" else "${shortEdge}p"}  ·  ${size.width} × ${size.height}"
}

/** Pure sizing/coordinate policy: 0 means native; never upscale or force a 16:9 ratio. */
object ResolutionPolicy {
    val shortEdges = listOf(0, 1080, 720, 540, 360)

    fun sizeFor(width: Int, height: Int, shortEdge: Int): RenderResolution {
        require(width > 0 && height > 0 && shortEdge >= 0)
        val scale = if (shortEdge == 0) 1.0 else min(1.0, shortEdge.toDouble() / min(width, height))
        return RenderResolution(
            (width * scale).roundToInt().coerceIn(1, width),
            (height * scale).roundToInt().coerceIn(1, height)
        )
    }

    fun optionsFor(width: Int, height: Int): List<ResolutionOption> = shortEdges
        .map { ResolutionOption(it, sizeFor(width, height, it)) }
        .distinctBy { it.size }

    fun configurationFor(width: Int, height: Int, shortEdge: Int): RenderConfiguration =
        RenderConfiguration(RenderResolution(width, height), sizeFor(width, height, shortEdge))

    fun matchesSurface(expected: RenderResolution?, width: Int, height: Int): Boolean =
        expected != null && width > 0 && height > 0 &&
            expected.width == width && expected.height == height

    fun touchPosition(x: Float, y: Float, viewWidth: Int, viewHeight: Int,
                      buffer: RenderResolution): Pair<Float, Float> {
        require(viewWidth > 0 && viewHeight > 0 && buffer.width > 0 && buffer.height > 0)
        return Pair(x * buffer.width / viewWidth, y * buffer.height / viewHeight)
    }

    fun renderScale(viewWidth: Int, viewHeight: Int, buffer: RenderResolution): Float {
        require(viewWidth > 0 && viewHeight > 0 && buffer.width > 0 && buffer.height > 0)
        return min(buffer.width.toFloat() / viewWidth, buffer.height.toFloat() / viewHeight)
    }
}
