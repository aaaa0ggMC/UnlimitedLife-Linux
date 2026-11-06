/**
 * @file app_control.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 桌面端外部控制通道（FOG_CONTROL）：UNIX 域流套接字 + 监听线程
 * @version 1.0
 * @date 2026-10-28
 *
 * @copyright Copyright (c) 2026
 * @parblock 动机
 * Wayland 下没有 xdotool/wmctrl 这类关窗口的手段，而 FPS Detective 报告只在干净退出时
 * 才打印。于是让 fog 自己起一个线程监听套接字，外部脚本发 quit 即可走与 ESC 相同的退出
 * 路径；顺带支持 shot <path> 在安全点截帧（复用 mascot 的交换链读回）。
 * @endparblock
 */
#include "app.h"

#ifndef __ANDROID__
#include <array>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <algorithm>
#include <filesystem>
#endif

using enum alib5::Severity;

void ControlChannel::start() {
#ifndef __ANDROID__
    const char* env = std::getenv("FOG_CONTROL");
    if(!env || !*env) return;

    if(std::string_view(env) == "1") {
        socket_path = std::format("/tmp/fog-control-{}.sock", ::getpid());
    } else {
        socket_path = env;
    }

    // 上一次异常退出可能留下套接字文件，bind 前先清掉
    std::error_code ec;
    if(std::filesystem::exists(socket_path, ec)) {
        std::filesystem::remove(socket_path, ec);
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) {
        socket_path.clear();
        return;
    }

    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, socket_path.c_str(), sizeof(address.sun_path) - 1);
    if(::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
       ::listen(fd, 4) != 0) {
        ::close(fd);
        socket_path.clear();
        return;
    }

    // 非阻塞 + poll：监听线程才能及时响应 stop_requested
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if(flags >= 0) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    listener = std::jthread([this, fd](std::stop_token token) { serve(fd, token); });
#endif
}

void ControlChannel::stop() {
#ifndef __ANDROID__
    if(listener.joinable()) {
        listener.request_stop();
        listener.join();
    }
    if(!socket_path.empty()) {
        std::error_code ec;
        std::filesystem::remove(socket_path, ec);
        socket_path.clear();
    }
#endif
}

auto ControlChannel::consume_shot(std::string& path) -> bool {
#ifndef __ANDROID__
    std::lock_guard<std::mutex> lock(request_mutex);
    if(!shot_pending) return false;
    path = std::move(pending_shot);
    pending_shot.clear();
    shot_pending = false;
    return true;
#else
    (void)path;
    return false;
#endif
}

auto ControlChannel::consume_ui(bool& visible) -> bool {
#ifndef __ANDROID__
    std::lock_guard<std::mutex> lock(request_mutex);
    if(!pending_ui) return false;
    visible = *pending_ui;
    pending_ui.reset();
    return true;
#else
    (void)visible;
    return false;
#endif
}

auto ControlChannel::consume_camera(std::string& spec) -> bool {
#ifndef __ANDROID__
    std::lock_guard<std::mutex> lock(request_mutex);
    if(!pending_camera) return false;
    spec = std::move(*pending_camera);
    pending_camera.reset();
    return true;
#else
    (void)spec;
    return false;
#endif
}

auto ControlChannel::consume_preset(int& preset) -> bool {
#ifndef __ANDROID__
    std::lock_guard<std::mutex> lock(request_mutex);
    if(!pending_preset) return false;
    preset = *pending_preset;
    pending_preset.reset();
    return true;
#else
    (void)preset;
    return false;
#endif
}

#ifndef __ANDROID__
void App::apply_control_camera(std::string_view spec) {
    // 形如 "x y z [yaw [pitch]]"，角度单位为度；缺省 yaw/pitch 时只搬位置不改朝向
    std::array<float, 5> values {};
    std::size_t count = 0;
    std::size_t offset = 0;
    while(count < values.size()) {
        const auto first = spec.find_first_not_of(' ', offset);
        if(first == std::string_view::npos) break;
        const auto [ptr, ec] = std::from_chars(spec.data() + first, spec.data() + spec.size(),
                                               values[count]);
        if(ec != std::errc{}) break;
        offset = static_cast<std::size_t>(ptr - spec.data());
        ++count;
    }
    if(count < 3) {
        lg(Error) << "control camera: 需要至少 x y z，收到: " << spec << std::endl;
        return;
    }

    const glm::vec3 position(values[0], values[1], values[2]);
    if(count >= 5) {
        // 视角状态也要一起改，否则下一帧 update_camera 会用 tilt 把朝向同步回去
        camera.tilt().yaw_deg = values[3];
        camera.tilt().pitch_deg = std::clamp(values[4], -89.0f, 89.0f);
        camera.transform().set_yaw_pitch(camera.tilt().yaw_deg, camera.tilt().pitch_deg);
    }
    camera.transform().position = position;
    camera.transform().dm_mark();
    lg(Info) << "control camera: pos(" << position.x << ", " << position.y << ", " << position.z
             << ") yaw " << camera.tilt().yaw_deg << " pitch " << camera.tilt().pitch_deg
             << std::endl;
}
#endif

