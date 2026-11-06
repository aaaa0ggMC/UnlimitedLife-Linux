/**
 * @file main.cpp
 * @brief fog_gl —— 用 AGE（OpenGL）复刻 examples/fog（AVE/Vulkan）的线性雾场景，
 *        用于对比 "Vulkan(AVE) vs OpenGL(AGE)" 的帧率。
 *
 * 规格来源（严格遵循，不得偏离）：
 *   - e1/bench_spec.md
 *   - examples/fog/app_setup.cpp:setup_world（场景逐字复刻）
 *
 * 实现要点：
 *   - AGE 抽象：Application / Window / Shader / ShaderUniform / Texture / Sampler /
 *     Model(age::model::Prefab) / Context(GL 状态) / world::Camera / world::Object。
 *   - AGE 缺失或不适用的能力，在示例内用原生 GL 补齐（不修改 modules/AGE、include/AGE）：
 *       1. `layout(std140) uniform CameraBlock` 的 UBO（AGE 无 UBO 封装）；
 *       2. float 清屏色（AGE 的 Window::clear 只接受 0~255 的 GLuint）；
 *       3. OpenGL core profile 提示（AGE 的 GLVersion 只暴露版本号，不含 profile）。
 *   - FPSDetective 为示例内自带头文件 fps_detective.hpp（由
 *     include/AVE/misc/fps_detective.cppm 移植，不 import ave）。
 *
 * 基准模式（无任何输入依赖，时间驱动）：
 *   预热 FOG_WARMUP_SECONDS（默认 2s，不计入统计）
 *   + 测量 FOG_BENCH_SECONDS（默认 60s）后自动退出并打印 FPSDetective 报告。
 */

#include <AGE/Application.h>
#include <AGE/Context.h>
#include <AGE/Model.h>
#include <AGE/ModelLoader/PrefabGenerator.h>
#include <AGE/Shader.h>
#include <AGE/Texture.h>
#include <AGE/Window.h>
#include <AGE/World/Camera.h>
#include <AGE/World/Components.h>
#include <AGE/World/Object.h>

#include <GL/glext.h>
#include <GLFW/glfw3.h>

#include <alib5/aecs.h>
#include <alib5/aclock.h>
#include <alib5/alogger.h>
#include <alib5/autil.h>

#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdlib>
#include <exception>
#include <memory>
#include <vector>

#include "fps_detective.hpp"

//// using ///////////////////////////////////////////////////////
using namespace alib5;
using namespace alib5::ecs;
using namespace age;
using namespace age::world;
using enum alib5::Severity;

//// 规格常量 /////////////////////////////////////////////////////
namespace fog_gl {

    namespace cfg {
        constexpr const char* kTitle = "Global Linear Fog (AGE / OpenGL) by aaaa0ggmc";
        constexpr int kWidth = 1024;   ///< spec：窗口 1024x768，不全屏，不改分辨率
        constexpr int kHeight = 768;

        constexpr int kCubeCount = 12;              ///< spec：12 个立方体
        constexpr float kCubeEdge = 1.0f;           ///< spec：ave::Prefab::cube(1.0f)
        constexpr float kGroundSize = 60.0f;        ///< spec：ave::Prefab::plane(60,60,15)
        constexpr float kGroundUvRepeat = 15.0f;    ///< spec：uv 重复 15
        constexpr float kCubeSpacingZ = -2.6f;      ///< spec：z = -2.6*i
        constexpr float kCubeLateralStep = 1.6f;    ///< spec：x = (i%3-1)*1.6
        constexpr float kCubeBaseHeight = 0.1f;     ///< spec：y = 0.1 + 0.35*sin(i*0.9)
        constexpr float kCubeBobAmplitude = 0.35f;
        constexpr float kCubeBobPhase = 0.9f;
        constexpr glm::vec3 kCubeRotationAxis{0.25f, 1.0f, 0.15f}; ///< spec：normalize(0.25,1,0.15)
        constexpr float kCubeRotationStepDeg = 12.0f;              ///< spec：旋转 12*i 度

