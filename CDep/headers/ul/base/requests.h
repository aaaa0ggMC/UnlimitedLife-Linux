#ifndef UL_BASE_REQUESTS_H
#define UL_BASE_REQUESTS_H
#include <unordered_map>
#include <cstdint>
#include <tuple>
#include <mutex>
#include <iostream>

namespace ul{
    template<class... Ts> struct Request{
        int64_t requestId; ///< The unique identifier for the request.
        std::tuple<Ts...> args; ///< The arguments for the request,stored in a tuple. Tuple will be shallow copied.

        Request<> * next = nullptr; ///< Pointer to the next request in the list,used for chaining requests.
    };

    class RequestList{
    public:
        Request<> * head = nullptr; ///< Pointer to the head of the request list.
        std::mutex mtx; ///< Mutex for thread-safe access to the request list.

        class ScopeLock{
        public:
            std::mutex & mutex; ///< Reference to the mutex for locking.

            ScopeLock(std::mutex & mtx) : mutex(mtx) {
                mutex.lock(); // Lock the mutex when the scope lock is created.
            }

            ~ScopeLock() {
                mutex.unlock(); // Unlock the mutex when the scope lock is destroyed.
            }
        };

        template<class... Ts> inline void push(const Request<Ts...> & request){
            ScopeLock lock(mtx); // Create a scope lock to ensure thread-safe access to the request list.
            Request<Ts...> * rq = new Request<Ts...>();
            rq->requestId = request.requestId;
            rq->args = request.args;
            rq->next = nullptr;
            if(!head){
                head = (ul::Request<>*)rq; // If the list is empty, set the head to the new request.
            }else{
                rq->next = head; // Otherwise, insert the new request at the beginning of the list.
                head = (ul::Request<>*)rq; // Update the head to point to the new request.
            }
        }

        inline int64_t headId() const noexcept {
            if(head){
                return head->requestId; // Return the ID of the head request.
            }
            return INT64_MAX; // If the list is empty, return -1.
        }

        inline Request<> * pop() noexcept {
            ScopeLock lock(mtx); // Create a scope lock to ensure thread-safe access to the request list.
            if(!head) return nullptr; // If the list is empty, return nullptr.
            Request<> * rq = head; // Get the head request.
            head = head->next; // Move the head to the next request.
            rq->next = nullptr; // Clear the next pointer of the popped request.
            return rq; // Return the popped request.
        }

        template<class... Ts> inline void releaseRequest(Request<Ts...> * rq) noexcept {
            if(!rq) return; // If the request is null, do nothing.
            std::cout << "Releasing request with ID: " << rq->requestId << std::get<0>(rq->args) << std::endl; // Debug output.
            delete rq; // Delete the request to free memory.
        }

    };

}

#endif // UL_BASE_REQUESTS_H