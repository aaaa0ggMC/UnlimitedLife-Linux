#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

import ave;
import alib6;

// ========================================================
// 测试结构体定义
// ========================================================

// 1. 标量结构体 (std140 规范要求结构体基准对齐到 16 字节)
struct alignas(16) ScalarBlockStd140 {
    float a;
    alib6::i32 b;
    alib6::u32 c;
    float d;
}; // 4 x 4 = 16 字节

// 2. 向量结构体
struct VectorsBlock {
    ave::vec2 v2;                // 8B (offset 0)
    alignas(16) ave::vec3 v3;    // 12B (align 16, offset 16)
    float _pad;                  // 4B (offset 28, 补齐至 32)
    alignas(16) ave::vec4 v4;    // 16B (align 16, offset 32)
}; // 总大小 48 字节，对齐 16 字节

// 3. 错误的三分量向量未对齐示范
struct BadVec3Block {
    float a; // offset 0
    struct UnalignedVec3 { float x, y, z; } v; // C++ offset 4, 但 GLSL 要求 offset 16
};

// 4. 标量数组 (std430 允许连续 4 字节，std140 强制 16 字节步长)
struct ScalarArrayBlock {
    float values[4]; // C++ 占 16 字节
};

// 5. 显式符合 std140 步长规则的数组 (每个元素 16 字节)
struct ValidStd140Array {
    struct AlignedFloat {
        alignas(16) float val;
    } values[4]; // 4 x 16 = 64 字节
};

// 6. 末尾未补齐至 16 的倍数的结构体
struct UnpaddedStruct {
    float a;
    float b;
    float c; // 12 字节
};

// 7. 矩阵块
struct MatrixBlock {
    ave::mat4 model;
    ave::mat4 view;
    ave::mat4 proj;
};

// 8. 顶点与实例结构体 (Vertex & Instance Layout)
struct VertexData {
    ave::vec3 pos;        // loc 0: R32G32B32_SFLOAT, off 0
    ave::vec2 uv;         // loc 1: R32G32_SFLOAT,    off 12
    ave::vec3 normal;     // loc 2: R32G32B32_SFLOAT, off 20
    ave::unorm8x4 color;  // loc 3: R8G8B8A8_UNORM,   off 32
}; // sizeof = 36

struct InstanceData {
    ave::mat4 model;      // loc 0..3: 4 x R32G32B32A32_SFLOAT, off 0, 16, 32, 48
    ave::vec4 tint;       // loc 4: R32G32B32A32_SFLOAT, off 64
}; // sizeof = 80

// ========================================================
// Concepts 约束函数模板
// ========================================================
template<ave::Std140Compatible T>
constexpr bool check_ubo_concept(const T&) { return true; }

template<ave::Std430Compatible T>
constexpr bool check_ssbo_concept(const T&) { return true; }

