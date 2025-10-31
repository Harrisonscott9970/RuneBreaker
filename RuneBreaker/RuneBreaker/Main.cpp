#include <SDL3/SDL.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <string>

const int WINDOW_W = 800;
const int WINDOW_H = 600;
const int PADDLE_H = 16;
const float PADDLE_SPEED = 600.0f;
const int BALL_SIZE = 10;
const int BRICK_PADDING = 4;
const int BRICK_TOP_OFFSET = 60;
const int BRICK_HEIGHT = 20;

struct Brick {
    SDL_FRect rect;
    bool alive;
    SDL_Color color;
};

bool intersects(const SDL_FRect& a, const SDL_FRect& b) {
    return !(a.x + a.w <= b.x || b.x + b.w <= a.x ||
        a.y + a.h <= b.y || b.y + b.h <= a.y);
}

std::vector<Brick> createBricks(int rows, int cols, float windowW) {
    std::vector<Brick> bricks;
    int totalPadding = (cols + 1) * BRICK_PADDING;
    float brickW = (windowW - totalPadding) / (float)cols;

    SDL_Color rowColors[] = {
        {255, 50, 50, 255},
        {255, 150, 50, 255},
        {255, 255, 50, 255},
        {50, 255, 100, 255},
        {50, 150, 255, 255},
        {180, 50, 255, 255},
        {255, 100, 200, 255}
    };

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = BRICK_PADDING + c * (brickW + BRICK_PADDING);
            float y = BRICK_TOP_OFFSET + r * (BRICK_HEIGHT + BRICK_PADDING);
            SDL_Color color = rowColors[r % 7];
            bricks.push_back({ SDL_FRect{x, y, brickW, (float)BRICK_HEIGHT}, true, color });
        }
    }
    return bricks;
}

enum class GameState { MENU, LEVEL_SELECT, PLAYING, GAME_OVER };

void drawButton(SDL_Renderer* renderer, SDL_FRect rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &rect);
}

