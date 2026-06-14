#include "engine/vulkan_core.h"
#include "engine/renderer.h"
#include "engine/texture.h"
#include "engine/input.h"
#include "game/game.h"
#include "embedded_shaders.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <memory>
#include <iostream>
#include <unistd.h>

static std::string exeDir() {
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if (len != -1) {
        buf[len] = '\0';
        std::string s(buf);
        auto pos = s.rfind('/');
        if (pos != std::string::npos)
            return s.substr(0, pos);
    }
    return ".";
}

static std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open: " + path);
    size_t size = (size_t)file.tellg();
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), (long)size);
    return buf;
}

static std::string findAsset(const std::string& name) {
    std::string dir = exeDir();
    const char* rels[] = {"assets/", "../assets/", "./assets/"};
    for (auto r : rels) {
        std::string full = dir + "/" + r + name;
        std::ifstream f(full);
        if (f.good()) return full;
    }
    return dir + "/assets/" + name;
}

int main() {
    try {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    int width = 960, height = 640;
    auto window = glfwCreateWindow(width, height, "Top Down", nullptr, nullptr);
    if (!window) { std::cerr << "failed to create window\n"; return 1; }

    Input::init(window);

    VulkanCore core(window);
    glfwSetFramebufferSizeCallback(window, VulkanCore::framebufferResizedCallback);

    std::vector<char> vertCode(sprite_vert_spv, sprite_vert_spv + sprite_vert_spv_size);
    std::vector<char> fragCode(sprite_frag_spv, sprite_frag_spv + sprite_frag_spv_size);
    Renderer renderer(&core, vertCode, fragCode);

    Texture wizardTex(&core, findAsset("wizard_player.png"),
                      renderer.textureSetLayout(), renderer.descriptorPool());
    Texture treeTex(&core, findAsset("tree.png"),
                    renderer.textureSetLayout(), renderer.descriptorPool());
    Texture houseTex(&core, findAsset("medieval_house.png"),
                     renderer.textureSetLayout(), renderer.descriptorPool());

    Game game(&wizardTex, &treeTex, &houseTex);

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window)) {
        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        if (deltaTime > 0.1f) deltaTime = 0.016f;
        lastTime = now;

        glfwPollEvents();

        game.update(deltaTime);

        auto cmd = core.beginFrame();
        if (cmd) {
            renderer.beginFrame(cmd);

            for (auto& spr : game.renderables())
                renderer.drawSprite(cmd, spr.position, spr.scale, spr.rotation, spr.texture);

            renderer.endFrame();
            core.endFrame();
        }
    }

    core.waitIdle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
    } catch (std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }
}
