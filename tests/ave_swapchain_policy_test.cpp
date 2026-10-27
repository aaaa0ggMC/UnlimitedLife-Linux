#include "../modules/AVE/swapchain_policy.inl"
#include <cstdio>
#include <cstdlib>

static void check(bool value, const char* message) {
    if(!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static bool equal(VkExtent2D a, VkExtent2D b) {
    return a.width == b.width && a.height == b.height;
}

int main() {
    using namespace ave::detail;
    VkSurfaceCapabilitiesKHR cap {};
    cap.currentExtent = {1080, 2400};
    cap.minImageExtent = {1, 1};
    cap.maxImageExtent = {4096, 4096};
    cap.supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR |
        VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR | VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;
    const VkExtent2D landscape {2400, 1080};
    for(const auto transform : {VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,
                               VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR,
                               VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR,
                               VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR}) {
        cap.currentTransform = transform;
        const auto window = window_extent(cap, landscape, true);
        const auto extent = swapchain_extent(cap, window, true);
        check(equal(extent, landscape), "Android keeps SurfaceView landscape extent");
        check(swapchain_transform(cap, true) == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
              "non-prerotated rendering must not claim currentTransform");
        check(valid_swapchain_extent(cap, extent, true), "Android permits non-current extent");
        check(equal(window_extent(cap, landscape, false), cap.currentExtent),
              "desktop fixed extent remains authoritative");
        check(equal(swapchain_extent(cap, landscape, false), cap.currentExtent),
              "desktop configuration still uses fixed extent");
        check(!valid_swapchain_extent(cap, landscape, false), "desktop rejects non-current extent");
        check(swapchain_transform(cap, false) == transform, "desktop transform unchanged");
    }
    check(equal(window_extent(cap, std::nullopt, true), cap.currentExtent),
          "Android without host size falls back to surface extent");
    cap.currentExtent = {std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()};
    for(const bool android : {false, true}) {
        check(equal(window_extent(cap, landscape, android), landscape), "undefined extent uses window");
        check(equal(swapchain_extent(cap, {8000, 9000}, android), {4096, 4096}),
              "oversize requests are clamped");
        check(!valid_swapchain_extent(cap, {0, 1080}, android), "zero extent rejected");
        check(!valid_swapchain_extent(cap, {4097, 1080}, android), "over-limit extent rejected");
        check(equal(window_extent(cap, std::nullopt, android), cap.minImageExtent),
              "missing extent falls back to minimum");
    }
    std::puts("Android/desktop swapchain orientation and extent policy: PASS");
}
