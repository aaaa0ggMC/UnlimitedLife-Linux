#include <AGE/World/Systems.h>
#include <stack>
#include <unordered_set>

using namespace age::world::systems;
using namespace age::world::comps;
using namespace age::world;

ComponentRequisitions<Transform> Viewer::requisitions;

void ParentSystem::update(){
    if(!pool || !transformPool)initPool();
    if(!pool || !transformPool)return;
    ///@todo 采用更加高性能的写法

    std::unordered_set<uint64_t> dealedParent;
    
    for(auto & [eid,_] : pool->mapper){
        std::stack<uint64_t> pstack;
        if(dealedParent.contains(eid))continue;
        pstack.push(eid);
        while(!pstack.empty()){
            uint64_t child_id = pstack.top();
            /*这个似乎是冗余的，测试下来多重parent chain注释这一块不影响
            if(dealedParent.contains(child_id)){
                pstack.pop();
                continue;
            }*/
            comps::Transform& child = transformPool->data[transformPool->mapper[child_id]];
            uint64_t parent_id = pool->data[pool->mapper[child_id]].parent.id;
            auto iter = pool->mapper.find(parent_id);
            if(iter != pool->mapper.end() && !dealedParent.contains(parent_id)){
                pstack.push(parent_id);
                continue;
            }
            auto piter = transformPool->mapper.find(parent_id);
            if(piter != transformPool->mapper.end()){
                child.buildModelMatrixWithParent(transformPool->data[piter->second]);
            }else child.buildModelMatrix();
            dealedParent.emplace(child_id);
            pstack.pop();
        }
    }
}