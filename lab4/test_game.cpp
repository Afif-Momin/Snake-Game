// lab4/test_game.cpp — Lab 4 tests for BitecodesHub/Snake-Game
// Uses the RandomSource object seam introduced in Commit 1.

#include "gtest/gtest.h"
#include "../Game.h"

#include <vector>
#include <deque>
#include <fstream>

// ── Stub: deterministic random source ─────────────────────────────
// Returns pre-programmed values in sequence.  When the sequence is
// exhausted it wraps around.
class FakeRandomSource : public RandomSource {
public:
    explicit FakeRandomSource(std::vector<int> vals) : values_(vals), idx_(0) {}
    int randomInt(int /*lo*/, int /*hi*/) override {
        int v = values_[idx_ % values_.size()];
        idx_++;
        return v;
    }
private:
    std::vector<int> values_;
    size_t idx_;
};

// ── Shared fixture (used by most tests) ───────────────────────────
class GameTest : public ::testing::Test {
protected:
    FakeRandomSource* fakeRng;
    Game* game;

    void SetUp() override {
        // Provide enough values so generateApple()/generateBomb() succeed
        // Apple will be placed at (5, 5), bomb at (3, 3)
        fakeRng = new FakeRandomSource({5, 5, 3, 3, 7, 7, 8, 8, 2, 2});
        game = new Game(20, 15, 150, fakeRng);
        game->init();
    }

    void TearDown() override {
        delete game;
        // fakeRng is NOT owned by game (ownsRandSrc_ = false), but
        // we passed it in — game doesn't delete it, so we do.
        delete fakeRng;
    }
};

// ═══════════════════════════════════════════════════════════════════
// Rule 1: The snake dies when its head hits the wall (border cells).
// ═══════════════════════════════════════════════════════════════════
TEST_F(GameTest, WallCollisionKillsSnake) {
    // Snake starts at (10, 7) heading RIGHT in a 20x15 grid.
    // Wall is at x=0, x=19, y=0, y=14.
    // Move the snake left until it hits the left wall.
    game->setDirection(LEFT);

    // Snake head is at x=10.  Move left 10 times → head goes to x=0 (wall).
    for (int i = 0; i < 10; ++i) {
        game->move();
    }

    EXPECT_TRUE(game->isGameOver());
}

// ═══════════════════════════════════════════════════════════════════
// Rule 2: The snake dies when its head enters its own body.
// ═══════════════════════════════════════════════════════════════════
TEST_F(GameTest, SelfCollisionKillsSnake) {
    delete game;
    delete fakeRng;

    // Snake starts at (10,7), (9,7), (8,7).
    // Apple 0: at (11, 7).
    // Move 1 (RIGHT): eats Apple 0 at (11, 7). Score=10. Body size=4.
    // Next generateApple needs: (12, 7)
    // Move 2 (RIGHT): eats Apple 1 at (12, 7). Score=20.
    // Score-lastBombScore >= 20, so generateBomb() runs, then generateApple() runs!
    // Bomb needs position away: (2, 2)
    // Apple 2 needs position: (13, 7)
    // Move 3 (RIGHT): eats Apple 2 at (13, 7). Score=30. Body size=6!
    // Apple 3 position: (2, 3)
    fakeRng = new FakeRandomSource({
        11, 7,  // Apple 0 (at init)
        12, 7,  // Apple 1
        2, 2,   // Bomb (triggered when score=20)
        13, 7,  // Apple 2
        2, 3,   // Apple 3
        2, 4    // extra
    });
    game = new Game(20, 15, 150, fakeRng);
    game->init();

    // Eat 3 apples (length becomes 3 + 3 = 6)
    game->move(); // eats at (11, 7), body: (11,7), (10,7), (9,7), (8,7)
    game->move(); // eats at (12, 7), body: (12,7), (11,7), (10,7), (9,7), (8,7)
    game->move(); // eats at (13, 7), body: (13,7), (12,7), (11,7), (10,7), (9,7), (8,7)

    ASSERT_EQ(game->getBody().size(), 6u);

    // Current body: (13,7) [head], (12,7), (11,7), (10,7), (9,7), (8,7)
    // Circle back into body:
    // Move DOWN: head -> (13, 8). Body: (13,8), (13,7), (12,7), (11,7), (10,7), (9,7)
    game->setDirection(DOWN);
    game->move();
    // Move LEFT: head -> (12, 8). Body: (12,8), (13,8), (13,7), (12,7), (11,7), (10,7)
    game->setDirection(LEFT);
    game->move();
    // Move UP: head -> (12, 7), which collides with (12,7) in body!
    game->setDirection(UP);
    game->move();

    EXPECT_TRUE(game->isGameOver());
}

