module;
#include <unordered_map>
#include <cstdint>
export module Base.Requests;

namespace ul{
    export class Request{
        int64_t requestId; ///< The unique identifier for the request.

        void * arg1;
        void * arg2;
    };

    ///request fn
    export using RequestFn = void(*)(void * arg1,void * arg2);

    export class RequestList{
    public:
        int64_t nextRequestId = 0; ///< The next request ID to be assigned,begin from 1.

        void push(Request){

        }

    };

}