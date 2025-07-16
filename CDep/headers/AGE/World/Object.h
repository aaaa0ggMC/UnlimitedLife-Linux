#ifndef AGE_H_OBJ
#define AGE_H_OBJ
#include <AGE/Utils.h>
#include <AGE/World/Components.h>
#include <AGE/World/EntityManager.h>
#include <cerrno>

namespace age::world {
    using namespace comps;
    struct AGE_API Object : public NonCopyable{
    private:
        ComponentWrapper<comps::Transform> tran;
        EntityManager & em;
        EntityWrapper e;
    public:
        inline Object(EntityManager & emm):em{emm},
        e{emm.createEntity(),emm}{
            e.add<Transform>();
            tran.build(emm,e.e.id);
        }

        inline Entity getEntity(){
            return e.e;
        }

        inline EntityWrapper getEntityWrapper(){
            return e;
        }

        inline ~Object(){
            e.destroy();
        }

        inline Transform& transform(){return *tran;}
    };
}

#endif