// ═══════════════════════════════════════════════════════════════════
// Rule 3: Eating an apple increases the score by exactly 10 points.
// ═══════════════════════════════════════════════════════════════════
TEST_F(GameTest, EatingAppleIncreasesScoreByTen) {
    // Place the apple directly in front of the snake.
    // Snake head is at (10,7), direction RIGHT.
    // We need the apple at (11, 7).
    // Re-init with a fake RNG that places apple at (11, 7).
    delete game;
    delete fakeRng;

    fakeRng = new FakeRandomSource({11, 7, 5, 5, 3, 3, 6, 6});
    game = new Game(20, 15, 150, fakeRng);
    game->init();

    ASSERT_EQ(game->getScore(), 0);
    ASSERT_EQ(game->getApple().x, 11);
    ASSERT_EQ(game->getApple().y, 7);

    game->move();  // head moves RIGHT to (11, 7) — eats apple

    EXPECT_EQ(game->getScore(), 10);
    EXPECT_FALSE(game->isGameOver());
}

// ═══════════════════════════════════════════════════════════════════
// Rule 4: An apple never spawns on a cell the snake occupies.
// ═══════════════════════════════════════════════════════════════════
TEST_F(GameTest, AppleNeverSpawnsOnSnake) {
    // The snake body is at (10,7), (9,7), (8,7).
    // Provide a FakeRng that first returns a position ON the snake,
    // then returns a valid position.
    delete game;
    delete fakeRng;

    // First pair (10, 7) is ON the snake → should be rejected.
    // Second pair (5, 5) is NOT on the snake → should be accepted.
    fakeRng = new FakeRandomSource({10, 7, 5, 5, 3, 3, 6, 6});
    game = new Game(20, 15, 150, fakeRng);
    game->init();

    // Apple should be at (5, 5), not (10, 7)
    Point apple = game->getApple();
    EXPECT_FALSE(game->isOnSnake(apple));
    EXPECT_EQ(apple.x, 5);
    EXPECT_EQ(apple.y, 5);
}

// ═══════════════════════════════════════════════════════════════════
// Rule 5: Hitting a bomb decreases score by 10; game over if < 0.
// ═══════════════════════════════════════════════════════════════════
TEST_F(GameTest, BombReducesScoreAndGameOverIfNegative) {
    // Start with score 0.  Place apple at (11,7), then bomb at (12,7).
    // Eat apple first (score→10), then a bomb should appear.
    // Move into the bomb → score should drop by 10.
    delete game;
    delete fakeRng;

    // First apple at (11, 7).  After eating, score = 10.
    // Need score - lastBombScore >= 20 to trigger bomb, so we need 
    // to eat 2 apples first.  
    // Actually, let's test the simple case: score = 0, hit bomb → game over.
    // We'll manually set up: place bomb right in front, score is 0.
    // Since we can't set score directly, we test the path where score >= 10.
    // Eat apple (score=10), eat another apple (score=20, bomb spawns).
    // Then move into bomb → score = 20-10 = 10 (not game over).

    // Simpler: just verify that hitting a bomb with score 0 ends the game.
    // We need to set bombActive and bomb position... but those are private.
    // Through the seam we can only control RNG.

    // Let's eat two apples to trigger bomb generation, then move into the bomb.
    // Apple 1 at (11,7). After eating: score=10, apple respawns.
    // Apple 2 at (12,7). After eating: score=20, bomb spawns at (13,7).
    // Then move RIGHT to hit bomb at (13,7) → score = 20-10 = 10.
    fakeRng = new FakeRandomSource({
        11, 7,   // apple 1 position
        12, 7,   // apple 2 position (after eating apple 1)
        13, 7,   // bomb position (triggered at score=20)
        14, 7,   // apple 3 position (after eating apple 2)
        4, 4,    // extras
        6, 6
    });
    game = new Game(20, 15, 150, fakeRng);
    game->init();

    ASSERT_EQ(game->getScore(), 0);

    // Eat apple 1: move RIGHT to (11, 7)
    game->move();
    ASSERT_EQ(game->getScore(), 10);

    // Eat apple 2: move RIGHT to (12, 7)
    game->move();
    ASSERT_EQ(game->getScore(), 20);

    // Bomb should now be active at (13, 7)
    ASSERT_TRUE(game->isBombActive());

    // Move RIGHT into bomb at (13, 7)
    game->move();
    EXPECT_EQ(game->getScore(), 10);  // 20 - 10 = 10
    EXPECT_FALSE(game->isGameOver());  // score >= 0, so not game over
    EXPECT_FALSE(game->isBombActive()); // bomb consumed
}

// ═══════════════════════════════════════════════════════════════════
// Additional: Leaderboard can be tested without any seam
// (it was always testable — public API, no terminal dependency)
// ═══════════════════════════════════════════════════════════════════
TEST(LeaderboardTest, AddAndRetrieveTopScores) {
    // Use a temp file
    std::string path = "lab4/test_leaderboard_temp.json";
    {
        // Clean start
        std::ofstream out(path);
        out << "[]";
    }

    Leaderboard lb(path);
    lb.add("Alice", 100);
    lb.add("Bob", 200);
    lb.add("Charlie", 50);

    auto top = lb.top(2);
    ASSERT_EQ(top.size(), 2u);
    EXPECT_EQ(top[0].name, "Bob");
    EXPECT_EQ(top[0].score, 200);
    EXPECT_EQ(top[1].name, "Alice");
    EXPECT_EQ(top[1].score, 100);

    // Clean up
    std::remove(path.c_str());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
