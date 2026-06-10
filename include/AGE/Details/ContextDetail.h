/**
 * @file ContextDetail.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 一些头文件 
 * @version 5.0
 * @date 2026-06-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef AGE_DET_CONTEXT_H
#define AGE_DET_CONTEXT_H
#include <AGE/Utils.h>

namespace age::context{
    struct ViewportInfo{
        int x;
        int y;
        std::size_t width;
        std::size_t height;

        bool operator==(const ViewportInfo & b) const {
            return x == b.x && y == b.y && width == b.width && height == b.height;
        }
    };

    enum class DrawBuffer : GLenum {
        None = GL_NONE,

        // Basic Draws
        Front = GL_FRONT,
        Back = GL_BACK,
        FrontAndBack = GL_FRONT_AND_BACK,

        // Stereo / Directional Draws (立体/左右视图缓冲)
        Left = GL_LEFT,
        Right = GL_RIGHT,
        FrontLeft = GL_FRONT_LEFT,
        FrontRight = GL_FRONT_RIGHT,
        BackLeft = GL_BACK_LEFT,
        BackRight = GL_BACK_RIGHT
    };

    struct Depth{
        bool enabled;
        GLint function;
        GLboolean mask;
    };

    enum class CullFaceMode : GLenum {
        Front = GL_FRONT,
        Back = GL_BACK,
        FrontAndBack = GL_FRONT_AND_BACK
    };

    enum class FrontFaceDirection : GLenum {
        CW = GL_CW,   // 顺时针
        CCW = GL_CCW  // 逆时针
    };

    struct CullFace {
        bool enabled;
        CullFaceMode mode;
    };

    enum class PolygonModeEnum : GLenum {
        Point = GL_POINT,
        Line = GL_LINE,
        Fill = GL_FILL
    };

    enum class PolygonFace : GLenum {
        Front = GL_FRONT,
        Back = GL_BACK,
        FrontAndBack = GL_FRONT_AND_BACK
    };

    struct PolygonMode {
        PolygonModeEnum front;
        PolygonModeEnum back;

        bool operator==(const PolygonMode& b) const {
            return front == b.front && back == b.back;
        }
    };

    struct PolygonOffset {
        bool fill_enabled;
        bool line_enabled;
        bool point_enabled;
        GLfloat factor;
        GLfloat units;
    };
}

#endif