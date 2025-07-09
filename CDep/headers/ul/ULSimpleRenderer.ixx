module;

export module ULSimpleRenderer;

import ULServer;

namespace ul {

    export class ULSimpleRenderer {
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