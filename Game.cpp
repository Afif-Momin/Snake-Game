#include "Game.h"
#include "Utils.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cctype>
#include <algorithm>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <conio.h>
#endif

// ── Original constructor (production path – unchanged behaviour) ──
Game::Game(int w, int h, int delay) :
    WIDTH(w), HEIGHT(h), DELAY_MS(delay),
    dir(RIGHT), running(false), paused(false), game_over(false),
    score(0), lastBombScore(0), bombActive(false),
    randSrc_(new ProductionRandomSource()), ownsRandSrc_(true),
    leaderboard("leaderboard.json")
{
    headEmoji = "\xF0\x9F\x90\x8D";  // 🐍
    bodyEmoji = "\xF0\x9F\x9F\xA2";  // 🟢
    appleEmoji = "\xF0\x9F\x8D\x8E"; // 🍎
    bombEmoji = "\xF0\x9F\x92\xA3";  // 💣
}

// ── Seam-enabled constructor (test path) ──────────────────────────
Game::Game(int w, int h, int delay, RandomSource* rngSource) :
    WIDTH(w), HEIGHT(h), DELAY_MS(delay),
    dir(RIGHT), running(false), paused(false), game_over(false),
    score(0), lastBombScore(0), bombActive(false),
    randSrc_(rngSource), ownsRandSrc_(false),
    leaderboard("leaderboard.json")
{
    headEmoji = "\xF0\x9F\x90\x8D";
    bodyEmoji = "\xF0\x9F\x9F\xA2";
    appleEmoji = "\xF0\x9F\x8D\x8E";
    bombEmoji = "\xF0\x9F\x92\xA3";
}

Game::~Game() {
    if (ownsRandSrc_) delete randSrc_;
}

void Game::init() {
    body.clear();
    body.push_back({WIDTH/2, HEIGHT/2});
    body.push_back({WIDTH/2 - 1, HEIGHT/2});
    body.push_back({WIDTH/2 - 2, HEIGHT/2});
    dir = RIGHT;
    score = 0;
    lastBombScore = 0;
    bombActive = false;
    game_over = false;
    paused = false;
    generateApple();
}

void Game::generateApple() {
    do {
        apple = { randSrc_->randomInt(1, WIDTH-2), randSrc_->randomInt(1, HEIGHT-2) };
    } while (isOnSnake(apple) || (bombActive && apple == bomb));
}

void Game::generateBomb() {
    do {
        bomb = { randSrc_->randomInt(1, WIDTH-2), randSrc_->randomInt(1, HEIGHT-2) };
    } while (isOnSnake(bomb) || bomb == apple);
    bombActive = true;
}

bool Game::isOnSnake(const Point& p) const {
    for (auto &s : body) if (s == p) return true;
    return false;
}

void Game::setDirection(Direction d) {
    // Prevent 180-degree turns (same logic as processInput)
    if ((dir == UP && d == DOWN) || (dir == DOWN && d == UP) ||
        (dir == LEFT && d == RIGHT) || (dir == RIGHT && d == LEFT)) {
        return;
    }
    dir = d;
}

void Game::processInput() {
    if (!kbhit()) return;
    int c = getch_nonblock();
    if (c == -1) return;

    if (c == 27) {
        int c1 = getch_nonblock();
        int c2 = getch_nonblock();
        if (c1 == '[') {
            switch (c2) {
                case 'A': if (dir != DOWN) dir = UP; break;
                case 'B': if (dir != UP) dir = DOWN; break;
                case 'C': if (dir != LEFT) dir = RIGHT; break;
                case 'D': if (dir != RIGHT) dir = LEFT; break;
            }
        }
        return;
    }

#ifdef _WIN32
    if (c == 0 || c == 224) {
        int c2 = _getch();
        switch (c2) {
            case 72: if (dir != DOWN) dir = UP; break;
            case 80: if (dir != UP) dir = DOWN; break;
            case 75: if (dir != RIGHT) dir = LEFT; break;
            case 77: if (dir != LEFT) dir = RIGHT; break;
        }
        return;
    }
#endif

    char ch = std::tolower((char)c);
    switch (ch) {
        case 'w': if (dir != DOWN) dir = UP; break;
        case 's': if (dir != UP) dir = DOWN; break;
        case 'a': if (dir != RIGHT) dir = LEFT; break;
        case 'd': if (dir != LEFT) dir = RIGHT; break;
        case 'p': paused = !paused; break;
        case 'q': running = false; break;
        default: break;
    }
}

