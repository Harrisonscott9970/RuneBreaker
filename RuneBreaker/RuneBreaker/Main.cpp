#include <SDL3/SDL.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <sstream>

const int WINDOW_W = 800;
const int WINDOW_H = 600;
const int PADDLE_H = 16;
const float PADDLE_SPEED = 600.0f;
const int BALL_SIZE = 10;
const int BRICK_PADDING = 4;
const int BRICK_TOP_OFFSET = 60;
const int BRICK_HEIGHT = 20;
const float POWERUP_SPEED = 150.0f;
const float POWERUP_SIZE = 24.0f;

// Power-up types
enum class PowerUpType { MULTI_BALL, WIDE_PADDLE, SLOW_BALL, EXTRA_LIFE, LASER, STICKY, COUNT };


// Brick with multiple hit points and rune decorations
struct Brick {
    SDL_FRect rect;
    int hits; // current hit points
    int maxHits; // maximum hit points
    SDL_Color color;
    bool alive;
    int runeType; // which rune pattern to display
    float glowPhase; // animation for glowing effect
};

// Visual particle for explosion effects
struct Particle {
    SDL_FRect rect;
    SDL_Color color;
    float lifetime; // particles fade out over time
    float vx, vy;  // velocity for physics
};

// Collectible power-up that falls from destroyed bricks
struct PowerUp {
    SDL_FRect rect;
    PowerUpType type;
    float vy; // falling speed
    SDL_Color color;
};

// Ball object
struct Ball {
    SDL_FRect rect;
    float vx, vy; // velocity vector
    bool active; // inactive balls are removed
};

// Laser projectile 
struct LaserBeam {
    SDL_FRect rect;
    float vy; // negative velocity (shoots upward)
};

// global game state 
std::vector<Particle> particles;
std::vector<PowerUp> powerups;
std::vector<Ball> balls;
std::vector<LaserBeam> lasers;

float shakeX = 0, shakeY = 0;
float shakeIntensity = 0;

// scoring system
int combo = 0;
float comboTimer = 0;

// active power-up states
bool stickyActive = false;
bool laserActive = false;
float laserTimer = 0;
float powerupTimer = 0;

int highScore = 0;

// ui pause
bool paused = false;
float menuAnimTime = 0;


// Load high score from file
void loadHighScore() {
    std::ifstream file("runebreaker_save.txt");
    if (file.is_open()) {
        file >> highScore;
        file.close();
    }
}

// Save high score if current score beats it
void saveHighScore(int score) {
    if (score > highScore) {
        highScore = score;
        std::ofstream file("runebreaker_save.txt");
        if (file.is_open()) {
            file << highScore;
            file.close();
        }
    }
}

