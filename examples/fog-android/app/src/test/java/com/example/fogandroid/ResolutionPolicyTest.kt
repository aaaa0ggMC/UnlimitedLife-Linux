package com.example.fogandroid

import org.junit.Assert.*
import org.junit.Test

class ResolutionPolicyTest {
    @Test fun nativeAnd720pPreserveUltrawideAspect() {
        assertEquals(RenderResolution(2400, 1080), ResolutionPolicy.sizeFor(2400, 1080, 0))
        assertEquals(RenderResolution(1600, 720), ResolutionPolicy.sizeFor(2400, 1080, 720))
        assertEquals(RenderResolution(1200, 540), ResolutionPolicy.sizeFor(2400, 1080, 540))
        assertEquals(RenderResolution(800, 360), ResolutionPolicy.sizeFor(2400, 1080, 360))
    }

    @Test fun handlesPortraitAndOddSizedWindowsWithoutStretching() {
        assertEquals(RenderResolution(720, 1600), ResolutionPolicy.sizeFor(1080, 2400, 720))
        assertEquals(RenderResolution(1170, 540), ResolutionPolicy.sizeFor(2340, 1080, 540))
        assertEquals(RenderResolution(1280, 720), ResolutionPolicy.sizeFor(3840, 2160, 720))
    }

    @Test fun neverUpscalesAndRemovesDuplicateModes() {
        assertEquals(RenderResolution(800, 480), ResolutionPolicy.sizeFor(800, 480, 1080))
        assertEquals(listOf(0, 360), ResolutionPolicy.optionsFor(800, 480).map { it.shortEdge })
        assertEquals(listOf(0, 720, 540, 360), ResolutionPolicy.optionsFor(2400, 1080).map { it.shortEdge })
    }

    @Test fun invalidSizesAreRejectedAndTinyWindowsRemainPositive() {
        assertThrows(IllegalArgumentException::class.java) { ResolutionPolicy.sizeFor(0, 1080, 720) }
        assertThrows(IllegalArgumentException::class.java) { ResolutionPolicy.sizeFor(1920, -1, 720) }
        assertThrows(IllegalArgumentException::class.java) { ResolutionPolicy.sizeFor(1920, 1080, -1) }
        assertEquals(RenderResolution(1, 1), ResolutionPolicy.sizeFor(1, 1, 360))
    }

    @Test fun waitsForTheSelectedBufferInsteadOfStartingAtNativeSize() {
        val selected = RenderResolution(1600, 720)
        assertFalse(ResolutionPolicy.matchesSurface(null, 2400, 1080))
        assertFalse(ResolutionPolicy.matchesSurface(selected, 2400, 1080))
        assertFalse(ResolutionPolicy.matchesSurface(selected, 0, 0))
        assertTrue(ResolutionPolicy.matchesSurface(selected, 1600, 720))
        assertFalse(ResolutionPolicy.matchesSurface(RenderResolution(800, 360), 1600, 720))
    }

    @Test fun touchCoordinatesFollowTheRenderBufferNotTheFullSizeView() {
        val (x, y) = ResolutionPolicy.touchPosition(1800f, 810f, 2400, 1080, RenderResolution(1600, 720))
        assertEquals(1200f, x, 0.001f)
        assertEquals(540f, y, 0.001f)
        val native = ResolutionPolicy.touchPosition(123f, 45f, 2400, 1080, RenderResolution(2400, 1080))
        assertEquals(123f, native.first, 0.001f)
        assertEquals(45f, native.second, 0.001f)
    }

    @Test fun outOfBoundsTouchIsNotClampedIntoAnUnintendedButton() {
        val (x, y) = ResolutionPolicy.touchPosition(-60f, 1110f, 2400, 1080, RenderResolution(1600, 720))
        assertEquals(-40f, x, 0.001f)
        assertEquals(740f, y, 0.001f)
        assertThrows(IllegalArgumentException::class.java) {
            ResolutionPolicy.touchPosition(0f, 0f, 0, 1080, RenderResolution(1600, 720))
        }
    }

    @Test fun guiScaleMaintainsPhysicalSizeAcrossResolutionChoices() {
        assertEquals(1f, ResolutionPolicy.renderScale(2400, 1080, RenderResolution(2400, 1080)), 0.001f)
        assertEquals(2f / 3f, ResolutionPolicy.renderScale(2400, 1080, RenderResolution(1600, 720)), 0.001f)
        assertEquals(1f / 3f, ResolutionPolicy.renderScale(2400, 1080, RenderResolution(800, 360)), 0.001f)
    }

    @Test fun viewResizeReconfiguresUiEvenWhenTheSelectedBufferIsUnchanged() {
        val old = ResolutionPolicy.configurationFor(2400, 1080, 720)
        val resized = ResolutionPolicy.configurationFor(1600, 720, 720)
        assertEquals(old.buffer, resized.buffer)
        assertNotEquals(old, resized)
        assertEquals(old, ResolutionPolicy.configurationFor(2400, 1080, 720))
        assertEquals(2f / 3f, ResolutionPolicy.renderScale(old.view.width, old.view.height, old.buffer), 0.001f)
        assertEquals(1f, ResolutionPolicy.renderScale(resized.view.width, resized.view.height, resized.buffer), 0.001f)
    }
}