void Game::move() {
    if (game_over || paused) return;

    Point head = body.front();
    Point newHead = head;
    switch (dir) {
        case UP: newHead.y--; break;
        case DOWN: newHead.y++; break;
        case LEFT: newHead.x--; break;
        case RIGHT: newHead.x++; break;
    }

    if (newHead.x <= 0 || newHead.x >= WIDTH-1 || newHead.y <= 0 || newHead.y >= HEIGHT-1) {
        game_over = true;
        return;
    }

    for (const auto &seg : body) {
        if (seg == newHead) {
            game_over = true;
            return;
        }
    }

    body.push_front(newHead);

    if (newHead == apple) {
        score += 10;
        std::cout << "\a" << std::flush;

        if (score - lastBombScore >= 20) {
            generateBomb();
            lastBombScore = score;
        }
        generateApple();
    } 
    else if (bombActive && newHead == bomb) {
        score -= 10;
        if (score < 0) {
            game_over = true;
            return;
        }
        int segmentsToRemove = std::min(2, (int)body.size() - 1);
        for (int i = 0; i < segmentsToRemove; ++i)
            if (body.size() > 1) body.pop_back();
        bombActive = false;
        std::cout << "\a" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "\a" << std::flush;
        return;
    }
    else {
        body.pop_back();
    }
}

void Game::draw() {
    setCursorPosition(0,0);
    std::cout << "\n  " << headEmoji << " TERMINAL SNAKE  " << headEmoji << "\n";
    std::cout << "  Score: " << score << (paused ? "  [PAUSED]" : "") << "    \n\n";

    for (int y=0;y<HEIGHT;y++) {
        std::cout << "  ";
        for (int x=0;x<WIDTH;x++) {
            Point p{x,y};
            if (x==0 || x==WIDTH-1 || y==0 || y==HEIGHT-1) {
                std::cout << "\xe2\xac\x9b";  // ⬛
            } else if (p == body.front()) {
                std::cout << headEmoji;
            } else if (isOnSnake(p)) {
                std::cout << bodyEmoji;
            } else if (p == apple) {
                std::cout << appleEmoji;
            } else if (bombActive && p == bomb) {
                std::cout << bombEmoji;
            } else {
                std::cout << "\xe3\x80\x80";  // fullwidth space
            }
        }
        std::cout << "\n";
    }

    std::cout << "\n  Controls: W/A/S/D | P=Pause | Q=Quit\n";
    std::cout << "  " << bombEmoji << " Bomb appears every 20 points! Avoid it!\n\n";
}

