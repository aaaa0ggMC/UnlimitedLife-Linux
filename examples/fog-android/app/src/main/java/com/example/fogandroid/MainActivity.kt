package com.example.fogandroid

import android.app.Activity
import android.content.res.AssetManager
import android.content.pm.ActivityInfo
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.Button
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.ScrollView
import android.widget.TextView
import java.io.File
import java.io.IOException
import java.util.concurrent.Executors

class MainActivity : Activity(), SurfaceHolder.Callback {
    private external fun nativeStart(
        surface: Surface, assetRoot: String, renderScale: Float,
        froxelPixelSize: Int, froxelDepthSlices: Int, froxelLightSteps: Int,
        diagnosticFlags: Int
    ): String
    private external fun nativeStop()
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeTouch(action: Int, x: Float, y: Float)
    private external fun nativeError(): String

    private val checkRenderError = object : Runnable {
        override fun run() {
            if (!running) return
            val error = nativeError()
            if (error.isNotEmpty()) {
                stopRendering()
                startFailed = true
                showStatus("Fog renderer failed: $error\nPause and resume the app to retry.")
            } else {
                surfaceView?.postDelayed(this, 500)
            }
        }
    }

    private lateinit var root: FrameLayout
    private var surfaceView: SurfaceView? = null
    private lateinit var status: TextView
    private lateinit var resolutionChoices: RadioGroup
    private lateinit var resolutionSummary: TextView
    private lateinit var enterButton: Button
    private lateinit var froxelChoices: RadioGroup
    private lateinit var froxelSummary: TextView
    private var displayedOptions = emptyList<ResolutionOption>()
    private var selectedShortEdge = 720
    private var selectedFroxel = FroxelPolicy.DEFAULT_INDEX
    private var diagnosticFlags = 0
    private var sceneEntered = false
    private var configuredRender: RenderConfiguration? = null
    private var resumed = false
    private var destroyed = false
    private var libraryReady = false
    private var assetsReady = false
    private var copyingAssets = false
    private var running = false
    private var startFailed = false
    private var surfaceWidth = 0
    private var surfaceHeight = 0
    private var pointerId = MotionEvent.INVALID_POINTER_ID

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Request the host orientation before creating the SurfaceView/native window.
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        hideSystemBars()
        selectedShortEdge = savedInstanceState?.getInt("shortEdge", 720)
            ?.takeIf { it in ResolutionPolicy.shortEdges } ?: 720
        selectedFroxel = savedInstanceState?.getInt("froxelLevel", FroxelPolicy.DEFAULT_INDEX)
            ?.takeIf { it in FroxelPolicy.levels.indices } ?: FroxelPolicy.DEFAULT_INDEX
        // Explicit opt-in only. Normal launcher starts retain the settings screen.
        val autoStart = intent.getBooleanExtra("fog_auto_start", false)
        if (savedInstanceState == null && autoStart) {
            selectedShortEdge = intent.getIntExtra("fog_short_edge", 720)
                .takeIf { it in ResolutionPolicy.shortEdges } ?: 720
            selectedFroxel = intent.getIntExtra("fog_froxel", FroxelPolicy.DEFAULT_INDEX)
                .takeIf { it in FroxelPolicy.levels.indices } ?: FroxelPolicy.DEFAULT_INDEX
        }
        diagnosticFlags = savedInstanceState?.getInt("diagnosticFlags") ?: if (autoStart) {
            1 or
                (if (intent.getBooleanExtra("fog_no_volume", false)) 2 else 0) or
                (if (intent.getBooleanExtra("fog_no_lights", false)) 4 else 0) or
                (if (intent.getBooleanExtra("fog_no_shadows", false)) 8 else 0) or
                (if (intent.getBooleanExtra("fog_no_dragon", false)) 16 else 0) or
                (if (intent.getBooleanExtra("fog_no_ui", false)) 32 else 0) or
                (if (intent.getBooleanExtra("fog_local_mist", false)) 64 else 0)
        } else 0
        status = TextView(this).apply {
            textSize = 16f
            setTextColor(Color.WHITE)
            setBackgroundColor(0xCC000000.toInt())
            setPadding(32, 24, 32, 24)
            isClickable = false
            isFocusable = false
        }
        root = FrameLayout(this).apply { setBackgroundColor(Color.rgb(24, 28, 34)) }
        setContentView(root)
        if (autoStart || savedInstanceState?.getBoolean("sceneEntered") == true) enterScene()
        else showResolutionPicker()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putInt("shortEdge", selectedShortEdge)
        outState.putInt("froxelLevel", selectedFroxel)
        outState.putInt("diagnosticFlags", diagnosticFlags)
        outState.putBoolean("sceneEntered", sceneEntered)
        super.onSaveInstanceState(outState)
    }

    private fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()

    private fun showResolutionPicker() {
        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(24), dp(16), dp(24), dp(16))
        }
        fun label(text: String, size: Float) = TextView(this).apply {
            this.text = text
            textSize = size
            setTextColor(Color.WHITE)
            setPadding(0, dp(4), 0, dp(8))
        }
        content.addView(label("Fog Android · 启动设置", 24f))
        content.addView(label("渲染分辨率", 19f))
        resolutionSummary = label("正在获取窗口尺寸…", 14f)
        content.addView(resolutionSummary)
        resolutionChoices = RadioGroup(this)
        content.addView(resolutionChoices)
        content.addView(label("保持屏幕比例并全屏显示；低分辨率更省算力。仅调整分辨率，不改变雾或阴影画质。", 14f))
        enterButton = Button(this).apply {
            text = "进入场景"
            isEnabled = false
            setOnClickListener { enterScene() }
        }
        content.addView(enterButton)
        content.addView(label("体积雾精度（Froxel 网格）", 19f))
        content.addView(label(
            "像素步长与深度层数决定网格密度，光源步数决定逐体素光照开销；" +
                "共六档，成本大致按 3 倍递进。网格按上面选定的渲染分辨率计算，" +
                "横纵向上限 192×128。标“关闭光衰减”的档位会让灯光穿透雾，属可见降质。", 14f))
        froxelSummary = label("正在获取窗口尺寸…", 14f)
        content.addView(froxelSummary)
        froxelChoices = RadioGroup(this)
        content.addView(froxelChoices)
        root.addView(ScrollView(this).apply {
            isFillViewport = true
            addView(content)
        }, FrameLayout.LayoutParams(-1, -1))
        root.addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ ->
            if (!sceneEntered) refreshResolutionChoices()
        }
        root.post { if (!sceneEntered) refreshResolutionChoices() }
    }

    private fun refreshResolutionChoices() {
        if (root.width <= 0 || root.height <= 0) return
        val options = ResolutionPolicy.optionsFor(root.width, root.height)
        if (options == displayedOptions) return
        displayedOptions = options
        // On a smaller screen, duplicate/upscaled modes disappear; native is the fallback.
        if (options.none { it.shortEdge == selectedShortEdge }) selectedShortEdge = 0
        resolutionChoices.setOnCheckedChangeListener(null)
        resolutionChoices.removeAllViews()
        for (option in options) {
            val button = RadioButton(this).apply {
                id = View.generateViewId()
                tag = option.shortEdge
                text = option.label
                textSize = 16f
                setTextColor(Color.WHITE)
                minHeight = dp(48)
            }
            resolutionChoices.addView(button)
            if (option.shortEdge == selectedShortEdge) resolutionChoices.check(button.id)
        }
        resolutionChoices.setOnCheckedChangeListener { group, id ->
            (group.findViewById<RadioButton>(id)?.tag as? Int)?.let { selectedShortEdge = it }
            // Froxel grids follow the selected render buffer, so relabel them too.
            refreshFroxelChoices()
        }
        resolutionSummary.text = "窗口 ${root.width} × ${root.height} · 按短边选择，保持屏幕比例"
        enterButton.isEnabled = true
        refreshFroxelChoices()
    }

    /// Froxel grids depend on the selected render buffer; skip until the picker exists.
    private fun refreshFroxelChoices() {
        if (!::froxelChoices.isInitialized) return
        if (root.width <= 0 || root.height <= 0) return
        val buffer = ResolutionPolicy.sizeFor(root.width, root.height, selectedShortEdge)
        froxelChoices.setOnCheckedChangeListener(null)
        froxelChoices.removeAllViews()
        for (option in FroxelPolicy.optionsFor(buffer.width, buffer.height)) {
            val button = RadioButton(this).apply {
                id = View.generateViewId()
                tag = option.level.index
                text = option.label
                textSize = 16f
                setTextColor(Color.WHITE)
                minHeight = dp(40)
            }
            froxelChoices.addView(button)
            if (option.level.index == selectedFroxel) froxelChoices.check(button.id)
        }
        froxelChoices.setOnCheckedChangeListener { group, id ->
            (group.findViewById<RadioButton>(id)?.tag as? Int)?.let { selectedFroxel = it }
            updateFroxelSummary(buffer)
        }
        updateFroxelSummary(buffer)
    }

    private fun updateFroxelSummary(buffer: RenderResolution) {
        if (!::froxelSummary.isInitialized) return
        val level = FroxelPolicy.levelFor(selectedFroxel)
        val grid = FroxelPolicy.gridFor(buffer.width, buffer.height, level)
        froxelSummary.text = "缓冲 ${buffer.width} × ${buffer.height} · 当前「${level.name}」：" +
            "${grid.label}（像素步长 ${level.pixelSize}，深度 ${grid.depth} 层，" +
            "光源 ${level.lightSteps} 步）" +
            if (level.disablesMediumAttenuation) " · 灯光会穿透雾" else ""
    }

    private fun enterScene() {
        if (sceneEntered) return
        sceneEntered = true
        root.removeAllViews()
        val view = SurfaceView(this).apply {
            holder.addCallback(this@MainActivity)
            setOnTouchListener { _, event -> handleTouch(event) }
            addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ -> applyResolution() }
        }
        surfaceView = view
        root.addView(view, FrameLayout.LayoutParams(-1, -1))
        root.addView(status, FrameLayout.LayoutParams(-1, -2, Gravity.TOP))
        showStatus("正在准备场景…")
        try {
            System.loadLibrary("fogandroid")
            libraryReady = true
        } catch (error: LinkageError) {
            showStatus("Native library unavailable: ${error.message}")
        }
        if (libraryReady) prepareAssets()
    }

    private fun applyResolution() {
        val view = surfaceView ?: return
        if (view.width <= 0 || view.height <= 0 || destroyed) return
        val configuration = ResolutionPolicy.configurationFor(view.width, view.height, selectedShortEdge)
        // An unchanged 720p buffer can still need a different UI scale after a
        // proportional View resize (for example 2400x1080 -> 1600x720).
        if (configuration == configuredRender) return
        stopRendering()
        configuredRender = configuration
        val size = configuration.buffer
        showStatus("渲染分辨率 ${size.width} × ${size.height}，正在准备…")
        // UI-thread call: lower the actual native/Vulkan buffer, not the View's layout size.
        view.holder.setFixedSize(size.width, size.height)
        maybeStart()
    }

    @Suppress("DEPRECATION")
    private fun hideSystemBars() {
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_FULLSCREEN or
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    override fun onResume() {
        super.onResume()
        resumed = true
        hideSystemBars()
        if (libraryReady && !assetsReady && !copyingAssets) prepareAssets()
        maybeStart()
    }

    override fun onPause() {
        resumed = false
        stopRendering()
        startFailed = false
        super.onPause()
    }

    override fun onDestroy() {
        destroyed = true
        stopRendering()
        super.onDestroy()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        if (holder === surfaceView?.holder) applyResolution()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        if (holder !== surfaceView?.holder) return
        surfaceWidth = width
        surfaceHeight = height
        if (running) {
            if (ResolutionPolicy.matchesSurface(configuredRender?.buffer, width, height)) nativeResize(width, height)
            else stopRendering()
        }
        maybeStart()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        if (holder !== surfaceView?.holder) return
        surfaceWidth = 0
        surfaceHeight = 0
        // JNI must join its render thread here, before Android releases the Surface.
        stopRendering()
        configuredRender = null
        startFailed = false
    }

    private fun maybeStart() {
        val view = surfaceView ?: return
        if (!sceneEntered || destroyed || !resumed || !libraryReady || !assetsReady || running || startFailed ||
            configuredRender?.view != RenderResolution(view.width, view.height) ||
            !ResolutionPolicy.matchesSurface(configuredRender?.buffer, surfaceWidth, surfaceHeight) ||
            view.width <= 0 || view.height <= 0 || !view.holder.surface.isValid) return

        // Publish the actual SurfaceView extent BEFORE the native thread builds a swapchain.
        nativeResize(surfaceWidth, surfaceHeight)
        val scale = ResolutionPolicy.renderScale(view.width, view.height, RenderResolution(surfaceWidth, surfaceHeight))
        val froxel = FroxelPolicy.levelFor(selectedFroxel)
        val error = nativeStart(view.holder.surface, File(filesDir, ASSET_ROOT).absolutePath, scale,
            froxel.pixelSize, froxel.depthSlices, froxel.lightSteps, diagnosticFlags)
        if (error.isNotEmpty()) {
            // Also clean up any partially initialized native state before a later retry.
            nativeStop()
            startFailed = true
            showStatus("Fog startup failed: $error\nPause and resume the app to retry.")
            return
        }
        running = true
        view.postDelayed(checkRenderError, 500)
        nativeResize(surfaceWidth, surfaceHeight)
        status.visibility = View.GONE
    }

    private fun stopRendering() {
        surfaceView?.removeCallbacks(checkRenderError)
        pointerId = MotionEvent.INVALID_POINTER_ID
        if (!running) return
        nativeTouch(MotionEvent.ACTION_CANCEL, 0f, 0f)
        nativeStop()
        running = false
    }

    private fun handleTouch(event: MotionEvent): Boolean {
        if (!running) return true
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                pointerId = event.getPointerId(0)
                sendTouch(MotionEvent.ACTION_DOWN, event.getX(0), event.getY(0))
            }
            MotionEvent.ACTION_MOVE -> {
                val index = event.findPointerIndex(pointerId)
                if (index >= 0) sendTouch(MotionEvent.ACTION_MOVE, event.getX(index), event.getY(index))
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val index = event.actionIndex
                if (event.getPointerId(index) == pointerId) {
                    sendTouch(MotionEvent.ACTION_UP, event.getX(index), event.getY(index))
                    pointerId = MotionEvent.INVALID_POINTER_ID
                }
            }
            MotionEvent.ACTION_CANCEL -> {
                nativeTouch(MotionEvent.ACTION_CANCEL, 0f, 0f)
                pointerId = MotionEvent.INVALID_POINTER_ID
            }
        }
        return true
    }

    private fun sendTouch(action: Int, x: Float, y: Float) {
        val view = surfaceView ?: return
        if (view.width <= 0 || view.height <= 0 || surfaceWidth <= 0 || surfaceHeight <= 0) return
        val (bufferX, bufferY) = ResolutionPolicy.touchPosition(
            x, y, view.width, view.height, RenderResolution(surfaceWidth, surfaceHeight))
        nativeTouch(action, bufferX, bufferY)
    }

    private fun showStatus(message: String) {
        status.text = message
        status.visibility = View.VISIBLE
    }

    private fun prepareAssets() {
        copyingAssets = true
        showStatus("Preparing fog assets…")
        val assetManager = applicationContext.assets
        val destination = File(filesDir, ASSET_ROOT)
        // Serialize copies across Activity recreation, without blocking the UI thread.
        assetExecutor.execute {
            val error = try {
                val entries = assetManager.list(ASSET_ROOT)
                if (entries.isNullOrEmpty()) throw IOException("Missing packaged assets/$ASSET_ROOT")
                copyAssetTree(assetManager, ASSET_ROOT, destination)
                null
            } catch (error: Exception) {
                error.message ?: error.javaClass.simpleName
            }
            runOnUiThread {
                if (!destroyed) {
                    copyingAssets = false
                    assetsReady = error == null
                    if (error == null) {
                        showStatus("Waiting for rendering surface…")
                        maybeStart()
                    } else {
                        showStatus("Asset preparation failed: $error\nPause and resume the app to retry.")
                    }
                }
            }
        }
    }

    companion object {
        private const val ASSET_ROOT = "fog-runtime"
        private val assetExecutor = Executors.newSingleThreadExecutor()

        private fun copyAssetTree(assets: AssetManager, source: String, destination: File) {
            val children = assets.list(source) ?: throw IOException("Cannot list $source")
            if (children.isNotEmpty()) {
                if (!destination.isDirectory && !destination.mkdirs()) {
                    throw IOException("Cannot create ${destination.name}")
                }
                for (child in children) copyAssetTree(assets, "$source/$child", File(destination, child))
            } else {
                assets.open(source).use { input ->
                    destination.outputStream().use { output -> input.copyTo(output) }
                }
            }
        }
    }
}
