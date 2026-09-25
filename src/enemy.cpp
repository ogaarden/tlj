#include "enemy.hpp"

// --- Baseklasse ---
void Enemy::draw() const {

    float enemyRadius = 15.0f; // Fast radius for kroppen (eller bruk orbRadius)
    DrawCircleV(position, enemyRadius, orbColor);
    DrawCircleLines(position.x, position.y, enemyRadius, BLACK); // Svart omriss for bedre synlighe

    if (hp < maxHp) {
        float barWidth = 30.0f;
        float barHeight = 4.0f;
        Vector2 barPos = { 
            position.x + (texture.width / 2.0f) - (barWidth / 2.0f), 
            position.y - 8.0f 
        };

        float hpPercent = (float)hp / (float)maxHp;
        DrawRectangleV(barPos, { barWidth, barHeight }, RED);
        DrawRectangleV(barPos, { barWidth * hpPercent, barHeight }, GREEN);
    }
}

void Enemy::takeDamage(int amount) { hp -= amount; }
bool Enemy::isDead() const { return hp <= 0; }

// --- Footman ---
Footman::Footman(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 140.0f;
    hp = 200;
    maxHp = 200;
    damage = 10;
    xpValue = 15;
    orbColor = BLUE;
    orbRadius = 5.0f;
    texture = tex;
}

void Footman::update(Vector2 playerPosition) {
    Vector2 dir = Vector2Normalize(Vector2Subtract(playerPosition, position));
    position.x += dir.x * speed * GetFrameTime();
    position.y += dir.y * speed * GetFrameTime();
}

// --- Goon ---
Goon::Goon(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 70.0f;
    hp = 80;
    maxHp = 80;
    damage = 25;
    xpValue = 40;
    orbColor = GREEN;
    orbRadius = 10.0f;
    texture = tex;
}

void Goon::update(Vector2 playerPosition) {
    Vector2 dir = Vector2Normalize(Vector2Subtract(playerPosition, position));
    position.x += dir.x * speed * GetFrameTime();
    position.y += dir.y * speed * GetFrameTime();
}

// --- Lackey ---
Lackey::Lackey(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 180.0f;
    hp = 10;
    maxHp = 10;
    damage = 5;
    xpValue = 8;
    orbColor = YELLOW;
    orbRadius = 4.0f;
    texture = tex;
}

void Lackey::update(Vector2 playerPosition) {
    waveTimer += GetFrameTime() * 6.0f;
    Vector2 dir = Vector2Normalize(Vector2Subtract(playerPosition, position));
    Vector2 perp = { -dir.y, dir.x };
    float offset = std::sin(waveTimer) * 40.0f;

    position.x += (dir.x * speed + perp.x * offset) * GetFrameTime();
    position.y += (dir.y * speed + perp.y * offset) * GetFrameTime();
}