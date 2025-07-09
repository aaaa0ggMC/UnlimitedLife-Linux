module;
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
export module Terrain.Chunk;
import Terrain.Block;
import Base.Requests;

namespace ul{
    using ChunkId = glm::ivec2;
    export class ChunkInfo{
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
        size_t operator()(const glm::ivec2& vec) const noexcept {
            return std::hash<int>()(vec.x) ^ std::hash<int>()(vec.y);
        }
    };

    template <>
    struct hash<ul::ChunkInfo> {
        size_t operator()(const ul::ChunkInfo& info) const noexcept {
            return std::hash<unsigned int>()(info.SDimension_index) ^ std::hash<glm::ivec2>()(info.chunkId);
        }
    };
}

namespace ul{
    using BlockMap = std::vector<Block*>;
    ////Chunk "Macros"
    export constexpr unsigned int $chunk_size = 32;
    export constexpr unsigned int $chunk_size_2 = $chunk_size * $chunk_size;

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
    Block* accessBlock(BlockMap& blockMap, unsigned int x,unsigned int y) noexcept{
        if(x >= $chunk_size || y >= $chunk_size){
            return nullptr; // Out of bounds
        }
        return blockMap[x + y * $chunk_size];
    }



    /// @brief A simple chunk class that contains a block map.
    export class Chunk{
    public:
        ChunkInfo info; ///< The chunk info, which contains the chunk's metadata.
        BlockMap blocks; ///< The block map of the chunk, which contains pointers to Block objects.May contain nullptrs if the block is not initialized.

        // Initialize the block map with nullptrs
        Chunk():blocks($chunk_size_2, nullptr) {}
    };

    export class ChunkManager{
    public:
        std::unordered_map<ChunkInfo,Chunk*> chunks; ///< A map of chunk info to chunk pointers, which allows for quick access to chunks by their metadata.
        RequestList * requestList = nullptr; ///< The request list for managing chunk loading and unloading requests.

        //ids
        int64_t id_loadChunk,id_unloadChunk; ///< Request IDs for loading and unloading chunks.

        Chunk* LoadChunk(crtype_t<ChunkId> id,int64_t SDimension_index) noexcept {
            ChunkInfo info{SDimension_index, id};
            auto it = chunks.find(info);
            if(it != chunks.end()){
                return it->second; // Chunk already loaded
            }

            //Step2: Create a new one
            auto it2 = chunks.emplace(info, new Chunk());
            if(requestList){
               
            }
            return it2.first->second; // Return the newly created chunk
        }

        Chunk* UnLoadChunk(crtype_t<ChunkId> id,int64_t SDimension_index) noexcept {
            ChunkInfo info{SDimension_index, id};
            auto it = chunks.find(info);
            if(it != chunks.end()){
                Chunk* chunk = it->second;
                chunks.erase(it);
                delete chunk; // Clean up memory
                return nullptr; // Successfully unloaded
            }
            return nullptr; // Chunk not found
        }

        void bindRequestList(RequestList & list){
            requestList = &list;
            /// @TODO do more
        }

    };
}
