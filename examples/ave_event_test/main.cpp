#include <cassert>
#include <iostream>
#include <vector>
#include <string>

import ave;
import alib6;

int main() {
    std::cout << "Starting AVE Event & Input Systems Test..." << std::endl;

    // ========================================================
    // 1. 测试按键名称反射 (constexpr Reflection & Lookup)
    // ========================================================
    {
        assert(ave::get_key_name(ave::KeyCode::W) == "W");
        assert(ave::get_key_name(ave::KeyCode::Space) == "Space");
        assert(ave::get_key_name(ave::KeyCode::Escape) == "Escape");
        assert(ave::get_key_name(ave::KeyCode::LeftShift) == "LeftShift");
        assert(ave::get_mouse_button_name(ave::MouseButton::Left) == "MouseLeft");
        assert(ave::get_mouse_button_name(ave::MouseButton::Right) == "MouseRight");
        std::cout << "[PASS] Key & Mouse name lookup verified." << std::endl;
    }

    // ========================================================
    // 2. 测试事件分类与 EventDispatcher
    // ========================================================
    {
        ave::WindowResizeEvent resize_ev(1280, 720);
        assert(resize_ev.get_type() == ave::EventType::WindowResize);
        assert(resize_ev.width == 1280 && resize_ev.height == 720);
        assert(resize_ev.is_in_category(ave::EventCategory::Window));
        assert(!resize_ev.is_in_category(ave::EventCategory::Keyboard));

        ave::KeyPressedEvent key_ev(ave::KeyCode::Space, 42, ave::KeyModifier::Shift | ave::KeyModifier::Control);
        assert(key_ev.get_type() == ave::EventType::KeyPressed);
        assert(key_ev.key == ave::KeyCode::Space);
        assert(key_ev.has_shift());
        assert(key_ev.has_control());
        assert(!key_ev.has_alt());
        assert(key_ev.is_in_category(ave::EventCategory::Keyboard));
        assert(key_ev.is_in_category(ave::EventCategory::Input));

        // Dispatcher 测试
        bool handled_resize = false;
        ave::EventDispatcher dispatcher(resize_ev);
        dispatcher.dispatch<ave::WindowResizeEvent>([&](ave::WindowResizeEvent& e) {
            handled_resize = true;
            assert(e.width == 1280);
            return true;
        });
        assert(handled_resize);
        assert(resize_ev.handled);

        std::cout << "[PASS] Event classification & EventDispatcher verified." << std::endl;
    }

    // ========================================================
    // 3. 测试 Window 多监听器与消费拦截 (handled)
    // ========================================================
    {
        ave::Window window;
        int listener1_calls = 0;
        int listener2_calls = 0;

        auto id1 = window.add_event_listener([&](ave::Event& e) {
            ++listener1_calls;
            // 标记为消费
            e.handled = true;
        });

        auto id2 = window.add_event_listener([&](ave::Event& e) {
            // 不应执行，因为上一层已经 handled
            ++listener2_calls;
        });

        ave::WindowCloseEvent close_ev;
        window.dispatch_event(close_ev);

        assert(listener1_calls == 1);
        assert(listener2_calls == 0); // 验证拦截成功

        // 移除监听器测试
        bool removed = window.remove_event_listener(id1);
        assert(removed);

        window.dispatch_event(close_ev);
        // 现在 listener1 移除了，listener2 应该被调用
        assert(listener1_calls == 1);
        assert(listener2_calls == 1);

        std::cout << "[PASS] Window multi-listeners & event interception verified." << std::endl;
    }

    // ========================================================
    // 4. 测试 Input 状态轮询体系 (按键生命周期)
    // ========================================================
    {
        ave::Input input;

        // 初始状态
        assert(input.is_key_up(ave::KeyCode::W));
        assert(!input.is_key_down(ave::KeyCode::W));
        assert(!input.is_key_just_pressed(ave::KeyCode::W));

        // Frame 0: 模拟事件 KeyPressed
        ave::KeyPressedEvent press_w(ave::KeyCode::W, 0, ave::KeyModifier::None);
        input.on_event(press_w);

        assert(input.is_key_down(ave::KeyCode::W));
        assert(input.is_key_just_pressed(ave::KeyCode::W));
        assert(!input.is_key_just_released(ave::KeyCode::W));

        // Frame 1: 换帧 new_frame (PressedThisTick -> Held)
        input.new_frame();

        assert(input.is_key_down(ave::KeyCode::W));
        assert(!input.is_key_just_pressed(ave::KeyCode::W)); // 已经不再是刚刚按下
        assert(!input.is_key_just_released(ave::KeyCode::W));

        // Frame 2: 模拟事件 KeyReleased
        ave::KeyReleasedEvent rel_w(ave::KeyCode::W, 0, ave::KeyModifier::None);
        input.on_event(rel_w);

        assert(!input.is_key_down(ave::KeyCode::W));
        assert(input.is_key_up(ave::KeyCode::W));
        assert(input.is_key_just_released(ave::KeyCode::W));

        // Frame 3: 换帧 (ReleasedThisTick -> Released)
        input.new_frame();
        assert(input.is_key_up(ave::KeyCode::W));
        assert(!input.is_key_just_released(ave::KeyCode::W));

        std::cout << "[PASS] Key lifecycle (PressedThisTick -> Held -> ReleasedThisTick -> Released) verified." << std::endl;
    }

    // ========================================================
    // 5. 测试鼠标按钮、位移与滚轮轮询
    // ========================================================
    {
        ave::Input input;

        // 模拟鼠标移动
        ave::MouseMoveEvent m1(100.0, 200.0);
        input.on_event(m1);
        input.new_frame();

        ave::MouseMoveEvent m2(125.0, 240.0);
        input.on_event(m2);

        auto [mx, my] = input.get_mouse_position();
        assert(mx == 125.0 && my == 240.0);

        auto [dx, dy] = input.get_mouse_delta();
        assert(dx == 25.0 && dy == 40.0);

        // 模拟滚轮
        ave::MouseScrollEvent scroll(0.0, 3.5);
        input.on_event(scroll);
        auto [sx, sy] = input.get_mouse_scroll();
        assert(sx == 0.0 && sy == 3.5);

        // 换帧后滚轮清零
        input.new_frame();
        auto [sx2, sy2] = input.get_mouse_scroll();
        assert(sx2 == 0.0 && sy2 == 0.0);

        // 模拟鼠标左键按下与释放
        ave::MouseButtonPressedEvent mb_down(ave::MouseButton::Left, ave::KeyModifier::None);
        input.on_event(mb_down);
        assert(input.is_mouse_button_down(ave::MouseButton::Left));
        assert(input.is_mouse_button_just_pressed(ave::MouseButton::Left));

        input.new_frame();
        assert(input.is_mouse_button_down(ave::MouseButton::Left));
        assert(!input.is_mouse_button_just_pressed(ave::MouseButton::Left));

        ave::MouseButtonReleasedEvent mb_up(ave::MouseButton::Left, ave::KeyModifier::None);
        input.on_event(mb_up);
        assert(input.is_mouse_button_up(ave::MouseButton::Left));
        assert(input.is_mouse_button_just_released(ave::MouseButton::Left));

        std::cout << "[PASS] Mouse position, delta, scroll, and button states verified." << std::endl;
    }

    // ========================================================
    // 6. 测试虚拟 Action 与 Axis 映射
    // ========================================================
    {
        ave::Input input;
        input.map_action("Jump", ave::KeyCode::Space);
        input.map_action("Fire", ave::MouseButton::Left);
        input.map_axis("MoveX", ave::KeyCode::D, ave::KeyCode::A);
        input.map_axis("MoveY", ave::KeyCode::W, ave::KeyCode::S);

        assert(!input.is_action_down("Jump"));
        assert(input.get_axis("MoveX") == 0.0f);
        assert(input.get_axis("MoveY") == 0.0f);

        // 按下 Space 和 D
        ave::KeyPressedEvent jump_press(ave::KeyCode::Space, 0, ave::KeyModifier::None);
        ave::KeyPressedEvent d_press(ave::KeyCode::D, 0, ave::KeyModifier::None);
        input.on_event(jump_press);
        input.on_event(d_press);

        assert(input.is_action_down("Jump"));
        assert(input.is_action_just_pressed("Jump"));
        assert(input.get_axis("MoveX") == 1.0f);

        // 此时再按下 A (同时按 D 和 A)，MoveX 应该抵消为 0
        ave::KeyPressedEvent a_press(ave::KeyCode::A, 0, ave::KeyModifier::None);
        input.on_event(a_press);
        assert(input.get_axis("MoveX") == 0.0f);

        // 松开 D，MoveX 应该变为 -1
        ave::KeyReleasedEvent d_rel(ave::KeyCode::D, 0, ave::KeyModifier::None);
        input.on_event(d_rel);
        assert(input.get_axis("MoveX") == -1.0f);

        std::cout << "[PASS] Virtual Action & Axis mapping verified." << std::endl;
    }

    // ========================================================
    // 7. 测试 Window 与 Input 的绑定联动 (bind_input)
    // ========================================================
    {
        ave::Window window;
        ave::Input input;
        window.bind_input(input);

        ave::KeyPressedEvent key_e(ave::KeyCode::Escape, 0, ave::KeyModifier::None);
        window.dispatch_event(key_e);

        // input 自动收到事件
        assert(input.is_key_down(ave::KeyCode::Escape));
        assert(input.is_key_just_pressed(ave::KeyCode::Escape));

        std::cout << "[PASS] Window bind_input automatic synchronization verified." << std::endl;
    }

    // ========================================================
    // 8. 测试 recreate_swapchain_from_window 安全机制与 ProfileWith surface bypass
    // ========================================================
    {
        ave::Window window;
        ave::Renderer empty_renderer;

        // 使用 alib6::Error 捕获错误报告，验证无效 Renderer 调用时安全报告错误并返回 false
        alib6::Error err;
        alib6::ErrorWrapper ew(err);
        bool res1 = ave::recreate_swapchain_from_window(empty_renderer, window, {}, ew);
        assert(!res1);
        assert(err.has_error());

        // 验证带 RenderBuildReport* 的指针回传重载
        ave::RenderBuildReport report;
        bool res2 = ave::recreate_swapchain_from_window(empty_renderer, window, &report, {}, ew);
        assert(!res2);

        // 验证带有 report 指针的快捷重载
        bool res3 = ave::recreate_swapchain_from_window(empty_renderer, window, &report, ew);
        assert(!res3);

        // 验证 ProfileWith surface bypass 字段存在且可赋空/非空
        ave::ProfileWith pw;
        assert(pw.surface == nullptr);
        pw.surface = empty_renderer.surface;

        std::cout << "[PASS] recreate_swapchain_from_window safety & ProfileWith surface bypass verified." << std::endl;
    }

    std::cout << "\n==============================================" << std::endl;
    std::cout << "  ALL AVE EVENT & INPUT SYSTEM TESTS PASSED!  " << std::endl;
    std::cout << "==============================================" << std::endl;
    return 0;
}
