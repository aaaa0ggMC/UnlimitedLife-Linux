#ifndef UL_TERRAIN_CHUNK_H
#define UL_TERRAIN_CHUNK_H

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "block.h"
#include "../base/requests.h"

namespace ul{
    using ChunkId = glm::ivec2;
    class ChunkInfo{
    public:
        int64_t SDimension_index;///< The dimension id of the chunk,which floats when the program restarted 
        ChunkId chunkId;

        ///implement the equality operator for ChunkInfo
        bool operator==(const ChunkInfo& other) const noexcept {
            return SDimension_index == other.SDimension_index && chunkId == other.chunkId;
        }
    };
}

///implement hash
namespace std {
    ///implement hash for ivec2
    template <>
    struct hash<glm::ivec2> {
        inline size_t operator()(const glm::ivec2& vec) const noexcept {
            return std::hash<int>()(vec.x) ^ std::hash<int>()(vec.y);
        }
    };

    template <>
    struct hash<ul::ChunkInfo> {
        inline size_t operator()(const ul::ChunkInfo& info) const noexcept {
            return std::hash<unsigned int>()(info.SDimension_index) ^ std::hash<glm::ivec2>()(info.chunkId);
        }
    };
}

namespace ul{
    using BlockMap = std::vector<Block*>;
    ////Chunk "Macros"
    constexpr unsigned int $chunk_size = 32;
    constexpr unsigned int $chunk_size_2 = $chunk_size * $chunk_size;

    template<typename T> struct __ConstRefType {
        using type = const T&;
    };
    template<typename T> using crtype_t = __ConstRefType<T>::type;  

    /// @brief  Access a block in the block map by its coordinates.
    /// @param blockMap 
    /// @param x 
    /// @param y 
    /// @return returns a pointer to the Block at the specified coordinates, or nullptr if out of bounds.
    /// @note  note that this function does not check if the block is null, so you should check it before using it.
    inline Block* accessBlock(BlockMap& blockMap, unsigned int x,unsigned int y) noexcept{
        if(x >= $chunk_size || y >= $chunk_size){
            return nullptr; // Out of bounds
        }
        return blockMap[x + y * $chunk_size];
    }



    /// @brief A simple chunk class that contains a block map.
    class Chunk{
    public:
        ChunkInfo info; ///< The chunk info, which contains the chunk's metadata.
        BlockMap blocks; ///< The block map of the chunk, which contains pointers to Block objects.May contain nullptrs if the block is not initialized.

        // Initialize the block map with nullptrs
        Chunk():blocks($chunk_size_2, nullptr) {}
    };

    class ChunkManager{
    public:
        std::unordered_map<ChunkInfo,Chunk*> chunks; ///< A map of chunk info to chunk pointers, which allows for quick access to chunks by their metadata.
        RequestList * requestList = nullptr; ///< The request list for managing chunk loading and unloading requests.
        int64_t id_loadChunk,id_unloadChunk; ///< Request IDs for loading and unloading chunks.

        ChunkManager(RequestList & rq,int64_t loadChunk, int64_t unloadChunk) noexcept
            : requestList(&rq), id_loadChunk(loadChunk), id_unloadChunk(unloadChunk) {};

        Chunk* LoadChunk(crtype_t<ChunkId> id,int64_t SDimension_index) noexcept;
        Chunk* UnLoadChunk(crtype_t<ChunkId> id,int64_t SDimension_index) noexcept;
    };
}

#endif // UL_TERRAIN_CHUNK_H