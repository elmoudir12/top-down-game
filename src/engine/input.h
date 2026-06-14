#pragma once

#include <GLFW/glfw3.h>

class Input {
public:
    static void init(GLFWwindow* window) { s_window = window; }
    static bool isKeyDown(int key) { return glfwGetKey(s_window, key) == GLFW_PRESS; }

private:
    static inline GLFWwindow* s_window = nullptr;
};