// Brick collision detection
bool intersects(const SDL_FRect& a, const SDL_FRect& b) {
    return !(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
}

// Color coding based on brick health
SDL_Color getHitColor(int hits, int maxHits) {
    if (maxHits == 1) return { 120, 80, 200, 255 }; // purple for 1-hit
    if (hits == maxHits) return { 200, 50, 50, 255 }; // red for full health
    if (hits == maxHits - 1) return { 200, 120, 50, 255 }; // orange for damaged
    return { 180, 150, 80, 255 };
}

// ---------- rune system ----------
const uint16_t RUNE_PATTERNS[][12] = {
    // Rune 0: triangle with inner circle
    {0b000001100000, 0b000011110000, 0b000110011000, 0b001100001100,
     0b001100001100, 0b011001110110, 0b011001110110, 0b110011111011,
     0b110011111011, 0b110000000011, 0b111111111111, 0b111111111111},

     // Rune 1: diamond with cross
     {0b000001100000, 0b000011110000, 0b000111111000, 0b001111111100,
      0b011100001110, 0b111000000111, 0b111000000111, 0b011100001110,
      0b001111111100, 0b000111111000, 0b000011110000, 0b000001100000},

      // Rune 2: vertical with wings
      {0b000001100000, 0b000001100000, 0b001101101100, 0b011101101110,
       0b111001100111, 0b000001100000, 0b000001100000, 0b000001100000,
       0b000001100000, 0b011001100110, 0b001101101100, 0b000111111000},

       // Rune 3: star pattern
       {0b000001100000, 0b000111111000, 0b001101101100, 0b011000000110,
        0b111000000111, 0b011001100110, 0b011001100110, 0b111000000111,
        0b011000000110, 0b001101101100, 0b000111111000, 0b000001100000},

        // Rune 4: eye shape
        {0b000111111000, 0b011111111110, 0b111100001111, 0b111001110111,
         0b110011111011, 0b110011111011, 0b110011111011, 0b110011111011,
         0b111001110111, 0b111100001111, 0b011111111110, 0b000111111000}
};

// Render a rune symbol
void drawRune(SDL_Renderer* r, float x, float y, float w, float h, int runeType, SDL_Color color, float glowIntensity = 0) {
    // draw outer glow layers
    if (glowIntensity > 0) {
        for (int i = 3; i >= 0; --i) {
            Uint8 alpha = (Uint8)(glowIntensity * 60 * (i + 1));
            SDL_SetRenderDrawColor(r, color.r, color.g, color.b, alpha);
            SDL_FRect glow = { x - i * 2, y - i * 2, w + i * 4, h + i * 4 };
            SDL_RenderFillRect(r, &glow);
        }
    }

    // draw dark background
    SDL_SetRenderDrawColor(r, color.r / 3, color.g / 3, color.b / 3, 255);
    SDL_FRect bgRect = { x, y, w, h };
    SDL_RenderFillRect(r, &bgRect);

    // select rune pattern and calculate pixel size
    int pattern = runeType % 5;
    float runeSize = std::min(w - 4, h - 2);
    float offsetX = x + (w - runeSize) / 2;
    float offsetY = y + (h - runeSize) / 2;
    float pixelSize = runeSize / 12.0f;

    // render rune
    for (int row = 0; row < 12; ++row) {
        uint16_t line = RUNE_PATTERNS[pattern][row];
        for (int col = 0; col < 12; ++col) {
            if (line & (1 << (11 - col))) {
                // brighten pixels when glowing
                Uint8 r_val = std::min(255, (int)(color.r + glowIntensity * 100));
                Uint8 g_val = std::min(255, (int)(color.g + glowIntensity * 100));
                Uint8 b_val = std::min(255, (int)(color.b + glowIntensity * 100));
                SDL_SetRenderDrawColor(r, r_val, g_val, b_val, 255);

                SDL_FRect pixel = { offsetX + col * pixelSize, offsetY + row * pixelSize, pixelSize + 1, pixelSize + 1 };
                SDL_RenderFillRect(r, &pixel);
            }
        }
    }

    // draw border
    SDL_SetRenderDrawColor(r, color.r / 2, color.g / 2, color.b / 2, 255);
    SDL_RenderRect(r, &bgRect);
}

// level generation

// Create brick layout with increasing difficulty per level
std::vector<Brick> createBricks(int rows, int cols, float windowW, int level) {
    std::vector<Brick> bricks;
    int totalPadding = (cols + 1) * BRICK_PADDING;
    float brickW = (windowW - totalPadding) / (float)cols;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            // create patterns with gaps on higher levels
            bool skip = false;
            if (level == 2 && r % 2 == 1 && c % 2 == 0) skip = true;
            if (level == 3 && (r + c) % 3 == 0) skip = true;
            if (level >= 8 && (r == rows / 2 && c == cols / 2)) skip = true;
            if (skip) continue;

            float x = BRICK_PADDING + c * (brickW + BRICK_PADDING);
            float y = BRICK_TOP_OFFSET + r * (BRICK_HEIGHT + BRICK_PADDING);

            // higher levels spawn tougher bricks
            int maxHits = 1;
            if (level >= 3 && rand() % 4 == 0) maxHits = 2;
            if (level >= 6 && rand() % 6 == 0) maxHits = 3;

            SDL_Color color = getHitColor(maxHits, maxHits);
            int runeType = rand() % 5;
            bricks.push_back({ SDL_FRect{x, y, brickW, (float)BRICK_HEIGHT}, maxHits, maxHits, color, true, runeType, 0 });
        }
    }
    return bricks;
}

// game state enum
enum class GameState { MENU, LEVEL_SELECT, PLAYING, WIN, PAUSED };

// draw window border
void drawBorder(SDL_Renderer* renderer, int w, int h) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderLine(renderer, 0, 0, w, 0);
    SDL_RenderLine(renderer, 0, 0, 0, h);
    SDL_RenderLine(renderer, w - 1, 0, w - 1, h);
    SDL_RenderLine(renderer, 0, h - 1, w, h - 1);
}

// visual effects
float hue = 0.0f; // global hue for rainbow effects

// Convert hue value to RGB color (for rainbow ball effect)
SDL_Color hueToRGB(float h) {
    Uint8 r = (Uint8)((std::sin(h) + 1) * 127);
    Uint8 g = (Uint8)((std::sin(h + 2) + 1) * 127);
    Uint8 b = (Uint8)((std::sin(h + 4) + 1) * 127);
    return { r, g, b, 255 };
}

// Draw paddle with gradient effect 
void drawMagicalPaddle(SDL_Renderer* renderer, SDL_FRect paddle, bool laser) {
    for (int i = 0; i < (int)paddle.w; ++i) {
        float t = i / paddle.w;
        if (laser) {
            SDL_SetRenderDrawColor(renderer, 255, (Uint8)(50 + 205 * t), (Uint8)(50 * (1 - t)), 255);
        }
        else {
            SDL_SetRenderDrawColor(renderer, (Uint8)(255 * (1 - t)), (Uint8)(50 + 205 * t), 255, 255);
        }
        SDL_FRect slice = { paddle.x + i, paddle.y, 1, paddle.h };
        SDL_RenderFillRect(renderer, &slice);
    }
}

