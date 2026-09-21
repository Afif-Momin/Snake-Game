#pragma once
#include <deque>
#include <random>
#include <string>
#include <chrono>
#include "Leaderboard.h"

// Represents a point on the game grid
struct Point {
    int x, y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

// Snake direction
enum Direction { UP, DOWN, LEFT, RIGHT };

// ── Object seam: abstract random source ──────────────────────────
class RandomSource {
public:
    virtual ~RandomSource() = default;
    virtual int randomInt(int lo, int hi) = 0;
};

// Production implementation – same mt19937 behaviour as before
class ProductionRandomSource : public RandomSource {
public:
    ProductionRandomSource()
        : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {}
    int randomInt(int lo, int hi) override {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(rng_);
    }
private:
    std::mt19937 rng_;
};

// Main game class
class Game {
public:
    // Original constructor signature preserved (uses internal RNG)
    Game(int w = 20, int h = 15, int delay = 150);

    // Seam-enabled constructor: caller supplies a RandomSource
    Game(int w, int h, int delay, RandomSource* rngSource);

    ~Game();

    void run();

    // ── Public for testing (seam) ────────────────────────────────
    void init();
    void generateApple();
    void generateBomb();
    void move();
    bool isOnSnake(const Point& p) const;
    void setDirection(Direction d);

    // ── Getters for test assertions ──────────────────────────────
    const std::deque<Point>& getBody()   const { return body; }
    Point          getApple()            const { return apple; }
    Point          getBomb()             const { return bomb; }
    int            getScore()            const { return score; }
    bool           isGameOver()          const { return game_over; }
    bool           isBombActive()        const { return bombActive; }
    Direction      getDirection()        const { return dir; }
    int            getWidth()            const { return WIDTH; }
    int            getHeight()           const { return HEIGHT; }

private:
    void draw();
    void processInput();
    void gameOver();

    // Game state
    int WIDTH, HEIGHT;
    int DELAY_MS;
    std::deque<Point> body;
    Direction dir;
    Point apple;
    Point bomb;
    bool running;
    bool paused;
    bool game_over;
    int score;
    int lastBombScore;  // Track when last bomb appeared
    bool bombActive;    // Whether bomb is currently on board

    // Random source (object seam)
    RandomSource* randSrc_;
    bool          ownsRandSrc_;

    // Emoji customization
    std::string headEmoji;
    std::string bodyEmoji;
    std::string appleEmoji;
    std::string bombEmoji;

    // Leaderboard
    Leaderboard leaderboard;
};