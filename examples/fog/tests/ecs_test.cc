/**
 * @file ecs_test.cc
 * @brief fog ECS 行为回归测试（alib5 原生 ECS）
 *
 * 原先经 ecs_compat.h 兼容层调用 alib6 形状 API；兼容层删除后改为 alib5 原生
 * `view() / is_valid() / entity_count()`，断言保持等价，不降低覆盖。
 */
#include <alib5/aecs.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {
struct Position { int value; };
struct Velocity { int value; };
void check(bool ok, const char* text) {
    if(!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
}

int main() {
    alib5::ecs::EntityManager em;
    auto first = em.create_entity();
    em.add_component<Position>(first, 1);
    em.add_component<Velocity>(first, 2);
    auto partial = em.create_entity();
    em.add_component<Position>(partial, 10);
    auto view = em.view<Position, Velocity>();
    check(view.count() == 1 && !view.empty(), "multi-component intersection");
    for(auto&& [entity, position, velocity] : view) position.value += velocity.value;
    check(em.get_component_raw<Position>(first)->value == 3, "range traversal must expose component references");
    auto added = em.create_entity();
    em.add_component<Position>(added, 20);
    em.add_component<Velocity>(added, 4);
    std::size_t visited = 0;
    for(auto&& [entity, position, velocity] : view) ++visited;
    check(visited == 2, "a reused view must see newly created entities");
    em.destroy_entity(first);
    check(!em.is_valid(first), "destroyed entity must be invalid");
    check(em.entity_count() == 2 && view.count() == 1, "live counts must exclude freed slots");
    const auto replacement = em.create_entity();
    check(replacement.id == first.id && replacement.version != first.version, "slot reuse must advance generation");
    check(em.is_valid(replacement) && !em.is_valid(first), "generation validation");
    check(!em.is_valid(alib5::ecs::Entity{}), "null entity must be invalid");
    view.for_each([](Position& position, Velocity& velocity) { position.value += velocity.value; });
    check(em.get_component_raw<Position>(added)->value == 24, "for_each semantics");
    std::puts("FOG ECS TESTS PASSED");
}
