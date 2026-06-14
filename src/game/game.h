#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

class Texture;

struct SpriteRenderData {
    glm::vec2 position;
    glm::vec2 scale;
    float rotation;
    Texture* texture;
};

class Game {
public:
    Game(Texture* wizardTex, Texture* treeTex, Texture* houseTex);
    ~Game() = default;

    void update(float deltaTime);
    const std::vector<SpriteRenderData>& renderables() const { return m_renderables; }

private:
    std::vector<SpriteRenderData> m_renderables;
    int m_heroIndex;
    float m_speed = 250.0f;
};