        constexpr float kCameraFovDeg = 45.0f;     ///< spec：fov 45°
        constexpr float kCameraNear = 0.1f;        ///< spec：near 0.1
        constexpr float kCameraFar = 300.0f;       ///< spec：far 300
        constexpr glm::vec3 kCameraPosition{0.0f, 1.6f, 10.0f}; ///< spec：位置 (0,1.6,10)
        constexpr float kCameraPitchDeg = -12.0f;                ///< spec：pitch = -12°
        constexpr float kYawAmplitudeDeg = 30.0f;                ///< spec：yaw(t)=30°*sin(2πt/10)
        constexpr float kYawPeriodSec = 10.0f;

        constexpr glm::vec3 kGroundPosition{0.0f, -1.0f, 0.0f};  ///< spec：地面位于 (0,-1,0)
        constexpr glm::vec3 kFogColor{0.55f, 0.68f, 0.78f};      ///< spec：雾色 (0.55,0.68,0.78)
        constexpr float kFogStart = 5.0f;                        ///< spec：线性雾 start=5
        constexpr float kFogEnd = 25.0f;                         ///< spec：线性雾 end=25

        constexpr const char* kShaderVert = "fog_gl/shaders/fog.vert";
        constexpr const char* kShaderFrag = "fog_gl/shaders/fog.frag";
        constexpr const char* kCubeTexturePath = "test_data/imgs/ice.png";
        constexpr const char* kGroundTexturePath = "test_data/imgs/wall.jpg";
    } // namespace cfg

    /// UBO binding point（与着色器里 glUniformBlockBinding 的选择一致）
    constexpr GLuint kUboBindingPoint = 0;

    /**
     * @brief 场景物体渲染组件。
     * @note AGE 没有内置渲染系统，这里沿用 examples/agetest 的 ObjectRender 模式：
     *       Model + Texture + 依赖注入 Transform。
     */
    struct ObjectRender {
        using Dependency = ComponentStack<comps::Transform>;

        Model* model { nullptr };
        Texture* texture { nullptr };
        bool visible { true };

        ref_t<comps::Transform> transform;

        void bind_dep(const Dependency::deptup_t& tup) {
            auto& [trs] = tup;
            transform = trs;
        }
    };

    /// 从环境变量读取秒数（用于缩短基准测试时长），非法/缺省时返回 fallback
    double env_seconds(const char* name, double fallback) {
        if (const char* raw = std::getenv(name); raw && *raw) {
            try {
                const double value = std::stod(raw);
                if (value > 0.0) return value;
            } catch (const std::exception&) {
            }
        }
        return fallback;
    }

    /**
     * @brief 生成与 ave::Prefab::plane(60,60,15) 等价的地面几何。
     * @note AGE 的 Prefab 只有 box/cube/sphere/torus，没有 plane；
     *       这里按 examples/fog 使用的 AVE Prefab::plane 逐顶点复刻：
     *       XZ 平面、法线 +Y、y=0、中心对齐、uv 重复 15、索引 {0,1,2,0,2,3}。
     */
    void build_ground_plane(Model& model) {
        auto data = model.add();

        ModelData& md = *data.first;
        Model::GraphRes& gr = *data.second;

        const float x = cfg::kGroundSize * 0.5f;
        const float z = cfg::kGroundSize * 0.5f;
        const float uv = cfg::kGroundUvRepeat;

        md.vertices = {-x, 0.0f, -z,  x, 0.0f, -z,  x, 0.0f, z,  -x, 0.0f, z};
        md.normals = {0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,
                      0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f};
        md.coords = {0.0f, 0.0f,  uv, 0.0f,  uv, uv,  0.0f, uv};
        md.indices = {0, 1, 2,  0, 2, 3};

        // 与 age::model::Prefab 的默认绑定一致：location 0=pos, 1=uv, 2=normal
        md.bind(gr.vao, gr.vbuffer, gr.ibuffer, gr.vtbuffer, gr.vnbuffer, 0, 1, 2);
    }

