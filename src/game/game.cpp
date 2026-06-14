#include "game.h"
#include "../engine/input.h"
#include "../engine/texture.h"

Game::Game(Texture* wizardTex, Texture* treeTex, Texture* houseTex) {
    m_heroIndex = 0;

    m_renderables.push_back({glm::vec2(480, 320), glm::vec2(42, 80), 0.0f, wizardTex});

    m_renderables.push_back({glm::vec2(150, 150), glm::vec2(104, 98), 0.0f, treeTex});
    m_renderables.push_back({glm::vec2(810, 150), glm::vec2(104, 98), 0.0f, treeTex});
    m_renderables.push_back({glm::vec2(150, 500), glm::vec2(104, 98), 0.0f, treeTex});
    m_renderables.push_back({glm::vec2(810, 500), glm::vec2(104, 98), 0.0f, treeTex});

    m_renderables.push_back({glm::vec2(680, 460), glm::vec2(108, 160), 0.0f, houseTex});
}

void Game::update(float deltaTime) {
    glm::vec2 move(0.0f);
    if (Input::isKeyDown(GLFW_KEY_W) || Input::isKeyDown(GLFW_KEY_UP))    move.y += 1.0f;
    if (Input::isKeyDown(GLFW_KEY_S) || Input::isKeyDown(GLFW_KEY_DOWN))  move.y -= 1.0f;
    if (Input::isKeyDown(GLFW_KEY_A) || Input::isKeyDown(GLFW_KEY_LEFT))  move.x -= 1.0f;
    if (Input::isKeyDown(GLFW_KEY_D) || Input::isKeyDown(GLFW_KEY_RIGHT)) move.x += 1.0f;

    if (move.x != 0.0f || move.y != 0.0f)
        m_renderables[m_heroIndex].position += glm::normalize(move) * m_speed * deltaTime;
}
