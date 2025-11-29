#include <AGE/World/Systems.h>
#include <stack>
#include <unordered_set>

using namespace age::world::systems;
using namespace age::world::comps;
using namespace age::world;
using namespace alib::g3::ecs;

void ParentSystem::update(){
    static thread_local detail::MonoticBitSet bitset;
    bitset.ensure(em.get_entity_pool_size());
    bitset.clear(false);

    pool.data.for_each(
        [this](Parent & root){
            std::stack<Parent*> pstack;
            if(bitset.get(root.get_bound().id - 1))return;
            pstack.push(&root);

            while(!pstack.empty()){
                Parent & p = *pstack.top();
                panicf_debug(
                    !p.parent.id || !p.get_bound().id,
                    "Invalid entity!Current[%d],Parent[%d]",
                p.get_bound().id,p.parent.id);
                auto iter  = this->pool.mapper.find(p.parent.id);
                if(!bitset.get(p.parent.id - 1) && iter != this->pool.mapper.end()){
                    pstack.push(&this->pool.data[(iter->second)]);
                    continue;
                }
                auto citer = this->transformPool.mapper.find(p.get_bound().id); 
                if(citer != this->transformPool.mapper.end()){
                    Transform& child = this->transformPool.data[citer->second];
                    auto piter = this->transformPool.mapper.find(p.parent.id);
                    
                    if(piter != this->transformPool.mapper.end()){
                        child.buildModelMatrixWithParent(this->transformPool.data[piter->second]);
                    }else child.buildModelMatrix();
                }
                
                bitset.set(p.get_bound().id - 1);
                pstack.pop();
            }

        }
    );
}