    /**
     * @brief fog_gl 应用：窗口 + AGE 资源 + ECS 场景 + 基准主循环
     */
    struct FogGLApp {
        //// 日志（先声明：LogFactory 依赖 Logger）////
        Logger logger;
        LogFactory lg;

        //// ECS ////
        EntityManager em;

        //// AGE ////
        Application app;    ///< 必须晚于 em 构造（持有 EntityManager&）
        Context context;    ///< OpenGL 状态封装
        Window* window { nullptr };

        //// 场景 ////
        world::Camera camera { em };
        std::vector<std::unique_ptr<Object>> objects;

        //// 图形资源 ////
        std::shared_ptr<Model> cube_model { std::make_shared<Model>() };
        std::shared_ptr<Model> ground_model { std::make_shared<Model>() };
        Shader shader { Shader::null() };
        ShaderUniform u_model;
        GLuint ubo { 0 };
        Sampler sampler;
        Texture* cube_texture { nullptr };
        Texture* ground_texture { nullptr };

        FogGLApp()
        : lg(logger, "FogGL")
        , app(em) {
            logger.append_mod<lot::Console>("console");
            camera.add<comps::Tag>("camera");
        }

        ~FogGLApp() {
            if (ubo) glDeleteBuffers(1, &ubo);
        }

        //// 环境变量：预热 / 测量时长 ////
        double warmup_seconds() const { return env_seconds("FOG_WARMUP_SECONDS", 2.0); }
        double measure_seconds() const { return env_seconds("FOG_BENCH_SECONDS", 60.0); }

        void setup() {
            setup_window();
            setup_gl_state();
            setup_shader();
            setup_ubo();
            setup_sampler();
            setup_textures();
            setup_models();
            setup_world();
        }

        void setup_window() {
            // AGE 的 setGLVersion 只设置 GLFW 的 context version hint，
            // 不含 profile；着色器为 #version 460 core，这里补一个原生 hint 请求 core profile。
            app.setGLVersion(4, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            CreateWindowInfo ci;
            ci.sid = "FogGL";
            ci.windowTitle = cfg::kTitle;
            ci.width = cfg::kWidth;
            ci.height = cfg::kHeight;
            ci.fps = 0;               ///< 不限帧（垂直同步关闭，性能可自由发挥）
            ci.style = WinStylePresetNormal;

            auto created = app.windows.create(ci);
            if (!created) {
                lg(Fatal) << "Failed to create GLFW window!" << endlog;
                std::exit(-1);
            }
            window = *created;
            window->makeCurrent();

            // spec：OpenGL 侧关闭垂直同步
            Window::setSwapInterval(0);

            // 打印 GL_RENDERER 以证明跑在独显（RTX 5060 Laptop GPU）上
            lg(Info) << "GL_VENDOR   : " << reinterpret_cast<const char*>(glGetString(GL_VENDOR)) << endlog;
            lg(Info) << "GL_RENDERER: " << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << endlog;
            lg(Info) << "GL_VERSION : " << reinterpret_cast<const char*>(glGetString(GL_VERSION)) << endlog;
            lg(Info) << "GLSL       : " << reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)) << endlog;
            lg(Info) << "Window     : " << cfg::kWidth << "x" << cfg::kHeight
                     << " (framebuffer " << window->getFrameBufferSize().x << "x"
                     << window->getFrameBufferSize().y << ")" << endlog;

            context.sync();
        }