void Game::gameOver() {
    disableRawMode();
    showCursor();

    clearScreen();
    std::cout << "\n\n  \xF0\x9F\x92\x80 GAME OVER! \xF0\x9F\x92\x80\n";
    std::cout << "  Final Score: " << score << "\n\n";

    std::string name = promptLine("  Enter your name: ");
    if (name.empty()) name = "Anonymous";

    std::cout << "\n  Are you a DAIICT student? (y/n): ";
    std::string choice;
    std::getline(std::cin, choice);
    bool isDAIICT = (!choice.empty() && (choice[0] == 'y' || choice[0] == 'Y'));

    std::string program;
    if (isDAIICT) {
        std::cout << "\n  Select your program:\n";
        std::cout << "   1) BTECH\n   2) MTECH\n   3) MSC (IT)\n   4) MDES\n   5) MSC (DATA SCIENCE)\n";
        std::cout << "   Enter choice (1-5): ";

        int progChoice = 0;
        if (!(std::cin >> progChoice)) {
            std::cin.clear(); progChoice = 1;
        }
        std::string temp; std::getline(std::cin, temp);
        switch (progChoice) {
            case 1: program = "BTECH"; break;
            case 2: program = "MTECH"; break;
            case 3: program = "MSCIT"; break;
            case 4: program = "MDES"; break;
            case 5: program = "MSC DS"; break;
            default: program = "BTECH";
        }
        name = "DAIICT-" + program + "-" + name;
    }

    Leaderboard overall("leaderboard_overall.json");
    overall.add(name, score);
    if (isDAIICT) {
        Leaderboard daiict("leaderboard_daiict.json");
        daiict.add(name, score);
    }

    auto color = [](std::string t, std::string c){ return "\033[" + c + "m" + t + "\033[0m"; };

    clearScreen();
    std::cout << "\n";
    std::cout << color("\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97\n", "38;5;214");
    std::cout << color("\xe2\x95\x91              \xF0\x9F\x90\x8D  TERMINAL SNAKE LEADERBOARD  \xF0\x9F\x90\x8D       \xe2\x95\x91\n", "1;93");
    std::cout << color("\xe2\x95\x9A\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9D\n\n", "38;5;214");

    auto showBoard = [&](std::string title, auto &list) {
        std::cout << color("   " + title + "\n", "1;96");
        std::cout << color("   " + std::string(48, '\xe2') + "\n", "36");  // simplified
        int rank = 1;
        for (auto &e : list) {
            std::string medal = rank==1?"\xF0\x9F\xA5\x87":rank==2?"\xF0\x9F\xA5\x88":rank==3?"\xF0\x9F\xA5\x89":" *";
            std::string clr = rank==1?"1;93":rank==2?"1;37":rank==3?"38;5;208":"0;36";
            std::cout << "   " << medal << "  "
                      << std::left << std::setw(25) << e.name
                      << " |  " << color(std::to_string(e.score)+" pts", clr) << "\n";
            rank++;
        }
        std::cout << "\n";
    };

    auto topO = overall.top(10);
    showBoard("\xF0\x9F\x8C\x8D  GLOBAL LEADERBOARD  \xF0\x9F\x8C\x8D", topO);

    Leaderboard daiict("leaderboard_daiict.json");
    auto topD = daiict.top(10);
    if (!topD.empty()) showBoard("\xF0\x9F\x8E\x93  DAIICT LEADERBOARD  \xF0\x9F\x8E\x93", topD);

    std::cout << color("   \xF0\x9F\x92\xA1 Beat the Top 3 and earn your medal!\n\n", "1;92");

    std::cout << "  Play again? (y/n): ";
    std::string restart;
    std::getline(std::cin, restart);

    if (!restart.empty() && (restart[0]=='y'||restart[0]=='Y')) {
        std::cout << "\n  Restarting...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        clearScreen();
        enableRawMode(); hideCursor();
        init(); running = true; game_over = false;
        while (running && !game_over) {
            processInput();
            if (!paused) move();
            draw();
            std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
        }
        disableRawMode(); showCursor(); gameOver();
    } else {
        std::cout << "\n  Thanks for playing! \xF0\x9F\x91\x8B\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

void Game::run() {
    clearScreen();
    std::cout << "Choose head emoji:\n 1) \xF0\x9F\x90\x8D Snake \n 2) \xF0\x9F\x90\x87 Rabbit \n 3) \xF0\x9F\x90\x80 Mouse \n 4) \xF0\x9F\x90\xAD Rat \n 5) Custom\n";
    std::cout << "Enter choice: ";
    int c = 0;
    if (!(std::cin >> c)) { c=1; std::cin.clear(); }
    std::string trash; std::getline(std::cin, trash);
    switch(c){case 1:headEmoji="\xF0\x9F\x90\x8D";break;case 2:headEmoji="\xF0\x9F\x90\x87";break;
        case 3:headEmoji="\xF0\x9F\x90\x80";break;case 4:headEmoji="\xF0\x9F\x90\xAD";break;
        case 5:{std::string e=promptLine("Enter emoji: ");if(!e.empty())headEmoji=e;}break;default:headEmoji="\xF0\x9F\x90\x8D";}

    std::string b=promptLine("Body emoji (default \xF0\x9F\x9F\xA2): ");if(!b.empty())bodyEmoji=b;
    std::string a=promptLine("Apple emoji (default \xF0\x9F\x8D\x8E): ");if(!a.empty())appleEmoji=a;
    std::cout << "\nDifficulty: 1) Easy 2) Normal 3) Hard: "; int d=2;
    if(!(std::cin>>d)){d=2;std::cin.clear();} std::getline(std::cin,trash);
    if(d==1)DELAY_MS*=2;else if(d==3)DELAY_MS=std::max(30,DELAY_MS/2);

    enableRawMode(); hideCursor(); init(); running=true; clearScreen(); draw();
    while(running && !game_over){ processInput(); if(!paused) move(); draw();
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS)); }
    disableRawMode(); showCursor(); gameOver();
}
