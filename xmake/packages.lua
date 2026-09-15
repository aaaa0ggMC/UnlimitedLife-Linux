-- Dependency acquisition belongs here; usage/visibility belongs on each target.
if is_plat("mingw", "msys") then
    add_requires("opengl")
    add_requires("pacman::glfw", {alias = "glfw"})
    add_requires("pacman::glew", {alias = "glew"})
    add_requires("pacman::glm", {alias = "glm"})
    add_requires("pacman::stb", {alias = "stb"})
    add_requires("miniaudio")
else
    add_requires("opengl", "glfw", "glew", "glm", "stb", "miniaudio")
end

add_requires("vulkansdk")
add_requires("vulkan-memory-allocator >=3.0.0")

if has_config("examples") then
    add_requires("imgui", {configs = {glfw = true, opengl3 = true}})
end