        void setup_gl_state() {
            // spec：深度测试 LESS + 写深度，不裁剪背面（默认已关），不混合（保持关闭）
            context.depth_test(true).depth_func(GL_LESS).depth_mask(GL_TRUE);
            context.cull_face_test(false);
            // AVE 侧交换链是 B8G8R8A8_SRGB（写入时硬件做 sRGB 编码）、纹理是 UNORM（不解码），
            // 这里打开 GL_FRAMEBUFFER_SRGB 使输出编码一致（画面等价，且与性能无关）。
            glEnable(GL_FRAMEBUFFER_SRGB);
            app.checkOpenGLError();
            lg(Info) << "GL state: depth_test=LESS+write, cull=off, blend=off" << endlog;
        }

        void setup_shader() {
            shader = app.shaders.fromFile("fog_gl", cfg::kShaderVert, cfg::kShaderFrag);
            if (!app.shaders.has("fog_gl") || shader.get_sid().empty()) {
                lg(Fatal) << "Failed to compile/link fog_gl shader (" << cfg::kShaderVert
                          << " / " << cfg::kShaderFrag << ")!" << endlog;
                std::exit(-1);
            }
            u_model = shader["model"];
            lg(Info) << "Shader compiled: " << cfg::kShaderVert << " + " << cfg::kShaderFrag << endlog;
        }

        void setup_ubo() {
            // 与 examples/fog 的 CameraFogUbo(std430) 布局一致：view/proj/fog_color/fog_params
            glCreateBuffers(1, &ubo);
            glNamedBufferStorage(ubo, sizeof(CameraFogUbo), nullptr, GL_DYNAMIC_STORAGE_BIT);

            // Shader 不暴露 program id，通过 uniform 句柄取（uniform 是否优化掉都会带上 program）
            const GLuint program = shader["model"].getProgramId();
            const GLuint block_index = glGetUniformBlockIndex(program, "CameraBlock");
            if (block_index == GL_INVALID_INDEX) {
                lg(Fatal) << "Uniform block CameraBlock not found in fog_gl shader!" << endlog;
                std::exit(-1);
            }
            glUniformBlockBinding(program, block_index, kUboBindingPoint);
            glBindBufferBase(GL_UNIFORM_BUFFER, kUboBindingPoint, ubo);

            lg(Info) << "Created CameraBlock UBO (binding " << kUboBindingPoint << ")" << endlog;
        }

        void setup_sampler() {
            auto created = app.samplers.create("fog_gl");
            if (!created) {
                lg(Fatal) << "Failed to create sampler!" << endlog;
                std::exit(-1);
            }
            sampler = *created;
            // 注意：AGE 的 SamplerInfo 默认并不初始化 minFilter/magFilter，
            // 而 sampler 对象的过滤参数会覆盖纹理自带的 LINEAR；这里显式指定线性过滤。
            // spec：线性过滤、不生成 mipmap（等价于只采样 mip 0）。
            sampler.minFilter(SamplerInfo::MinFilter::Linear)
                .magFilter(SamplerInfo::MagFilter::Linear)
                .wrapS(SamplerInfo::WrapMethod::Repeat)
                .wrapT(SamplerInfo::WrapMethod::Repeat);
            sampler.bind(GL_TEXTURE0);
            lg(Info) << "Sampler: LINEAR/LINEAR, REPEAT (no mipmap)" << endlog;
        }

        void setup_textures() {
            CreateTextureInfo ci;
            ci.source = CreateTextureInfo::FromFile;
            ci.internalFormat = GL_RGBA;
            ci.genMipmap = false;   ///< spec：不生成 mipmap
            ci.uploadToOpenGL = true;

            ci.sid = "ice";
            ci.file.path = cfg::kCubeTexturePath;
            if (auto texture = app.textures.create(ci)) {
                cube_texture = *texture;
                lg(Info) << "Loaded " << cfg::kCubeTexturePath << " (cubes) "
                         << cube_texture->getTextureInfo().width << "x"
                         << cube_texture->getTextureInfo().height << endlog;
            } else {
                lg(Fatal) << "Failed to load " << cfg::kCubeTexturePath << "!" << endlog;
                std::exit(-1);
            }

            ci.sid = "wall";
            ci.file.path = cfg::kGroundTexturePath;
            if (auto texture = app.textures.create(ci)) {
                ground_texture = *texture;
                lg(Info) << "Loaded " << cfg::kGroundTexturePath << " (ground) "
                         << ground_texture->getTextureInfo().width << "x"
                         << ground_texture->getTextureInfo().height << endlog;
            } else {
                lg(Fatal) << "Failed to load " << cfg::kGroundTexturePath << "!" << endlog;
                std::exit(-1);
            }
        }

