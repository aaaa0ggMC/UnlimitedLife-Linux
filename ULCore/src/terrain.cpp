#include <ul/terrain/base.h>

using namespace ul;

//// Chunks ////
Chunk* ChunkManager::LoadChunk(crtype_t<ChunkId> id,int64_t SDimension_index) noexcept{
    ChunkInfo info{SDimension_index, id};
    auto it = chunks.find(info);
    if(it != chunks.end()){
        return it->second; // Chunk already loaded
    }

    //Step2: Create a new one
    auto it2 = chunks.emplace(info, new Chunk());
    if(requestList){
        
    }else {
        ///@TODO handle error reporting
    }
    return it2.first->second; // Return the newly created chunk
}

Chunk* ChunkManager::UnLoadChunk(crtype_t<ChunkId> id,int64_t SDimension_index) noexcept{
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