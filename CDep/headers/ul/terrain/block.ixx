module;
#include <glm/glm.hpp>
#include <ctype.h>
export module Terrain.Block;

namespace ul{
    // this is unexported, but used in Block
    class Chunk;

    export class Block{
    public:
        ////properties

        ///Position of the block in the chunk
        glm::ivec2 relativePosition = glm::ivec2(0, 0);
        ///Chunk Id
        Chunk * chunk = nullptr;
        ///Block Type SID index,the SID_index floats when the program restarted,so to store the block type, we use SID rather than the index
        int64_t SID_index = 0;

    };
}
