/**
 * @file loader.cppm
 * @brief Shape 文件加载：OBJ / STL（ASCII + Binary），自动识别格式
 * @version 5.0
 * @date 2026-09-29
 * @start-date 2026/09/29
 *
 * 从 AGE 的 age::model::Loader（fmt::Obj / Stl / StlAscii / StlBinary）移植：
 * - 产物改为 ave::Shape（interleaved），不再是 ModelData 的 SoA 四数组；
 * - API 改为「可选返回值 + ErrorWrapper」：失败返回 std::nullopt 并经 ew 上报，
 *   替换 AGE 的全局 Error::def.pushMessage；
 * - 格式选择从模板标签类型改为 ave::ShapeFormat 枚举；
 * - 相对 AGE 的修正与增强：
 *   - STL(含 binary) 的 facet 法线复制到该面全部 3 个顶点（AGE 里每面只写一条，
 *     法线数组与顶点数不匹配，GPU 侧会错位）；
 *   - OBJ 面支持任意边数（n-gon 扇形三角化），AGE 只处理 3/4 顶点；
 *   - 行解析基于拷贝后的零终止缓冲，不依赖 string_view 底层恰好有 '\0'；
 *   - STL binary 加入长度与三角形数边界校验（AGE 直接按头部计数读，越界即 UB）。
 */
module;
#include <AVE/config.h>

export module ave.shape:loader;

import std;
import alib6;
import ave.ecode;
import :shape;

export namespace ave {

    /**
     * @brief 几何文件格式
     * @start-date 2026/09/29
     */
    enum class ShapeFormat {
        /// Wavefront OBJ（ASCII，支持 v/vt/vn 与 v//vn 两类面索引组合）
        Obj,
        /// STL（实现内自动再识别 ASCII / Binary）
        Stl,
    };

    /// @brief 获取格式的可读名称（"Obj" / "Stl"）
    [[nodiscard]] inline constexpr std::string_view to_string(ShapeFormat format) noexcept {
        switch(format) {
            case ShapeFormat::Obj: return "Obj";
            case ShapeFormat::Stl: return "Stl";
        }
        return "Unknown";
    }

    /**
     * @brief 加载选项
     * @start-date 2026/09/29
     */
    struct AVE_API ShapeLoadOptions {
        /// 翻转 V 分量（兼容 OpenGL 风格纹理坐标；blender 导出通常不需要翻）
        bool flip_v { false };
    };

    /**
     * @brief Shape 文件加载器
     * @start-date 2026/09/29
     *
     * 不持有任何 GPU 资源；得到 Shape 后如何绑定/上传由用户自理。
     *
     * @note 自动识别规则与 AGE 一致：去除前导空白后以 "solid" 开头视为 STL ASCII，
     *       否则按 STL Binary（显式 ShapeFormat::Stl 时同样分流）。
     *       注意某些二进制 STL 的 80 字节头也以 "solid" 开头，会被误判为 ASCII
     *       ——这是该启发式的固有局限，误判时可显式指定格式规避。
     * @warning STL 无 UV：顶点 uv 统一填充为 (0, 1/0)——flip_v 决定 V 取 1 还是 0。
     */
    struct AVE_API ShapeLoader {

        /**
         * @brief 从文件加载（自动识别 Obj/Stl）
         * @param path 文件路径
         * @param options 加载选项
         * @param ew 错误上报通道
         * @return 成功返回 ave::Shape；打开/读取失败或数据为空返回 std::nullopt
         */
        [[nodiscard]] static std::optional<Shape> from_file(
            std::string_view path,
            ShapeLoadOptions options = {},
            alib6::ErrorWrapper ew = {}
        );

        /**
         * @brief 从文件加载（显式指定格式）
         * @param path 文件路径
         * @param format 强制使用的格式（跳过自动识别）
         * @param options 加载选项
         * @param ew 错误上报通道
         */
        [[nodiscard]] static std::optional<Shape> from_file(
            std::string_view path,
            ShapeFormat format,
            ShapeLoadOptions options = {},
            alib6::ErrorWrapper ew = {}
        );

        /**
         * @brief 从内存解析（自动识别 Obj/Stl）
         * @param data 文件内容（无需以 '\0' 结尾）
         * @param options 加载选项
         * @param ew 错误上报通道
         * @return 成功返回 ave::Shape；数据为空返回 std::nullopt
         */
        [[nodiscard]] static std::optional<Shape> from_memory(
            std::string_view data,
            ShapeLoadOptions options = {},
            alib6::ErrorWrapper ew = {}
        );

        /**
         * @brief 从内存解析（显式指定格式）
         * @param data 文件内容（无需以 '\0' 结尾）
         * @param format 强制使用的格式（跳过自动识别）
         * @param options 加载选项
         * @param ew 错误上报通道
         * @return 成功返回 ave::Shape；数据为空返回 std::nullopt
         */
        [[nodiscard]] static std::optional<Shape> from_memory(
            std::string_view data,
            ShapeFormat format,
            ShapeLoadOptions options = {},
            alib6::ErrorWrapper ew = {}
        );

    private:
        /// @brief 分发到具体解析器（data 非空由调用方保证）
        [[nodiscard]] static Shape parse(
            std::string_view data,
            ShapeFormat format,
            ShapeLoadOptions options
        );
    };

}

export namespace std {
    /// @brief ave::ShapeFormat 的 std::formatter 特化（转发到 ave::to_string）
    template<>
    struct formatter<ave::ShapeFormat, char> {
        constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
        auto format(ave::ShapeFormat format, format_context& ctx) const {
            return std::formatter<std::string_view, char>{}.format(
                ave::to_string(format), ctx
            );
        }
    };
}