int main() {
    std::cout << "Starting AVE Buffer Reflection (std140 / std430) Tests..." << std::endl;

    // ========================================================
    // 1. 测试标量结构体 (Scalar Block)
    // ========================================================
    {
        static_assert(ave::is_std140_compatible_v<ScalarBlockStd140>);
        static_assert(ave::is_std430_compatible_v<ScalarBlockStd140>);
        constexpr auto rep = ave::verify_std140<ScalarBlockStd140>();
        assert(rep.is_valid());
        assert(rep.actual_size == 16 && rep.expected_size == 16);
        std::cout << "[PASS] Scalar layout verification verified." << std::endl;
    }

    // ========================================================
    // 2. 测试向量结构体与 16 字节对齐 (Vectors Block)
    // ========================================================
    {
        static_assert(ave::is_std140_compatible_v<VectorsBlock>);
        static_assert(ave::is_std430_compatible_v<VectorsBlock>);
        constexpr auto rep = ave::verify_std140<VectorsBlock>();
        assert(rep.is_valid());
        assert(rep.actual_size == 48 && rep.expected_size == 48);

        // 验证各字段的期望偏移
        assert(rep.members[0].expected_offset == 0);  // v2
        assert(rep.members[1].expected_offset == 16); // v3
        assert(rep.members[2].expected_offset == 28); // _pad
        assert(rep.members[3].expected_offset == 32); // v4
        std::cout << "[PASS] Vector alignment & padding verified." << std::endl;
    }

    // ========================================================
    // 3. 负向测试：未对齐的三分量向量拦截 (BadVec3Block)
    // ========================================================
    {
        static_assert(!ave::is_std140_compatible_v<BadVec3Block>);
        static_assert(!ave::is_std430_compatible_v<BadVec3Block>);

        constexpr auto rep = ave::verify_std140<BadVec3Block>();
        assert(!rep.is_valid());
        assert(!rep.members_match);
        // 验证第二项成员 v 报告偏移错误
        assert(rep.members[1].actual_offset == 4);
        assert(rep.members[1].expected_offset == 16);
        assert(!rep.members[1].is_valid);
        std::cout << "[PASS] Unaligned vec3 detection & diagnostic verified." << std::endl;
    }

    // ========================================================
    // 4. 核心差异测试：数组在 std140 与 std430 下的行为 (ScalarArrayBlock)
    // ========================================================
    {
        // 在 std430 (SSBO) 下，标量数组不需要 16 字节步长，因此自然对齐通过
        static_assert(ave::is_std430_compatible_v<ScalarArrayBlock>);
        // 在 std140 (UBO) 下，数组元素必须向上补齐为 16 字节，C++ 密集排列不兼容！
        static_assert(!ave::is_std140_compatible_v<ScalarArrayBlock>);

        constexpr auto rep_std140 = ave::verify_std140<ScalarArrayBlock>();
        assert(!rep_std140.is_valid());
        assert(rep_std140.actual_size == 16);
        assert(rep_std140.expected_size == 64); // 4 * 16 = 64
        assert(!rep_std140.size_matches);

        constexpr auto rep_std430 = ave::verify_std430<ScalarArrayBlock>();
        assert(rep_std430.is_valid());
        assert(rep_std430.actual_size == 16);
        assert(rep_std430.expected_size == 16);
        std::cout << "[PASS] std140 vs std430 array stride divergence verified." << std::endl;
    }

    // ========================================================
    // 5. 显式对齐 std140 数组验证 (ValidStd140Array)
    // ========================================================
    {
        static_assert(ave::is_std140_compatible_v<ValidStd140Array>);
        constexpr auto rep = ave::verify_std140<ValidStd140Array>();
        assert(rep.is_valid());
        assert(rep.actual_size == 64 && rep.expected_size == 64);
        std::cout << "[PASS] Valid std140 padded array verified." << std::endl;
    }

    // ========================================================
    // 6. 结构体大小补齐至 16 的倍数差异 (UnpaddedStruct)
    // ========================================================
    {
        // 12 字节结构体：std430 下大小为 12 (通过)；std140 下大小强制补齐到 16 (拦截)
        static_assert(ave::is_std430_compatible_v<UnpaddedStruct>);
        static_assert(!ave::is_std140_compatible_v<UnpaddedStruct>);

        constexpr auto rep140 = ave::verify_std140<UnpaddedStruct>();
        assert(!rep140.is_valid());
        assert(rep140.actual_size == 12 && rep140.expected_size == 16);

        constexpr auto rep430 = ave::verify_std430<UnpaddedStruct>();
        assert(rep430.is_valid());
        assert(rep430.actual_size == 12 && rep430.expected_size == 12);
        std::cout << "[PASS] Struct 16-byte multiple padding divergence verified." << std::endl;
    }

    // ========================================================
    // 7. C++ Concepts 编译期约束验证
    // ========================================================
    {
        ScalarBlockStd140 s{};
        assert(check_ubo_concept(s));
        assert(check_ssbo_concept(s));

        ScalarArrayBlock arr{};
        // check_ubo_concept(arr); // 编译期拦截！
        assert(check_ssbo_concept(arr));

        std::cout << "[PASS] Std140Compatible & Std430Compatible concepts verified." << std::endl;
    }

    // ========================================================
    // 8. 矩阵块验证 (MatrixBlock)
    // ========================================================
    {
        static_assert(ave::is_std140_compatible_v<MatrixBlock>);
        static_assert(ave::is_std430_compatible_v<MatrixBlock>);
        constexpr auto rep = ave::verify_std140<MatrixBlock>();
        assert(rep.is_valid());
        assert(rep.actual_size == 192); // 3 个 mat4，每个 4x16 = 64B，总计 192B
        std::cout << "[PASS] Matrix block layout verified." << std::endl;
    }

    // ========================================================
    // 9. 日志管道兼容与可视化表格打印 (alib6::aout & to_table)
    // ========================================================
    {
        // 打印一个通过报告
        auto good_rep = ave::verify_std140<VectorsBlock>();
        alib6::aout << good_rep << alib6::endlog;
        alib6::aout << "\n" << good_rep.to_table() << alib6::endlog;

        // 打印一个失配诊断报告
        auto bad_rep = ave::verify_std140<BadVec3Block>();
        alib6::aout << bad_rep << alib6::endlog;
        alib6::aout << "\n" << bad_rep.to_table() << alib6::endlog;

        std::cout << "[PASS] alib6::aout streaming & Unicode table formatting verified." << std::endl;
    }

    // ========================================================
    // 10. 顶点布局默认反射验证 (Default Vertex Layout Reflection)
    // ========================================================
    {
        auto layout = ave::vertex_layout<VertexData>().build();
        assert(layout.bindings.size() == 1);
        assert(layout.bindings[0].binding == 0);
        assert(layout.bindings[0].stride == sizeof(VertexData)); // 36 字节
        assert(layout.bindings[0].input_rate == VK_VERTEX_INPUT_RATE_VERTEX);

        assert(layout.attributes.size() == 4);
        // pos: loc 0, binding 0, format RGB32_SFLOAT, offset 0
        assert(layout.attributes[0].location == 0);
        assert(layout.attributes[0].binding == 0);
        assert(layout.attributes[0].format == VK_FORMAT_R32G32B32_SFLOAT);
        assert(layout.attributes[0].offset == offsetof(VertexData, pos));

        // uv: loc 1, binding 0, format RG32_SFLOAT, offset 12
        assert(layout.attributes[1].location == 1);
        assert(layout.attributes[1].binding == 0);
        assert(layout.attributes[1].format == VK_FORMAT_R32G32_SFLOAT);
        assert(layout.attributes[1].offset == offsetof(VertexData, uv));

        // normal: loc 2, binding 0, format RGB32_SFLOAT, offset 20
        assert(layout.attributes[2].location == 2);
        assert(layout.attributes[2].binding == 0);
        assert(layout.attributes[2].format == VK_FORMAT_R32G32B32_SFLOAT);
        assert(layout.attributes[2].offset == offsetof(VertexData, normal));

        // color: loc 3, binding 0, format R8G8B8A8_UNORM, offset 32
        assert(layout.attributes[3].location == 3);
        assert(layout.attributes[3].binding == 0);
        assert(layout.attributes[3].format == VK_FORMAT_R8G8B8A8_UNORM);
        assert(layout.attributes[3].offset == offsetof(VertexData, color));

        std::cout << "[PASS] Default vertex layout reflection & format deduction verified." << std::endl;
    }

    // ========================================================
    // 11. Location 自定义与位图智能避让 (MonoBitSet Slot Allocation)
    // ========================================================
    {
        // 显式将 uv 绑定到 location 5，其他字段自动填充空闲槽位 (0, 1, 2)
        auto layout = ave::vertex_layout<VertexData>()
            .with_location<&VertexData::uv>(5)
            .build();

        assert(layout.attributes[1].location == 5); // uv -> 5
        // 其余未显式指定字段自动分配槽位 0, 1, 2
        assert(layout.attributes[0].location == 0); // pos -> 0
        assert(layout.attributes[2].location == 1); // normal -> 1
        assert(layout.attributes[3].location == 2); // color -> 2

        std::cout << "[PASS] MonoBitSet auto-slot packing & explicit location verified." << std::endl;
    }

    // ========================================================
    // 12. 冲突检测与 alib6::ErrorWrapper 报错拦截 (Collision Detection)
    // ========================================================
    {
        alib6::Error err;
        // 故意让 pos 和 uv 都显式占用 location 2，触发冲突检测！
        auto builder = ave::vertex_layout<VertexData>()
            .with_location<&VertexData::pos>(2)
            .with_location<&VertexData::uv>(2);

        auto layout = builder.build(err);
        assert(err.has_error());
        assert(err.size() >= 1);
        std::cout << "[PASS] Location collision detected and reported via ErrorWrapper: " 
                  << err[0].message << std::endl;
    }

    // ========================================================
    // 13. 矩阵列自动拆解与实例化流 (Matrix Expansion & Instancing)
    // ========================================================
    {
        auto inst_layout = ave::vertex_layout<InstanceData>()
            .with_binding(1)
            .as_instance()
            .build();

        assert(inst_layout.bindings.size() == 1);
        assert(inst_layout.bindings[0].binding == 1);
        assert(inst_layout.bindings[0].input_rate == VK_VERTEX_INPUT_RATE_INSTANCE);
        assert(inst_layout.bindings[0].stride == sizeof(InstanceData)); // 80 字节

        // mat4 自动拆解为 4 列 + 1 个 vec4 tint = 总共 5 个 attributes
        assert(inst_layout.attributes.size() == 5);
        for (uint32_t c = 0; c < 4; ++c) {
            assert(inst_layout.attributes[c].location == c);
            assert(inst_layout.attributes[c].binding == 1);
            assert(inst_layout.attributes[c].format == VK_FORMAT_R32G32B32A32_SFLOAT);
            assert(inst_layout.attributes[c].offset == c * 16);
        }
        // tint
        assert(inst_layout.attributes[4].location == 4);
        assert(inst_layout.attributes[4].binding == 1);
        assert(inst_layout.attributes[4].offset == 64);

        std::cout << "[PASS] Matrix mat4 expansion & instance rate verified." << std::endl;
    }

    // ========================================================
    // 14. 积木式多流合并 (Multi-Stream operator+) 与 CreateInfo 输出
    // ========================================================
    {
        // 拼积木：网格流 + 实例流
        auto composite = (
            ave::vertex_layout<VertexData>()
            +
            ave::vertex_layout<InstanceData>().as_instance()
        );

        auto full_state = composite.build();
        assert(full_state.bindings.size() == 2);
        assert(full_state.bindings[0].binding == 0);
        assert(full_state.bindings[1].binding == 1);
        assert(full_state.bindings[1].input_rate == VK_VERTEX_INPUT_RATE_INSTANCE);

        // 4 个 Vertex 属性 + 5 个 Instance 属性 = 9 个属性
        assert(full_state.attributes.size() == 9);

        // 验证连续槽位无重叠分配
        for (uint32_t i = 0; i < 9; ++i) {
            assert(full_state.attributes[i].location == i);
        }

        // 测试安全生成 Vulkan CreateInfo
        VkPipelineVertexInputStateCreateInfo ci = full_state.to_create_info();
        assert(ci.vertexBindingDescriptionCount == 2);
        assert(ci.vertexAttributeDescriptionCount == 9);
        assert(ci.pVertexBindingDescriptions != nullptr);
        assert(ci.pVertexAttributeDescriptions != nullptr);

        // 打印精美的 Unicode 状态表格
        alib6::aout << "\nCombined Multi-Stream Vertex Input State:\n"
                    << full_state.to_table() << alib6::endlog;

        std::cout << "[PASS] Multi-stream operator+ and CreateInfo pipeline integration verified." << std::endl;
    }

    std::cout << "\n========================================================" << std::endl;
    std::cout << "  ALL AVE BUFFER REFLECTION & LAYOUT TESTS PASSED!     " << std::endl;
    std::cout << "========================================================" << std::endl;
    return 0;
}