void drawBorder(SDL_Renderer* renderer, int w, int h) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderLine(renderer, 0, 0, w, 0);
    SDL_RenderLine(renderer, 0, 0, 0, h);
    SDL_RenderLine(renderer, w - 1, 0, w - 1, h);
    SDL_RenderLine(renderer, 0, h - 1, w, h - 1);
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init error: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL3 Brick Breaker", WINDOW_W, WINDOW_H, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer error: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GameState state = GameState::MENU;
    int level = 1;
    const int maxLevels = 3;
    int unlockedLevel = 1;
    int score = 0;
    int lives = 3;

    SDL_FRect paddle{ (WINDOW_W - 120) / 2.0f, WINDOW_H - 50.0f, 120, (float)PADDLE_H };
    SDL_FRect ball{ WINDOW_W / 2.0f - BALL_SIZE / 2.0f, WINDOW_H / 2.0f - BALL_SIZE / 2.0f, (float)BALL_SIZE, (float)BALL_SIZE };
    float ballVX = 380.0f;
    float ballVY = -380.0f;
    bool launched = false;

    std::vector<Brick> bricks = createBricks(5, 10, WINDOW_W);

    Uint64 prev = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double delta = (double)(now - prev) / (double)SDL_GetPerformanceFrequency();
        prev = now;
        float dt = (float)delta;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;

            if (state == GameState::MENU) {
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    int mx = e.button.x, my = e.button.y;
                    SDL_FRect playButton{ WINDOW_W / 2.0f - 100, WINDOW_H / 2.0f - 60, 200, 50 };
                    SDL_FRect levelButton{ WINDOW_W / 2.0f - 100, WINDOW_H / 2.0f + 10, 200, 50 };

                    if (mx >= playButton.x && mx <= playButton.x + playButton.w &&
                        my >= playButton.y && my <= playButton.y + playButton.h) {
                        level = 1;
                        score = 0;
                        lives = 3;
                        launched = false;
                        paddle.w = 120;
                        ballVX = 380.0f;
                        ballVY = -380.0f;
                        bricks = createBricks(5, 10, WINDOW_W);
                        state = GameState::PLAYING;
                    }
                    else if (mx >= levelButton.x && mx <= levelButton.x + levelButton.w &&
                        my >= levelButton.y && my <= levelButton.y + levelButton.h) {
                        state = GameState::LEVEL_SELECT;
                    }
                }
            }

            else if (state == GameState::LEVEL_SELECT) {
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    int mx = e.button.x, my = e.button.y;
                    SDL_FRect backButton{ 20, 20, 100, 40 };
                    if (mx >= backButton.x && mx <= backButton.x + backButton.w &&
                        my >= backButton.y && my <= backButton.y + backButton.h) {
                        state = GameState::MENU;
                    }

                    for (int i = 1; i <= maxLevels; ++i) {
                        SDL_FRect lvlBtn{ WINDOW_W / 2.0f - 75, 150 + i * 70, 150, 50 };
                        if (mx >= lvlBtn.x && mx <= lvlBtn.x + lvlBtn.w &&
                            my >= lvlBtn.y && my <= lvlBtn.y + lvlBtn.h && i <= unlockedLevel) {
                            level = i;
                            score = 0;
                            lives = 3;
                            launched = false;
                            int rows = 5 + (level - 1);
                            float newSpeed = 380.0f + (level - 1) * 60.0f;
                            float newPaddleW = 120.0f - (level - 1) * 20.0f;
                            paddle.w = newPaddleW;
                            ballVX = newSpeed;
                            ballVY = -newSpeed;
                            bricks = createBricks(rows, 10, WINDOW_W);
                            state = GameState::PLAYING;
                        }
                    }
                }
            }

            else if (state == GameState::PLAYING) {
                if (e.type == SDL_EVENT_KEY_DOWN) {
                    SDL_Scancode sc = e.key.scancode;

                    if (sc == SDL_SCANCODE_ESCAPE) state = GameState::MENU;
                    else if (sc == SDL_SCANCODE_SPACE && !launched) launched = true;
                    else if (sc == SDL_SCANCODE_R) {
                        launched = false;
                        score = 0;
                        lives = 3;
                        int rows = 5 + (level - 1);
                        float newSpeed = 380.0f + (level - 1) * 60.0f;
                        paddle.w = 120.0f - (level - 1) * 20.0f;
                        ballVX = newSpeed;
                        ballVY = -newSpeed;
                        bricks = createBricks(rows, 10, WINDOW_W);
                    }
                    else if (sc == SDL_SCANCODE_F) for (auto& b : bricks) b.alive = false;
                }
            }

            else if (state == GameState::GAME_OVER) {
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    int mx = e.button.x, my = e.button.y;
                    SDL_FRect againBtn{ WINDOW_W / 2.0f - 100, WINDOW_H / 2.0f - 25, 200, 50 };
                    if (mx >= againBtn.x && mx <= againBtn.x + againBtn.w &&
                        my >= againBtn.y && my <= againBtn.y + againBtn.h) {
                        state = GameState::MENU;
                    }
                }
            }
        }

        if (state == GameState::PLAYING) {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A])
                paddle.x -= PADDLE_SPEED * dt;
            if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D])
                paddle.x += PADDLE_SPEED * dt;

            if (paddle.x < 0) paddle.x = 0;
            if (paddle.x + paddle.w > WINDOW_W) paddle.x = WINDOW_W - paddle.w;

            if (launched) {
                ball.x += ballVX * dt;
                ball.y += ballVY * dt;

                if (ball.x <= 0 || ball.x + BALL_SIZE >= WINDOW_W)
                    ballVX = -ballVX;
                if (ball.y <= 0)
                    ballVY = -ballVY;

                if (intersects(ball, paddle) && ballVY > 0) {
                    ballVY = -fabs(ballVY);
                }

                for (auto& b : bricks) {
                    if (b.alive && intersects(ball, b.rect)) {
                        b.alive = false;
                        score += 100;
                        ballVY = -ballVY;
                        break;
                    }
                }

                if (ball.y + BALL_SIZE > WINDOW_H) {
                    lives--;
                    launched = false;
                    ball.x = paddle.x + paddle.w / 2 - BALL_SIZE / 2;
                    ball.y = paddle.y - BALL_SIZE - 2;
                    if (lives <= 0) {
                        state = GameState::GAME_OVER;
                    }
                }

                bool allDead = true;
                for (auto& b : bricks) if (b.alive) allDead = false;
                if (allDead) {
                    unlockedLevel = std::min(unlockedLevel + 1, maxLevels);
                    state = GameState::LEVEL_SELECT;
                }
            }
            else {
                ball.x = paddle.x + paddle.w / 2 - BALL_SIZE / 2;
                ball.y = paddle.y - BALL_SIZE - 2;
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        if (state == GameState::MENU) {
            SDL_FRect playButton{ WINDOW_W / 2.0f - 100, WINDOW_H / 2.0f - 60, 200, 50 };
            SDL_FRect levelButton{ WINDOW_W / 2.0f - 100, WINDOW_H / 2.0f + 10, 200, 50 };
            drawButton(renderer, playButton, SDL_Color{ 0, 180, 255, 255 });
            drawButton(renderer, levelButton, SDL_Color{ 0, 255, 100, 255 });
        }

        else if (state == GameState::LEVEL_SELECT) {
            SDL_FRect backButton{ 20, 20, 100, 40 };
            drawButton(renderer, backButton, SDL_Color{ 255, 80, 80, 255 });
            for (int i = 1; i <= maxLevels; ++i) {
                SDL_FRect lvlBtn{ WINDOW_W / 2.0f - 75, 150 + i * 70, 150, 50 };
                SDL_Color color = (i <= unlockedLevel) ? SDL_Color{ 0, 200, 255, 255 } : SDL_Color{ 100, 100, 100, 255 };
                drawButton(renderer, lvlBtn, color);
            }
        }

        else if (state == GameState::PLAYING) {
            drawBorder(renderer, WINDOW_W, WINDOW_H);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &paddle);
            SDL_RenderFillRect(renderer, &ball);
            for (auto& b : bricks)
                if (b.alive) {
                    SDL_SetRenderDrawColor(renderer, b.color.r, b.color.g, b.color.b, b.color.a);
                    SDL_RenderFillRect(renderer, &b.rect);
                }

            SDL_FRect scoreBar{ 10, 10, (float)(score / 10 % 200 + 50), 10 };
            SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255);
            SDL_RenderFillRect(renderer, &scoreBar);

            for (int i = 0; i < lives; i++) {
                SDL_FRect heart{ WINDOW_W - 20 - i * 20, 10, 10, 10 };
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderFillRect(renderer, &heart);
            }
        }

        else if (state == GameState::GAME_OVER) {
            SDL_FRect againBtn{ WINDOW_W / 2.0f - 100, WINDOW_H / 2.0f - 25, 200, 50 };
            drawButton(renderer, againBtn, SDL_Color{ 255, 150, 0, 255 });
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}