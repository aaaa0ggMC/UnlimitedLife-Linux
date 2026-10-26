#include <AVE/error_code.h>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

namespace {
void check(bool ok, const char* message) {
    if(!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

struct TrackingResource : std::pmr::memory_resource {
    std::size_t live_bytes {0};
    std::size_t allocations {0};

    void* do_allocate(std::size_t size, std::size_t alignment) override {
        void* result = std::pmr::new_delete_resource()->allocate(size, alignment);
        live_bytes += size;
        ++allocations;
        return result;
    }
    void do_deallocate(void* pointer, std::size_t size, std::size_t alignment) override {
        live_bytes -= size;
        std::pmr::new_delete_resource()->deallocate(pointer, size, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
} // namespace

int main() {
    TrackingResource resource;
    {
        ave::Error errors(&resource);
        ave::ErrorWrapper wrapper(errors);
        check(bool(wrapper), "host-backed error receiver is active");
        // AVE 必须直接复用 alib5 的实现，而不是自带一份壳。
        static_assert(std::is_same_v<ave::ErrorWrapper, alib5::ErrorWrapper>,
            "ave::ErrorWrapper must be alib5::ErrorWrapper");
        static_assert(std::is_same_v<ave::Error, alib5::Error>, "ave::Error must be alib5::Error");
        static_assert(std::is_same_v<ave::ErrorRecord, alib5::ErrorRecord>,
            "ave::ErrorRecord must be alib5::ErrorRecord");
        static_assert(std::is_same_v<ave::CodeWithLocation, alib5::CodeWithLocation>,
            "ave::CodeWithLocation must be alib5::CodeWithLocation");
        // alib6 语义：bool 表示“是否会上报”；无宿主时按默认策略（panic）为真。
        check(bool(ave::ErrorWrapper{}) == alib5::ErrorWrapper::__need_invoke_error(),
            "empty receiver activity must follow the alib5 policy");
        const auto line = std::source_location::current().line() + 1;
        wrapper.report("literal", 17);
        check(errors.size() == 1 && errors[0].message == "literal" && errors[0].code == 17,
            "literal reporting must preserve message and code");
        check(errors[0].location.line() == line, "implicit error code must retain caller location");
        const std::string text(512, 'x');
        wrapper.report(std::string_view(text), 18);
        check(errors[1].message == text.c_str(), "string_view overload must copy the message");
        check(errors[1].message.get_allocator().resource() == &resource,
            "copied messages must use the error receiver allocator");
        std::pmr::string owned(256, 'y', &resource);
        wrapper.report(std::move(owned), 19);
        wrapper.report(20, "formatted {}", 42);
        check(errors.size() == 4 && errors[3].message == "formatted 42", "all reporting overloads");
        errors.clear();
        check(!errors.has_error() && errors.begin() == errors.end(), "clear must hide old records");
        wrapper.report(21, "replacement");
        check(errors.size() == 1 && errors[0].code == 21 && errors[0].message == "replacement",
            "reporting after clear must reuse the first record");
        check(resource.allocations > 0, "error records must allocate through the supplied resource");
    }
    check(resource.live_bytes == 0, "all error storage must be released");
    std::puts("AVE ERROR TESTS PASSED");
}