// Draw ball with rainbow glow effect
void drawMagicalBall(SDL_Renderer* renderer, SDL_FRect ball, float hue) {
    SDL_Color glowColor = hueToRGB(hue);

    // draw glow layers
    for (int i = 3; i >= 0; --i) {
        Uint8 alpha = (Uint8)(80 * (i + 1));
        SDL_SetRenderDrawColor(renderer, glowColor.r, glowColor.g, glowColor.b, alpha);
        SDL_FRect glow = { ball.x - i, ball.y - i, ball.w + i * 2, ball.h + i * 2 };
        SDL_RenderFillRect(renderer, &glow);
    }

    // draw solid ball center
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &ball);
}

// Trigger screen shake effect (intensity determines strength)
void addScreenShake(float intensity) {
    shakeIntensity = std::max(shakeIntensity, intensity);
}

// Spawn particle trail behind ball
void addBallParticle(SDL_FRect ball) {
    Particle p;
    p.rect = { ball.x + BALL_SIZE / 2 - 1, ball.y + BALL_SIZE / 2 - 1, 2, 2 };
    p.color = SDL_Color{ 255, 255, 255, 200 };
    p.lifetime = 0.2f;
    p.vx = (rand() % 100 - 50) * 0.5f;
    p.vy = (rand() % 100 - 50) * 0.5f;
    particles.push_back(p);
}

// Spawn explosion particles when brick is destroyed
void addBrickParticles(const SDL_FRect& brick, SDL_Color color) {
    int count = 15 + rand() % 10;
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.rect = { brick.x + brick.w / 2, brick.y + brick.h / 2, 3, 3 };
        p.color = color;
        p.lifetime = 0.4f + ((rand() % 100) / 200.0f);
        p.vx = (rand() % 200 - 100) * 2.0f;
        p.vy = (rand() % 200 - 100) * 2.0f;
        particles.push_back(p);
    }
}

// Update and render all active particles
void updateAndDrawParticles(SDL_Renderer* renderer, float dt) {
    for (int i = (int)particles.size() - 1; i >= 0; --i) {
        particles[i].lifetime -= dt;
        particles[i].rect.x += particles[i].vx * dt;
        particles[i].rect.y += particles[i].vy * dt;
        particles[i].vy += 300.0f * dt; // gravity

        if (particles[i].lifetime <= 0) {
            particles.erase(particles.begin() + i);
        }
        else {
            // fade out based on remaining time
            Uint8 alpha = (Uint8)(255 * (particles[i].lifetime / 0.6f));
            SDL_SetRenderDrawColor(renderer, particles[i].color.r, particles[i].color.g,
                particles[i].color.b, alpha);
            SDL_RenderFillRect(renderer, &particles[i].rect);
        }
    }
}

// spawn power-up from destroyed brick
void spawnPowerUp(SDL_FRect brick) {
    if (rand() % 100 < 25) {
        PowerUp p;
        p.rect = { brick.x + brick.w / 2 - POWERUP_SIZE / 2, brick.y, POWERUP_SIZE, POWERUP_SIZE };
        p.type = (PowerUpType)(rand() % (int)PowerUpType::COUNT);
        p.vy = POWERUP_SPEED;

        // assign color based on type
        switch (p.type) {
        case PowerUpType::MULTI_BALL: p.color = { 100, 255, 255, 255 }; break;
        case PowerUpType::WIDE_PADDLE: p.color = { 100, 255, 100, 255 }; break;
        case PowerUpType::SLOW_BALL: p.color = { 255, 255, 100, 255 }; break;
        case PowerUpType::EXTRA_LIFE: p.color = { 255, 100, 100, 255 }; break;
        case PowerUpType::LASER: p.color = { 255, 100, 255, 255 }; break;
        case PowerUpType::STICKY: p.color = { 255, 200, 100, 255 }; break;
        default: p.color = { 200, 200, 200, 255 }; break;
        }
        powerups.push_back(p);
    }
}