#ifndef __ANDROID__
void ControlChannel::serve(int listen_fd, std::stop_token token) {
    while(!token.stop_requested()) {
        pollfd descriptor { listen_fd, POLLIN, 0 };
        const int ready = ::poll(&descriptor, 1, 200);
        if(ready <= 0) continue;  // 超时或被信号打断：回去检查 stop

        // 一次只处理一个连接：命令短、回复短，脚本也是发一条收一条再断开
        const int client = ::accept4(listen_fd, nullptr, nullptr, SOCK_CLOEXEC);
        if(client < 0) continue;
        handle_client(client);
        ::close(client);
    }
    ::close(listen_fd);
}

void ControlChannel::handle_client(int client) {
    std::string line;
    char buffer[512];
    while(line.find('\n') == std::string::npos && line.size() < sizeof(buffer)) {
        const ssize_t received = ::recv(client, buffer, sizeof(buffer), 0);
        if(received <= 0) break;
        line.append(buffer, static_cast<size_t>(received));
    }
    while(!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }

    std::string reply;
    if(line == "ping") {
        reply = "pong\n";
    } else if(line == "quit") {
        reply = "bye\n";
        quit_requested.store(true, std::memory_order_relaxed);
    } else if(line.rfind("shot", 0) == 0) {
        std::string_view path = std::string_view(line).substr(4);
        const auto first = path.find_first_not_of(' ');
        if(first == std::string_view::npos) {
            reply = "error: shot requires a path\n";
        } else {
            std::lock_guard<std::mutex> lock(request_mutex);
            pending_shot = path.substr(first);
            shot_pending = true;
            reply = "ok\n";
        }
    } else if(line.rfind("ui", 0) == 0) {
        std::string_view mode = std::string_view(line).substr(2);
        const auto first = mode.find_first_not_of(' ');
        mode = first == std::string_view::npos ? std::string_view{} : mode.substr(first);
        if(mode == "on" || mode == "off") {
            std::lock_guard<std::mutex> lock(request_mutex);
            pending_ui = (mode == "on");
            reply = "ok\n";
        } else {
            reply = "error: ui expects on|off\n";
        }
    } else if(line.rfind("cam", 0) == 0) {
        std::string_view spec = std::string_view(line).substr(3);
        const auto first = spec.find_first_not_of(' ');
        spec = first == std::string_view::npos ? std::string_view{} : spec.substr(first);
        if(spec.empty()) {
            reply = "error: cam expects 'x y z [yaw [pitch]]'\n";
        } else {
            std::lock_guard<std::mutex> lock(request_mutex);
            pending_camera = spec;
            reply = "ok\n";
        }
    } else if(line.rfind("preset", 0) == 0) {
        std::string_view value = std::string_view(line).substr(6);
        const auto first = value.find_first_not_of(' ');
        value = first == std::string_view::npos ? std::string_view{} : value.substr(first);
        int index = -1;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), index);
        if(ec != std::errc{} || ptr != value.data() + value.size()
           || index < 0 || index >= scene_preset_count) {
            reply = "error: preset expects 0.." + std::to_string(scene_preset_count - 1) + "\n";
        } else {
            std::lock_guard<std::mutex> lock(request_mutex);
            pending_preset = index;
            reply = "ok\n";
        }
    } else {
        reply = "error: unknown command\n";
    }

    size_t sent = 0;
    while(sent < reply.size()) {
        const ssize_t written = ::send(client, reply.data() + sent, reply.size() - sent, MSG_NOSIGNAL);
        if(written <= 0) break;
        sent += static_cast<size_t>(written);
    }
    ::shutdown(client, SHUT_WR);
}
#endif
