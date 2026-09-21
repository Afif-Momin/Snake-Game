# Lab 4 — BitecodesHub/Snake-Game

| | |
|---|---|
| Repository | https://github.com/BitecodesHub/Snake-Game |
| Base tag | `lab4-base` at commit `f0ea27c119936a244a1a0b36a06d68a32761219f` |
| Pull request | Lab 4: Fruit never appears on a cell the snake occupies |

---

## 1. Five rules — [5]

Written before opening the source. Behaviour, with an observable outcome.

| # | Rule |
|---|---|
| 1 | The snake dies when its head collides with the boundary wall. |
| 2 | The snake dies when its head enters its own body (self-collision). |
| 3 | Eating an apple increases the player score by exactly 10 points. |
| 4 | An apple never spawns on a cell that the snake's body currently occupies. |
| 5 | Hitting an active bomb reduces the player's score by 10 points. |

All rules were stated before inspecting implementation details. The exact scoring delta (+10 for normal food) is observable from ordinary gameplay.

---

## 2. What you could test, and what stopped you — [10]

No source changes in this part. Every `file:line` below is a line in `lab4-base`.

| # | Rule | Test written? | Blocking dependency (`file:line` + what it is) |
|---|---|---|---|
| 1 | Snake dies on wall collision | No | `Game.h:27,41` — `move()` and `game_over` flag are private with no public getters/setters; `Game.cpp:61-103` — `processInput()` directly calls `kbhit()`/`getch_nonblock()` from terminal I/O; `Game.cpp:312` — game loop in `run()` contains unintercepted `std::this_thread::sleep_for()`. |
| 2 | Snake dies on self-collision | No | `Game.h:27,35,41` — `move()`, `body`, and `game_over` are private members; `Game.cpp:29-41` — `init()` hardcodes initial body to length 3 with no public interface to configure body geometry or trigger steps without interactive `run()`. |
| 3 | Eating apple adds 10 points | No | `Game.cpp:19` — `rng` is seeded directly with `steady_clock::now()` inside private constructor state; `Game.cpp:43-48` — `generateApple()` is private and non-deterministic, preventing placing an apple predictably in front of the snake; `Game.h:42` — `score` has no public getter. |
| 4 | Apple never spawns on snake | No | `Game.cpp:43-48` — `generateApple()` is private and uses internal non-deterministic `std::mt19937 rng`; `Game.h:26,35,37` — `isOnSnake()`, `body`, and `apple` are private members, so neither generation nor placement invariants can be invoked or asserted from a test harness. |
| 5 | Hitting bomb reduces score by 10 | No | `Game.cpp:49-54` — `generateBomb()` is private and driven by non-deterministic RNG; `Game.h:38,42,44` — `bomb`, `score`, and `bombActive` are private; `Game.cpp:141-155` — bomb collision handling is buried inside private `move()` with embedded terminal beep `\a` and hardcoded `sleep_for(100ms)`. |

> **Rules testable without modifying the source: 0 / 5**

"It needs user input" is not a blocking dependency. In `lab4-base`, all 5 core gameplay rules are rendered completely untestable because `Game` encapsulates all game loop logic (`move()`, `init()`, `generateApple()`, `generateBomb()`) and state (`body`, `score`, `apple`, `bomb`, `game_over`) as private members with zero public accessors, while coupling execution directly to hardware clock seeding (`Game.cpp:19`), interactive console polling (`Game.cpp:62`), and thread sleeps (`Game.cpp:152, 312`).

---

## 3. Coverage, and what it missed — [6]

| | |
|---|---|
| Line coverage | 32.52 % of 246 lines (Game.cpp) |
| Branch coverage | 18.41 % of 592 branches (Game.cpp) |
| Command used | `g++ --coverage -O0 -g -std=c++17 -I. -Ilab4/googletest/googletest/include Game.cpp Utils.cpp Leaderboard.cpp lab4/test_game.cpp lab4/gtest-all.o -o lab4/tests.exe && ./lab4/tests.exe && gcov -b lab4/tests-Game.gcno` |

**One rule that is executed by the suite but not verified by it:**

| | |
|---|---|
| Rule | Rule 4: An apple never spawns on a cell the snake occupies. |
| Line that runs | `Game.cpp:139` (and `Game.cpp:48` in `lab4-base`, called inside `init()` / apple consumption) |
| The assertion that is missing | In tests like `EatingAppleIncreasesScoreByTen` (and `BombReducesScoreAndGameOverIfNegative`), when an apple is eaten, `generateApple()` runs and executes the `while (isOnSnake(apple) || ...)` rejection loop, but the test only asserts `EXPECT_EQ(game->getScore(), 10)` and completely omits asserting `EXPECT_FALSE(game->isOnSnake(game->getApple()))` on the newly spawned apple. |

---

## 4. The seam — [10]

| | |
|---|---|
| Rule made testable | Rule 4: An apple never spawns on a cell the snake occupies |
| Commit 1 (seam) | `adf2922ca8ca937add506a270c31833630a496f9` |
| Commit 2 (test) | `5916ad4fe6ea8337e84ccbe8acb81e346b4b947c` |
| Seam kind | object |
| Enabling point | `Game::Game(int w, int h, int delay, RandomSource* rngSource)` constructor parameter |
| What production code gave up | Nothing |

The seam cost nothing in production. The original default constructor `Game(int w, int h, int delay)` remains available and instantiates `ProductionRandomSource`, which wraps the identical `std::mt19937` engine seeded via `std::chrono::steady_clock::now().time_since_epoch().count()` as before. All gameplay rules, timing, terminal drawing, and user-facing behaviours are 100% identical. The only overhead is virtual method dispatch on coordinate generation during apple/bomb spawns, which is completely unmeasurable in a game with 120ms tick loops.

---

## 5. The double — [4]

| | |
|---|---|
| What you passed through the seam | stub (`FakeRandomSource`) |
| The method under test | `Game::generateApple()` |

Was the collaborator asked a question or told to do something? The collaborator was asked a question ("what random integer should be generated between `lo` and `hi`?"), returning a numeric coordinate answer. Because the collaborator supplies indirect input values queried by `generateApple()`, a stub providing pre-programmed coordinate responses was used rather than a mock verifying side-effect invocations.

---

## 6. Two smells in your own tests — [5]

| | Smell | `file:line` | One-line fix |
|---|---|---|---|
| 1 | General fixture | `lab4/test_game.cpp:33-39` | Remove `SetUp()` and construct `Game` with explicit coordinates directly inside each individual test. |
| 2 | Eager test | `lab4/test_game.cpp:167-220` | Split `BombReducesScoreAndGameOverIfNegative` into two separate tests: `BombDecreasesScore` and `BombTriggersGameOverWhenScoreNegative`. |

Fixing them is optional. Finding them is not.
