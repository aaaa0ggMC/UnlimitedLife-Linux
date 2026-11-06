#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <cmath>
#include <cstdio>

static glm::vec4 sky_clip(const glm::mat4& view, const glm::mat4& proj, glm::vec3 position) {
    const glm::vec4 clip = proj * glm::mat4(glm::mat3(view)) * glm::vec4(position, 1.0f);
    return {clip.x, clip.y, clip.w, clip.w};
}

int main() {
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 300.0f);
    proj[1][1] *= -1.0f;
    const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 point(0.2f, 0.3f, -1.0f);
    const auto original = sky_clip(view, proj, point);
    const auto moved = sky_clip(glm::translate(view, glm::vec3(5.0f, 8.0f, -20.0f)), proj, point);
    assert(glm::length(original - moved) < 1e-6f);
    const auto rotated = sky_clip(glm::rotate(view, 0.3f, glm::vec3(0.0f, 1.0f, 0.0f)), proj, point);
    assert(glm::length(original - rotated) > 0.1f);
    assert(std::abs(original.z / original.w - 1.0f) < 1e-6f);
    proj[0][0] *= 2.0f;
    assert(glm::length(original - sky_clip(view, proj, point)) > 0.1f);
    std::puts("SKY MATH PASSED: translation invariant, rotation/projection responsive, far depth = 1");
}