        void setup_models() {
            // spec：立方体边长 1.0（ave::Prefab::cube(1.0f)），纹理 ice.png
            age::model::Prefab::cube(cfg::kCubeEdge, *cube_model);
            // 地面 60x60、uv 重复 15（ave::Prefab::plane(60,60,15) 等价），纹理 wall.jpg
            build_ground_plane(*ground_model);

            lg(Info) << "Geometry: cube=1.0 (36 indices), ground=60x60 (6 indices, uv repeat 15)" << endlog;
        }

        void setup_world() {
            //// 12 个立方体：逐字复刻 examples/fog/app_setup.cpp:setup_world ////
            // 位置 ((i%3-1)*1.6, 0.1+0.35*sin(i*0.9), -2.6*i)
            // 绕 normalize(0.25,1,0.15) 旋转 12*i 度（缩放恒为 1）
            for (int i = 0; i < cfg::kCubeCount; ++i) {
                auto& object = *objects.emplace_back(std::make_unique<Object>(em));
                object.add<comps::Tag>("cube");
                auto trs = object.add<comps::Transform>();
                auto render = object.add<ObjectRender>();
                render->model = cube_model.get();
                render->texture = cube_texture;

                trs->setPosition(glm::vec3(
                    (i % 3 - 1) * cfg::kCubeLateralStep,
                    cfg::kCubeBaseHeight + cfg::kCubeBobAmplitude * std::sin(static_cast<float>(i) * cfg::kCubeBobPhase),
                    cfg::kCubeSpacingZ * static_cast<float>(i)
                ));
                if (i != 0) {
                    trs->rotate(cfg::kCubeRotationAxis, glm::radians(cfg::kCubeRotationStepDeg * static_cast<float>(i)));
                }
            }

            //// 地面（砖墙，置于 y = -1）////
            auto& ground = *objects.emplace_back(std::make_unique<Object>(em));
            ground.add<comps::Tag>("ground");
            auto gtrs = ground.add<comps::Transform>();
            auto grender = ground.add<ObjectRender>();
            grender->model = ground_model.get();
            grender->texture = ground_texture;
            gtrs->setPosition(cfg::kGroundPosition);

            //// 相机：位置 (0,1.6,10)，pitch=-12°，yaw 由主循环按时间驱动 ////
            camera.transform().setPosition(cfg::kCameraPosition);
            camera.projector().setClipPlane(cfg::kCameraNear, cfg::kCameraFar);

            lg(Info) << "Scene ready: " << objects.size() << " renderables (12 cubes + 1 ground)" << endlog;
        }