// custom bitmap font
namespace ui {
    //font bitmaps for all characters
    static const uint8_t FONT5x7[][7] = {
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x1E,0x11,0x13,0x15,0x19,0x11,0x1E},
        {0x04,0x0C,0x14,0x04,0x04,0x04,0x1F},{0x1E,0x11,0x01,0x06,0x08,0x10,0x1F},
        {0x1E,0x11,0x01,0x06,0x01,0x11,0x1E},{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
        {0x1F,0x10,0x1E,0x01,0x01,0x11,0x1E},{0x0E,0x10,0x1E,0x11,0x11,0x11,0x0E},
        {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
        {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E},{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
        {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},{0x0E,0x11,0x10,0x17,0x11,0x11,0x0E},
        {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},
        {0x1F,0x02,0x02,0x02,0x12,0x12,0x0C},{0x11,0x12,0x14,0x18,0x14,0x12,0x11},
        {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},{0x11,0x1B,0x15,0x11,0x11,0x11,0x11},
        {0x11,0x19,0x15,0x13,0x11,0x11,0x11},{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
        {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
        {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},{0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},{0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
        {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},{0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
        {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},{0x00,0x04,0x00,0x00,0x00,0x04,0x00},
        {0x01,0x01,0x02,0x04,0x08,0x10,0x10},{0x11,0x09,0x02,0x04,0x08,0x12,0x11},
        {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},{0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
        {0x04,0x0A,0x11,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x11,0x0A,0x04}
    };

    // Map character to font index
    static int glyphIndex(char ch) {
        if (ch == ' ') return 0;
        if (ch >= '0' && ch <= '9') return 1 + (ch - '0');
        if (ch >= 'A' && ch <= 'Z') return 11 + (ch - 'A');
        if (ch >= 'a' && ch <= 'z') return 11 + (ch - 'a');
        if (ch == ':') return 37;
        if (ch == '/') return 38;
        if (ch == '%') return 39;
        if (ch == '-') return 40;
        if (ch == '+') return 41;
        if (ch == 'x' || ch == 'X') return 34;
        if (ch == '^') return 42;
        if (ch == 'v') return 43;
        return 0;
    }

    // Draw single character at position
    void drawChar(SDL_Renderer* r, float x, float y, char ch, SDL_Color c, int s = 2) {
        int idx = glyphIndex(ch);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        for (int row = 0; row < 7; ++row) {
            uint8_t line = FONT5x7[idx][row];
            for (int col = 0; col < 5; ++col)
                if (line & (1 << (4 - col))) {
                    SDL_FRect px = { x + col * s, y + row * s, (float)s, (float)s };
                    SDL_RenderFillRect(r, &px);
                }
        }
    }

    // Draw text string 
    void drawText(SDL_Renderer* r, float x, float y, const std::string& t, SDL_Color c, int s = 2) {
        float startX = x;
        for (char ch : t) {
            if (ch == '\n') { y += 8 * s; x = startX; }
            else { drawChar(r, x, y, ch, c, s); x += 6 * s; }
        }
    }

    // Draw text with drop shadow
    void drawTextShadow(SDL_Renderer* r, float x, float y, const std::string& t, SDL_Color mainC, SDL_Color shadowC, int s = 2) {
        drawText(r, x + 2, y + 2, t, shadowC, s);
        drawText(r, x, y, t, mainC, s);
    }
}

// Animated background runes for menu screen
void drawMenuRunes(SDL_Renderer* renderer, int w, int h, float time) {
    for (int i = 0; i < 8; ++i) {
        float x = (i * 150.0f + time * 20.0f);
        while (x > w) x -= w + 50;
        float y = 100 + std::sin(time + i) * 30;
        float glow = (std::sin(time * 2 + i) + 1) * 0.5f;

        SDL_Color color = { 80, 60, 120, 100 };
        drawRune(renderer, x, y, 40, 40, i, color, glow * 0.3f);
    }
}

// main game loop
int main() {
    // Initialize SDL3
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init error: " << SDL_GetError() << "\n";
        return 1;
    }

    loadHighScore();

    // Create window and renderer
    SDL_Window* window = SDL_CreateWindow("Rune Breaker", WINDOW_W, WINDOW_H, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Initialize game state
    GameState state = GameState::MENU;
    int level = 1, unlockedLevel = 1, score = 0, lives = 3;
    const int maxLevels = 10;

    SDL_FRect paddle{ (WINDOW_W - 120) / 2.0f, WINDOW_H - 50.0f, 120, (float)PADDLE_H };
    float paddleTargetW = 120;

    // Initialize first ball
    balls.clear();
    balls.push_back({ SDL_FRect{WINDOW_W / 2.0f - BALL_SIZE / 2.0f, WINDOW_H / 2.0f, (float)BALL_SIZE, (float)BALL_SIZE},
                     380.0f, -380.0f, true });

    bool launched = false;
    Ball* stuckBall = nullptr;
    float stuckBallOffset = 0;
    std::vector<Brick> bricks = createBricks(5, 10, WINDOW_W, level);
    Uint64 prev = SDL_GetPerformanceCounter();
    bool running = true;
    bool mouseClicked = false;

    // main loop
    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((now - prev) / (double)SDL_GetPerformanceFrequency());
        prev = now;
        if (dt > 0.1f) dt = 0.1f;

        // input handling
        SDL_Event e;
        mouseClicked = false;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
                mouseClicked = true;
            else if (e.type == SDL_EVENT_KEY_DOWN) {
                // esc key handling for pause/back
                if (e.key.key == SDLK_ESCAPE) {
                    if (state == GameState::PLAYING) state = GameState::PAUSED;
                    else if (state == GameState::PAUSED) state = GameState::PLAYING;
                    else { state = GameState::MENU; launched = false; score = 0; lives = 3; level = 1; }
                }
                // p key for pause toggle
                if (e.key.key == SDLK_P && state == GameState::PLAYING) {
                    state = GameState::PAUSED;
                }
                else if (e.key.key == SDLK_P && state == GameState::PAUSED) {
                    state = GameState::PLAYING;
                }
            }
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);
        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        // update rainbow hue for ball effect
        hue += dt * 2.0f;
        if (hue > 6.28f) hue -= 6.28f;

        menuAnimTime += dt;

        // update screen shake effect
        if (shakeIntensity > 0) {
            shakeX = (rand() % 100 - 50) / 50.0f * shakeIntensity;
            shakeY = (rand() % 100 - 50) / 50.0f * shakeIntensity;
            shakeIntensity -= dt * 10.0f;
            if (shakeIntensity < 0) shakeIntensity = 0;
        }

        SDL_SetRenderDrawColor(renderer, 10, 10, 20, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderViewport(renderer, nullptr);

        // apply screen shake
        if (shakeIntensity > 0) {
            SDL_Rect vp = { (int)shakeX, (int)shakeY, w, h };
            SDL_SetRenderViewport(renderer, &vp);
        }

        drawBorder(renderer, w, h);

        float mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&mx, &my);

        // menu state
        if (state == GameState::MENU) {
            drawMenuRunes(renderer, w, h, menuAnimTime);

            // animated title with glow
            float titleGlow = (std::sin(menuAnimTime * 2) + 1) * 0.5f;
            for (int i = 4; i > 0; --i) {
                Uint8 alpha = (Uint8)(titleGlow * 40 * i);
                SDL_Color glowColor = { 150, 100, 200, alpha };
                ui::drawText(renderer, w / 2 - 150 - i * 2, 120 - i * 2, "Rune Breaker", glowColor, 4);
            }
            ui::drawTextShadow(renderer, w / 2 - 150, 120, "Rune Breaker", { 255, 220, 255, 255 }, { 80, 40, 100, 255 }, 4);

            // menu button hover effects
            SDL_Color playColor = (my > 250 && my < 300) ? SDL_Color{ 255, 255, 150, 255 } : SDL_Color{ 200, 200, 255, 255 };
            SDL_Color selectColor = (my > 310 && my < 360) ? SDL_Color{ 255, 255, 150, 255 } : SDL_Color{ 200, 200, 255, 255 };

            ui::drawText(renderer, w / 2 - 120, 250, "Click To Play", playColor, 3);
            ui::drawText(renderer, w / 2 - 140, 310, "Level Select", selectColor, 3);

            // decorative runes and high score
            drawRune(renderer, w / 2 - 180, 390, 30, 30, 0, { 150, 100, 200, 255 }, 0.3f);
            drawRune(renderer, w / 2 + 150, 390, 30, 30, 1, { 150, 100, 200, 255 }, 0.3f);
            ui::drawText(renderer, w / 2 - 120, 400, "Highscore " + std::to_string(highScore), { 255, 220, 100, 255 }, 2);

            // menu button click detection
            if (mouseClicked) {
                if (my > 310 && my < 360) state = GameState::LEVEL_SELECT;
                else if (my > 250 && my < 300) {
                    // start new game
                    state = GameState::PLAYING;
                    bricks = createBricks(5, 10, (float)w, level);
                    launched = false;
                    score = 0;
                    lives = 3;
                    balls.clear();
                    balls.push_back({ SDL_FRect{WINDOW_W / 2.0f - BALL_SIZE / 2.0f, WINDOW_H / 2.0f, (float)BALL_SIZE, (float)BALL_SIZE},
                                     380.0f, -380.0f, true });
                    powerups.clear();
                    particles.clear();
                    combo = 0;
                    paddleTargetW = 120;
                    paddle.w = 120;
                    stickyActive = false;
                    laserActive = false;
                }
            }
        }
        // level select state
        else if (state == GameState::LEVEL_SELECT) {
            ui::drawTextShadow(renderer, w / 2 - 120, 80, "SELECT LEVEL", { 255,255,255,255 }, { 50,50,50,255 }, 3);

            // display all levels (locked levels are grayed out)
            for (int i = 1; i <= maxLevels; ++i) {
                SDL_Color col = (i <= unlockedLevel) ? SDL_Color{ 200,200,255,255 } : SDL_Color{ 80,80,80,255 };
                ui::drawText(renderer, w / 2 - 60, 130 + i * 35, "LEVEL " + std::to_string(i), col, 2);

                // click to start level
                if (mouseClicked && i <= unlockedLevel && my > 130 + i * 35 - 5 && my < 130 + i * 35 + 20) {
                    level = i;
                    state = GameState::PLAYING;
                    bricks = createBricks(5 + level / 2, 10, (float)w, level);
                    launched = false;
                    score = 0;
                    lives = 3;
                    balls.clear();
                    balls.push_back({ SDL_FRect{WINDOW_W / 2.0f - BALL_SIZE / 2.0f, WINDOW_H / 2.0f, (float)BALL_SIZE, (float)BALL_SIZE},
                                     380.0f, -380.0f, true });
                    powerups.clear();
                    particles.clear();
                    combo = 0;
                    paddleTargetW = 120;
                    paddle.w = 120;
                    stickyActive = false;
                    laserActive = false;
                }
            }
            ui::drawText(renderer, 20, h - 40, "ESC - BACK", { 150, 150, 150, 255 }, 2);
        }
        // paused state
        else if (state == GameState::PAUSED) {
            ui::drawTextShadow(renderer, w / 2 - 80, h / 2 - 40, "PAUSED", { 255, 255, 255, 255 }, { 80, 80, 80, 255 }, 4);
            ui::drawText(renderer, w / 2 - 120, h / 2 + 20, "P or ESC to resume", { 200, 200, 255, 255 }, 2);
        }
        // playing state
        else if (state == GameState::PLAYING) {
            // animate multi-hit brick glow
            for (auto& b : bricks) {
                if (b.alive && b.maxHits > 1) {
                    b.glowPhase += dt * 3.0f;
                }
            }

            // update timers
            if (comboTimer > 0) comboTimer -= dt;
            if (comboTimer <= 0) combo = 0;
            if (laserTimer > 0) laserTimer -= dt;
            if (laserTimer <= 0) laserActive = false;
            if (powerupTimer > 0) powerupTimer -= dt;

            // paddle movement
            if (keys[SDL_SCANCODE_LEFT]) paddle.x -= PADDLE_SPEED * dt;
            if (keys[SDL_SCANCODE_RIGHT]) paddle.x += PADDLE_SPEED * dt;
            paddle.x = std::clamp(paddle.x, 0.0f, (float)w - paddle.w);

            // smooth paddle width transitions
            if (paddle.w < paddleTargetW) paddle.w = std::min(paddle.w + 200.0f * dt, paddleTargetW);
            if (paddle.w > paddleTargetW) paddle.w = std::max(paddle.w - 200.0f * dt, paddleTargetW);

            // f key to skip level (for testing/debugging)
            if (keys[SDL_SCANCODE_F]) {
                if (level < maxLevels) {
                    level++;
                    unlockedLevel = std::max(unlockedLevel, level);
                    bricks = createBricks(5 + level / 2, 10, (float)w, level);
                    launched = false;
                    balls.clear();
                    balls.push_back({ SDL_FRect{WINDOW_W / 2.0f - BALL_SIZE / 2.0f, WINDOW_H / 2.0f, (float)BALL_SIZE, (float)BALL_SIZE},
                                     380.0f, -380.0f, true });
                    powerups.clear();
                    combo = 0;
                    paddleTargetW = 120;
                    stickyActive = false;
                    laserActive = false;
                    stuckBall = nullptr;
                }
                else {
                    saveHighScore(score);
                    state = GameState::WIN;
                }
            }

            // laser firing
            if (laserActive && keys[SDL_SCANCODE_SPACE] && lasers.size() < 3) {
                LaserBeam laser;
                laser.rect = { paddle.x + paddle.w / 2 - 2, paddle.y - 10, 4, 15 };
                laser.vy = -600.0f;
                lasers.push_back(laser);
            }

            drawMagicalPaddle(renderer, paddle, laserActive);

            // ball launch logic
            if (!launched && balls.size() > 0) {
                // attach ball to paddle before launch
                balls[0].rect.x = paddle.x + paddle.w / 2 - BALL_SIZE / 2;
                balls[0].rect.y = paddle.y - BALL_SIZE - 2;
                if (keys[SDL_SCANCODE_SPACE]) {
                    launched = true;
                    stuckBall = nullptr;
                }
            }
            else {
                // ball physics
                for (auto& ball : balls) {
                    if (!ball.active) continue;

                    // handle sticky paddle mechanic
                    if (stickyActive && stuckBall == &ball) {
                        ball.rect.x = paddle.x + stuckBallOffset - BALL_SIZE / 2;
                        ball.rect.y = paddle.y - BALL_SIZE - 2;
                        if (keys[SDL_SCANCODE_SPACE]) {
                            stuckBall = nullptr;
                            ball.vy = -std::abs(ball.vy);
                        }
                        continue;
                    }

                    // update ball position
                    ball.rect.x += ball.vx * dt;
                    ball.rect.y += ball.vy * dt;

                    // spawn particle trail
                    if (rand() % 3 == 0) addBallParticle(ball.rect);

                    // wall collisions
                    if (ball.rect.x <= 0 || ball.rect.x + BALL_SIZE >= w) {
                        ball.vx *= -1;
                        ball.rect.x = std::clamp(ball.rect.x, 0.0f, (float)w - BALL_SIZE);
                    }
                    if (ball.rect.y <= 0) {
                        ball.vy *= -1;
                        ball.rect.y = 0;
                    }

                    // ball falls off screen
                    if (ball.rect.y > h) {
                        ball.active = false;
                    }
                }

                // remove dead balls
                balls.erase(std::remove_if(balls.begin(), balls.end(),
                    [](const Ball& b) { return !b.active; }), balls.end());

                // lose life
                if (balls.empty()) {
                    lives--;
                    addScreenShake(8.0f);
                    if (lives <= 0) {
                        saveHighScore(score);
                        state = GameState::MENU;
                    }
                    else {
                        // reset ball on paddle
                        launched = false;
                        balls.push_back({ SDL_FRect{WINDOW_W / 2.0f - BALL_SIZE / 2.0f, WINDOW_H / 2.0f, (float)BALL_SIZE, (float)BALL_SIZE},
                                         380.0f, -380.0f, true });
                        paddleTargetW = 120;
                        stickyActive = false;
                        stuckBall = nullptr;
                    }
                }
            }

            // paddle collision
            for (auto& ball : balls) {
                if (!ball.active) continue;
                if (intersects(ball.rect, paddle) && ball.vy > 0) {
                    if (stickyActive && !stuckBall) {
                        // stick ball to paddle
                        stuckBall = &ball;
                        stuckBallOffset = ball.rect.x + BALL_SIZE / 2 - paddle.x;
                    }
                    else {
                        // bounce with angle based on hit position
                        float hitPos = (ball.rect.x + BALL_SIZE / 2 - paddle.x) / paddle.w - 0.5f;
                        ball.vx = hitPos * 700.0f;
                        ball.vy = -std::abs(ball.vy);
                        ball.rect.y = paddle.y - BALL_SIZE;
                    }
                }
            }

            // brick collisions
            for (auto& ball : balls) {
                if (!ball.active) continue;
                for (auto& b : bricks) {
                    if (b.alive && intersects(ball.rect, b.rect)) {
                        b.hits--;
                        if (b.hits <= 0) {
                            b.alive = false;
                            spawnPowerUp(b.rect);
                            addBrickParticles(b.rect, b.color);
                            addScreenShake(3.0f);
                        }
                        else {
                            b.color = getHitColor(b.hits, b.maxHits);
                        }

                        ball.vy *= -1;
                        combo++;
                        comboTimer = 2.0f;

                        // combo multiplier for scoring
                        int points = 10 * std::max(1, combo / 3);
                        score += points;
                        break;
                    }
                }
            }

            // laser collisions
            for (int i = (int)lasers.size() - 1; i >= 0; --i) {
                lasers[i].rect.y += lasers[i].vy * dt;

                // remove off-screen lasers
                if (lasers[i].rect.y < 0) {
                    lasers.erase(lasers.begin() + i);
                    continue;
                }

                // check laser-brick collisions
                for (auto& b : bricks) {
                    if (b.alive && intersects(lasers[i].rect, b.rect)) {
                        b.hits--;
                        if (b.hits <= 0) {
                            b.alive = false;
                            spawnPowerUp(b.rect);
                            addBrickParticles(b.rect, b.color);
                            addScreenShake(2.0f);
                        }
                        else {
                            b.color = getHitColor(b.hits, b.maxHits);
                        }
                        score += 10;
                        lasers.erase(lasers.begin() + i);
                        break;
                    }
                }
            }

            // powerup collection
            for (int i = (int)powerups.size() - 1; i >= 0; --i) {
                powerups[i].rect.y += powerups[i].vy * dt;

                // remove off-screen powerups
                if (powerups[i].rect.y > h) {
                    powerups.erase(powerups.begin() + i);
                    continue;
                }

                // collect powerup
                if (intersects(powerups[i].rect, paddle)) {
                    PowerUpType type = powerups[i].type;
                    powerupTimer = 10.0f;

                    // apply powerup effect
                    switch (type) {
                    case PowerUpType::MULTI_BALL:
                        if (balls.size() > 0) {
                            Ball b1 = balls[0];
                            b1.vx = balls[0].vx + 150;
                            Ball b2 = balls[0];
                            b2.vx = balls[0].vx - 150;
                            balls.push_back(b1);
                            balls.push_back(b2);
                        }
                        break;
                    case PowerUpType::WIDE_PADDLE:
                        paddleTargetW = 180;
                        break;
                    case PowerUpType::SLOW_BALL:
                        for (auto& b : balls) {
                            b.vx *= 0.7f;
                            b.vy *= 0.7f;
                        }
                        break;
                    case PowerUpType::EXTRA_LIFE:
                        lives++;
                        break;
                    case PowerUpType::LASER:
                        laserActive = true;
                        laserTimer = 8.0f;
                        break;
                    case PowerUpType::STICKY:
                        stickyActive = true;
                        break;
                    default: break;
                    }

                    powerups.erase(powerups.begin() + i);
                }
            }

            // render game objects

            // draw balls
            for (auto& ball : balls) {
                if (ball.active) drawMagicalBall(renderer, ball.rect, hue);
            }

            // draw bricks with runes
            for (auto& b : bricks) {
                if (b.alive) {
                    float glowIntensity = 0;
                    if (b.maxHits > 1) {
                        glowIntensity = (std::sin(b.glowPhase) + 1) * 0.5f;
                    }
                    drawRune(renderer, b.rect.x, b.rect.y, b.rect.w, b.rect.h, b.runeType, b.color, glowIntensity);
                }
            }

            // draw powerups with icons
            for (auto& p : powerups) {
                SDL_SetRenderDrawColor(renderer, p.color.r, p.color.g, p.color.b, 255);
                SDL_RenderFillRect(renderer, &p.rect);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                float cx = p.rect.x + p.rect.w / 2;
                float cy = p.rect.y + p.rect.h / 2;

                // draw icon based on powerup type
                if (p.type == PowerUpType::MULTI_BALL) {
                    SDL_FRect dot1 = { cx - 6, cy - 3, 4, 4 };
                    SDL_FRect dot2 = { cx + 2, cy - 3, 4, 4 };
                    SDL_RenderFillRect(renderer, &dot1);
                    SDL_RenderFillRect(renderer, &dot2);
                }
                else if (p.type == PowerUpType::WIDE_PADDLE) {
                    SDL_FRect bar = { cx - 8, cy, 16, 3 };
                    SDL_RenderFillRect(renderer, &bar);
                }
                else if (p.type == PowerUpType::EXTRA_LIFE) {
                    ui::drawChar(renderer, cx - 4, cy - 6, '+', { 255, 255, 255, 255 }, 2);
                }
                else if (p.type == PowerUpType::LASER) {
                    SDL_FRect beam1 = { cx - 2, cy - 8, 2, 8 };
                    SDL_FRect beam2 = { cx + 2, cy - 8, 2, 8 };
                    SDL_RenderFillRect(renderer, &beam1);
                    SDL_RenderFillRect(renderer, &beam2);
                }
            }

            // draw lasers
            for (auto& laser : lasers) {
                SDL_SetRenderDrawColor(renderer, 255, 100, 255, 255);
                SDL_RenderFillRect(renderer, &laser.rect);
            }

            updateAndDrawParticles(renderer, dt);

            // hud
            ui::drawText(renderer, 20, 20, "SCORE " + std::to_string(score), { 255,255,255,255 }, 2);
            ui::drawText(renderer, w - 120, 20, "LIVES " + std::to_string(lives), { 255,200,200,255 }, 2);
            ui::drawText(renderer, w / 2 - 50, 20, "LV " + std::to_string(level), { 200,255,200,255 }, 2);

            // show combo multiplier
            if (combo >= 3) {
                std::string comboText = "x" + std::to_string(combo / 3 + 1) + " COMBO";
                ui::drawTextShadow(renderer, w / 2 - 60, 50, comboText, { 255, 255, 100, 255 }, { 100, 100, 50, 255 }, 2);
            }

            // tutorial text on first level
            if (level == 1 && !launched) {
                ui::drawText(renderer, w / 2 - 100, h - 100, "SPACE - Launch", { 255,255,255,255 }, 2);
                ui::drawText(renderer, w / 2 - 100, h - 70, "LEFT/RIGHT - Move", { 255,255,255,255 }, 2);
            }

            ui::drawText(renderer, 20, h - 30, "P - PAUSE", { 150, 150, 150, 255 }, 1);

            // level complete
            if (std::none_of(bricks.begin(), bricks.end(), [](const Brick& b) {return b.alive; })) {
                if (level < maxLevels) {
                    level++;
                    unlockedLevel = std::max(unlockedLevel, level);
                    bricks = createBricks(5 + level / 2, 10, (float)w, level);
                    launched = false;
                    balls.clear();
                    balls.push_back({ SDL_FRect{WINDOW_W / 2.0f - BALL_SIZE / 2.0f, WINDOW_H / 2.0f, (float)BALL_SIZE, (float)BALL_SIZE},
                                     380.0f, -380.0f, true });
                    powerups.clear();
                    combo = 0;
                    paddleTargetW = 120;
                    stickyActive = false;
                    laserActive = false;
                    stuckBall = nullptr;
                }
                else {
                    saveHighScore(score);
                    state = GameState::WIN;
                }
            }
        }
        // win state
        else if (state == GameState::WIN) {
            ui::drawTextShadow(renderer, w / 2 - 120, 180, "YOU WIN!", { 255,255,255,255 }, { 80,80,80,255 }, 5);
            ui::drawText(renderer, w / 2 - 100, 280, "FINAL SCORE", { 200,255,200,255 }, 3);
            ui::drawText(renderer, w / 2 - 80, 320, std::to_string(score), { 255,255,100,255 }, 4);

            if (score >= highScore) {
                ui::drawText(renderer, w / 2 - 100, 380, "NEW HIGH SCORE!", { 255,100,100,255 }, 2);
            }

            ui::drawText(renderer, w / 2 - 160, 450, "CLICK TO RETURN", { 200,200,255,255 }, 2);

            // return to menu
            if (mouseClicked) {
                state = GameState::MENU;
                lives = 3;
                score = 0;
                launched = false;
                level = 1;
                powerups.clear();
                particles.clear();
                balls.clear();
                balls.push_back({ SDL_FRect{WINDOW_W / 2.0f - BALL_SIZE / 2.0f, WINDOW_H / 2.0f, (float)BALL_SIZE, (float)BALL_SIZE},
                                 380.0f, -380.0f, true });
                combo = 0;
                paddleTargetW = 120;
                paddle.w = 120;
                stickyActive = false;
                laserActive = false;
            }
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
