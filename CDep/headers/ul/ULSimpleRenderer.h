#ifndef UL_ULSIMPLE_RENDERER_H
#define UL_ULSIMPLE_RENDERER_H

#include "ULServer.h"

namespace ul {
    class ULSimpleRenderer {
    public:
        ULSimpleRenderer(){
        }

        void bindServer(ULServer* server){
            // Bind the server to this renderer
            this->server = server;
        }

        void launch(){
            // Launch the renderer
            if (server) {
                // Use server data for rendering
            }
        }
    private:
        ULServer* server = nullptr;
    };
}

#endif // UL_ULSIMPLE_RENDERER_H