        //// 相机系统（纯时间驱动）：yaw(t) = 30° * sin(2π t / 10)，R = Ry(yaw)·Rx(pitch) ////
        void update_camera(double t) {
            const double yaw_deg = cfg::kYawAmplitudeDeg * std::sin(2.0 * std::numbers::pi * t / cfg::kYawPeriodSec);
            const float yaw = static_cast<float>(glm::radians(static_cast<float>(yaw_deg)));
            const float pitch = static_cast<float>(glm::radians(cfg::kCameraPitchDeg));
            camera.transform().setRotation(
                glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f))
            );
        }

        void render_frame() {
            const auto framebuffer = window->getFrameBufferSize();

            camera.projector().setFOV(glm::radians(cfg::kCameraFovDeg));
            camera.projector().setAspectRatio(static_cast<float>(framebuffer.x), static_cast<float>(framebuffer.y));
            camera.projector().setClipPlane(cfg::kCameraNear, cfg::kCameraFar);
            camera.projector().buildProjectionMatrix();

            // 观察矩阵：与 examples/fog(AVE) 一致 —— 对相机 model 矩阵求逆。
            //（AGE 的 comps::Viewer::buildViewMatrix 只对 lookAt() 写入的共轭四元数成立，
            //  对 setRotation 写入的旋转不取反；为保证与 AVE 版画面等价，这里直接用同一公式。）
            const glm::mat4 view = glm::inverse(camera.transform().buildModelMatrix());

            //// 上传相机 + 全局雾参数 UBO ////
            CameraFogUbo ubo_data {
                .view = view,
                .proj = camera.projector().proj_matrix,
                .fog_color = glm::vec4(cfg::kFogColor, 1.0f),
                .fog_params = glm::vec4(cfg::kFogStart, cfg::kFogEnd, 0.0f, 0.0f),
            };
            glNamedBufferSubData(ubo, 0, sizeof(CameraFogUbo), &ubo_data);

            glViewport(0, 0, framebuffer.x, framebuffer.y);

            // 清屏色 = 雾色：没有几何的区域同样"雾茫茫"
            //（AGE 的 Window::clear 只接受 0~255 的 GLuint 清屏色，这里用原生 glClearColor）
            glClearColor(cfg::kFogColor.r, cfg::kFogColor.g, cfg::kFogColor.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shader.bind();

            Texture* current_texture = nullptr;
            auto& pool = em.get_or_create_pool<ObjectRender>();
            pool.data.for_each(
                [&](ObjectRender& render) {
                    if (!render.visible || !render.model || !render.texture) return;

                    if (render.texture != current_texture) {
                        render.texture->bind(GL_TEXTURE0);
                        current_texture = render.texture;
                    }
                    // 每个 draw 一个 model 矩阵（等价于 AVE 版 push constant）
                    u_model.uploadmat4(render.transform->buildModelMatrix());

                    render.model->draw(*window, 1, PrimitiveType::Triangles);
                }
            );
        }

        int run() {
            const double warmup = warmup_seconds();
            const double measure = measure_seconds();

            lg(Info) << "Benchmark: warmup " << warmup << "s (not counted) + measure " << measure
                     << "s, window " << cfg::kWidth << "x" << cfg::kHeight << ", swap interval 0"
                     << endlog;
            lg(Info) << "FPS statistics start after warmup; camera time includes warmup." << endlog;

            fog_gl::misc::FPSDetective detective;
            alib5::Clock loop_clock(false);
            loop_clock.start();

            //// 预热（不计入统计）////
            while (!window->shouldClose() && (loop_clock.get_all() / 1000.0) < warmup) {
                window->pollEvents();
                update_camera(loop_clock.get_all() / 1000.0);
                render_frame();
                window->display();
            }
            lg(Info) << "Warmup done (" << warmup << "s), measuring..." << endlog;

            //// 测量 ////
            detective.start();
            while (!window->shouldClose() && (loop_clock.get_all() / 1000.0) < warmup + measure) {
                window->pollEvents();
                update_camera(loop_clock.get_all() / 1000.0);
                render_frame();
                window->display();
                // spec：帧间隔口径 = 每次循环迭代末尾（present/swap 之后）打一次点
                detective.next_frame();
            }

            lg(Info) << "Measured " << detective.frames << " frames." << endlog;
            lg(Info) << detective.table() << endlog;
            return 0;
        }

    private:
        /// 与 examples/fog 的 CameraFogUbo(std430) 完全一致的布局（std140 下也成立）
        struct alignas(16) CameraFogUbo {
            glm::mat4 view;
            glm::mat4 proj;
            glm::vec4 fog_color;   ///< rgb = 雾色
            glm::vec4 fog_params;  ///< x = start，y = end
        };
    };

} // namespace fog_gl

int main() {
    {
        fog_gl::FogGLApp app;
        app.setup();
        app.run();
    }
    return 0;
}
