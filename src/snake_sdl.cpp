// ============================================================================
//  Змейка на C++ + SDL2 — графическая версия (визуализатор вместо терминала)
//  Домашнее задание 1, вариант: «5 сундуков».
//
//  Правила:
//    - поле 10x10, змейка стартует из центра, длина равна 1;
//    - ягода появляется случайно на свободной клетке, даёт +1 к счёту
//      и +1 к длине (красный квадратик — виден ВСЕГДА);
//    - на поле спрятаны 5 сундуков — они мигают: в каждый момент времени
//      виден только ОДИН жёлтый сундук, каждые BLINK_STEPS шагов (сейчас 10
//      ходов ~ 1.5 сек — медленное переключение) видимость переходит к
//      следующему. Сундук можно открыть, когда он ВИДЕН и змейка наступила
//      на него; КРОМЕ ТОГО, после окончания мигания клетка сундука ещё
//      HIGHLIGHT_STEPS шагов подсвечена тёмно-жёлтым («послесвечение») —
//      по ней тоже можно забрать сундук: +2 очка и +1 к длине;
//    - цель — собрать все 5 сундуков (экран победы);
//    - столкновение со стеной или собственным хвостом -> проигрыш;
//      в конце игры выводится итоговый счёт;
//    - справка: показывается до старта и вызывается во время игры (H).
//
//  Сборка (WSL / Linux):
//      sudo apt install libsdl2-dev
//      g++ -std=c++17 -O2 -Wall snake_sdl.cpp -o snake_sdl $(sdl2-config --cflags --libs)
//  Запуск:
//      ./snake_sdl
//  Из VS Code: Terminal -> New Terminal (WSL), те же команды.
// ============================================================================

#include "snake_sdl.h"

#include <SDL2/SDL.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace snake {

// --------------------------------- Константы --------------------------------
const int GRID = 10;                       // размер поля 10x10 (клеток)
const int CELL = 48;                       // размер клетки в пикселях
const int TOPBAR = 56;                     // высота панели со счётом
const int WIN_W = GRID * CELL;             // ширина окна
const int WIN_H = GRID * CELL + TOPBAR;    // высота окна
const int FPS_MS = 150;                    // пауза между ходами (мс)
const int NUM_CHESTS = 5;                  // количество сундуков по заданию
const int BLINK_STEPS = 10;                // сундук виден 10 ходов (~1.5 сек), затем мигает дальше
const int HIGHLIGHT_STEPS = 20;            // «послесвечение»: подсветка клетки сундука
                                           // ещё 20 ходов после мигания — по ней тоже
                                           // можно забрать сундук (он уже не виден)

enum Dir { DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3 };
// dirX — смещение по горизонтали (x), dirY — смещение по вертикали (y).
const int dirX[4] = { 0, 0,-1, 1};         // dx для UP, DOWN, LEFT, RIGHT
const int dirY[4] = {-1, 1, 0, 0};         // dy для UP, DOWN, LEFT, RIGHT

// Цвета (RGB)
const SDL_Color COL_BG     = { 24, 26, 34 , 255};
const SDL_Color COL_GRID   = { 40, 44, 56 , 255};
const SDL_Color COL_SNAKE  = { 60, 200, 110, 255};
const SDL_Color COL_HEAD   = { 120, 255, 160, 255};
const SDL_Color COL_FOOD   = { 240, 80, 80 , 255};   // ягоды — красные
const SDL_Color COL_TEXT   = { 230, 230, 235, 255};
const SDL_Color COL_PANEL  = { 16, 18, 24 , 255};
const SDL_Color COL_CHEST  = { 255, 215, 0  , 255};  // сундуки — ЖЁЛТЫЕ
const SDL_Color COL_GOLD   = { 255, 255, 160, 255};  // рамка/блик сундука
const SDL_Color COL_GLOW   = { 140, 110, 20 , 255};  // «послесвечение» — тёмно-жёлтая подсветка клетки

// ------------------------------ Простой текст -------------------------------
// Рендерим текст без внешних шрифтов: встроенный bitmap-шрифт (8x8 маска).
struct Glyph { char ch; unsigned char rows[8]; };

static const Glyph FONT[] = {
 {' ',{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
 {'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11,0x00}},
 {'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E,0x00}},
 {'C',{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E,0x00}},
 {'D',{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E,0x00}},
 {'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F,0x00}},
 {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10,0x00}},
 {'G',{0x0E,0x11,0x10,0x17,0x11,0x11,0x0E,0x00}},
 {'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11,0x00}},
 {'I',{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E,0x00}},
 {'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C,0x00}},
 {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11,0x00}},
 {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F,0x00}},
 {'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11,0x00}},
 {'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11,0x00}},
 {'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E,0x00}},
 {'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10,0x00}},
 {'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D,0x00}},
 {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11,0x00}},
 {'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E,0x00}},
 {'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0x00}},
 {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E,0x00}},
 {'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04,0x00}},
 {'W',{0x11,0x11,0x11,0x15,0x15,0x1B,0x11,0x00}},
 {'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11,0x00}},
 {'Y',{0x11,0x11,0x0A,0x04,0x04,0x04,0x04,0x00}},
 {'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F,0x00}},
 {'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E,0x00}},
 {'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E,0x00}},
 {'2',{0x0E,0x11,0x01,0x06,0x08,0x10,0x1F,0x00}},
 {'3',{0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E,0x00}},
 {'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02,0x00}},
 {'5',{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E,0x00}},
 {'6',{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E,0x00}},
 {'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08,0x00}},
 {'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E,0x00}},
 {'9',{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C,0x00}},
 {'-',{0x00,0x00,0x00,0x0E,0x00,0x00,0x00,0x00}},
 {'.',{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}},
 {',',{0x00,0x00,0x00,0x00,0x0C,0x04,0x08,0x00}},
 {':',{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00,0x00}},
 {'!',{0x04,0x04,0x04,0x04,0x04,0x00,0x04,0x00}},
 {'?',{0x0E,0x11,0x01,0x06,0x04,0x00,0x04,0x00}},
 {'/',{0x01,0x01,0x02,0x04,0x08,0x10,0x10,0x00}},
 {'%',{0x11,0x01,0x02,0x04,0x08,0x10,0x11,0x00}},
 {'+',{0x00,0x04,0x04,0x1F,0x04,0x04,0x00,0x00}},
 {'=',{0x00,0x00,0x1F,0x00,0x1F,0x00,0x00,0x00}},
 {'(',{0x02,0x04,0x08,0x08,0x08,0x04,0x02,0x00}},
 {')',{0x08,0x04,0x02,0x02,0x02,0x04,0x08,0x00}}
};

// Функция поиска глифа; отсутствующие символы рисуем как '?'
static const Glyph* findGlyph(char c) {
    if ((unsigned char)c >= 0x80) { static const Glyph sp{' ',{0,0,0,0,0,0,0,0}}; return &sp; }
    for (const auto &g : FONT) if (g.ch == c) return &g;
    static const Glyph q = {'?',{0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}};
    return &q;
}

// Ширина строки в пикселях при данном масштабе
static int textWidth(const char *text, int scale) {
    int n = 0;
    for (const char *p = text; *p; ++p) ++n;
    return n > 0 ? n * 6 * scale - scale : 0; // 5px глиф + 1px интервал
}

static void drawText(SDL_Renderer *r, int x, int y, const char *text,
                     SDL_Color color, int scale = 2) {
    if (x == INT_MIN) x = (WIN_W - textWidth(text, scale)) / 2; // центрирование
    int cx = x;
    for (const char *p = text; *p; ++p) {
        const Glyph *g = findGlyph(*p);
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (g->rows[row] & (0x80 >> col)) {
                    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
                    SDL_Rect px = { cx + col*scale, y + row*scale, scale, scale };
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
        cx += 6 * scale;
    }
}

// ---------------------------------- Игра ------------------------------------
struct Cell { int x, y; };

class Game {
public:
    Cell body[GRID*GRID];   // массив сегментов тела (кольцевой буфер)
    int headIdx = 0;        // индекс головы в кольцевом буфере
    int length = 1;         // текущая длина змейки
    Dir dir = DIR_RIGHT;    // текущее направление
    Cell food{};            // позиция ягоды (еды)
    int score = 0;          // счёт
    bool gameOver = false;  // флаг проигрыша
    bool win = false;       // флаг победы (собраны все сундуки)
    bool paused = false;    // пауза (клавиша P)
    bool showHelp = false;  // оверлей справки (клавиша H)

    // ---- Вариант задания: 5 сундуков ----
    Cell chests[NUM_CHESTS];        // позиции всех сундуков (массив)
    bool chestOpened[NUM_CHESTS];   // открыт ли сундук
    int visibleChest = 0;           // какой сундук ВИДЕН сейчас (мигает)
    int blinkTimer = 0;             // счётчик шагов до смены видимого сундука
    int glowChest = -1;             // сундук с «послесвечением»: он перестал
                                    // мигать, но его клетку ещё можно забрать
    int glowLeft = 0;               // сколько шагов осталось у подсветки
    int chestsCollected = 0;        // сколько сундуков собрано
    int lastEvent = 0;              // код последнего события для экрана конца
    // коды событий: 0 нет, 1 ягода, 2 сундук, 3 стена, 4 хвост, 5 победа

    Game() { reset(); }

    void reset() {
        // Змейка стартует в центре поля, длина равна 1
        body[0] = { GRID/2, GRID/2 };
        headIdx = 0; length = 1; dir = DIR_RIGHT;
        score = 0; gameOver = false; win = false; paused = false;
        lastEvent = 0;
        placeItems();
    }

    // Клетка занята телом змейки?
    bool isBody(int x, int y) const {
        for (int i = 0; i < length; ++i) {
            const Cell &c = body[(headIdx - i + GRID*GRID) % (GRID*GRID)];
            if (c.x == x && c.y == y) return true;
        }
        return false;
    }

    // Ягода и сундуки появляются случайно на СВОБОДНЫХ клетках
    // (используется массив свободных клеток — требование к базовой программе)
    void placeItems() {
        Cell freeCells[GRID*GRID];
        int n = 0;
        for (int y = 0; y < GRID; ++y)
            for (int x = 0; x < GRID; ++x)
                if (!isBody(x, y)) freeCells[n++] = { x, y };

        // Перемешиваем свободные клетки (Fisher–Yates), чтобы взять разные
        for (int i = n - 1; i > 0; --i) {
            int j = rand() % (i + 1);
            Cell t = freeCells[i]; freeCells[i] = freeCells[j]; freeCells[j] = t;
        }
        // Первые NUM_CHESTS клеток — под сундуки, следующая — под ягоду
        for (int i = 0; i < NUM_CHESTS; ++i) {
            chests[i] = freeCells[i];
            chestOpened[i] = false;
        }
        food = freeCells[NUM_CHESTS];
        chestsCollected = 0;
        visibleChest = 0;   // с первого кадра виден сундук #1
        blinkTimer = 0;
        glowChest = -1;     // послесвечение ещё не активировано
        glowLeft = 0;
    }

    // Один шаг игры: движение, столкновения, еда, сундуки, мигание
    void step() {
        if (gameOver || win || paused) return;

        Cell next = { body[headIdx].x + dirX[dir],
                      body[headIdx].y + dirY[dir] };

        // Столкновение со стеной -> проигрыш
        if (next.x < 0 || next.x >= GRID || next.y < 0 || next.y >= GRID) {
            gameOver = true; lastEvent = 3; return;
        }

        // Открыть сундук можно, когда он ВИДЕН (мигает), ИЛИ по его
        // «послесвечению» — клетке, подсвеченной после окончания мигания
        bool willEatFood = (next.x == food.x && next.y == food.y);
        bool willOpenChest = false;
        int chestIdx = -1;
        if (visibleChest >= 0 && visibleChest < NUM_CHESTS && !chestOpened[visibleChest] &&
            next.x == chests[visibleChest].x && next.y == chests[visibleChest].y) {
            willOpenChest = true; chestIdx = visibleChest;
        } else if (glowChest >= 0 && glowLeft > 0 && !chestOpened[glowChest] &&
            next.x == chests[glowChest].x && next.y == chests[glowChest].y) {
            willOpenChest = true; chestIdx = glowChest;   // взятие по подсветке
        }

        // Хвост в этом шаге сдвигается, поэтому последняя клетка свободна,
        // если ничего не съедено
        bool grow = willEatFood || willOpenChest;
        for (int i = 0; i < length - (grow ? 0 : 1); ++i) {
            const Cell &c = body[(headIdx - i + GRID*GRID) % (GRID*GRID)];
            if (c.x == next.x && c.y == next.y) { gameOver = true; lastEvent = 4; return; }
        }

        // Двигаем голову (кольцевой буфер)
        headIdx = (headIdx + 1) % (GRID*GRID);
        body[headIdx] = next;

        lastEvent = 0;
        if (willEatFood) { ++length; ++score; lastEvent = 1; spawnFood(); }
        if (willOpenChest) {
            // Сундук: +2 очка и +1 к длине
            chestOpened[chestIdx] = true;
            score += 2; ++length; ++chestsCollected;
            lastEvent = 2;
            // Открытый сундук гаснет во всех своих проявлениях
            if (glowChest == chestIdx) { glowChest = -1; glowLeft = 0; }
            // Если видимый сундук открыт — сразу показать следующий живой
            advanceVisibleChest(true);
            if (chestsCollected == NUM_CHESTS) { win = true; lastEvent = 5; return; }
        }

        // Мигание: каждые BLINK_STEPS шагов видимость переходит к след. сундуку.
        // Сундук, который только что перестал мигать, получает «послесвечение»
        // на HIGHLIGHT_STEPS шагов — его клетку ещё можно забрать.
        if (++blinkTimer >= BLINK_STEPS) {
            blinkTimer = 0;
            int was = visibleChest;
            advanceVisibleChest(false);
            if (was >= 0 && !chestOpened[was]) { glowChest = was; glowLeft = HIGHLIGHT_STEPS; }
        }
        // Тик подсветки: гаснет через HIGHLIGHT_STEPS шагов
        if (glowLeft > 0 && --glowLeft == 0) glowChest = -1;
    }

    // Перевести «видимость» на следующий неоткрытый сундук (круговой обход)
    void advanceVisibleChest(bool immediate) {
        for (int k = 1; k <= NUM_CHESTS; ++k) {
            int cand = (visibleChest + k) % NUM_CHESTS;
            if (cand >= 0 && cand < NUM_CHESTS && !chestOpened[cand]) {
                visibleChest = cand;
                if (immediate) blinkTimer = 0;
                return;
            }
        }
        visibleChest = -1; // все открыты — мигать больше нечему
    }

    // Появление новой ягоды на случайной СВОБОДНОЙ клетке
    void spawnFood() {
        Cell freeCells[GRID*GRID];
        int n = 0;
        for (int y = 0; y < GRID; ++y)
            for (int x = 0; x < GRID; ++x) {
                bool taken = isBody(x, y);
                // на месте ещё не открытых сундуков ягода не появляется
                for (int i = 0; i < NUM_CHESTS && !taken; ++i)
                    if (!chestOpened[i] && chests[i].x == x && chests[i].y == y) taken = true;
                if (!taken) freeCells[n++] = { x, y };
            }
        if (n == 0) { gameOver = true; lastEvent = 3; return; } // поле заполнено
        food = freeCells[rand() % n];
    }

    // Смена направления с запретом разворота на 180 градусов
    void turn(Dir d) {
        if ((d == DIR_UP && dir == DIR_DOWN) || (d == DIR_DOWN && dir == DIR_UP) ||
            (d == DIR_LEFT && dir == DIR_RIGHT) || (d == DIR_RIGHT && dir == DIR_LEFT))
            return;
        dir = d;
    }
};

// ------------------------------- Отрисовка ----------------------------------
// Рисуем сундук: ЖЁЛТЫЙ корпус + крышка + белый замок + светлая рамка
static void drawChest(SDL_Renderer *r, int px, int py) {
    SDL_Rect box = { px + 8, py + 14, CELL - 16, CELL - 22 };
    SDL_SetRenderDrawColor(r, COL_CHEST.r, COL_CHEST.g, COL_CHEST.b, 255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, COL_GOLD.r, COL_GOLD.g, COL_GOLD.b, 255);
    SDL_RenderDrawRect(r, &box);
    // крышка
    SDL_Rect lid = { px + 8, py + 14, CELL - 16, 8 };
    SDL_SetRenderDrawColor(r, 210, 160, 0, 255);
    SDL_RenderFillRect(r, &lid);
    // замок
    SDL_Rect lock = { px + CELL/2 - 3, py + 22, 6, 8 };
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderFillRect(r, &lock);
}

static void drawGame(SDL_Renderer *r, const Game &g) {
    // Фон
    SDL_SetRenderDrawColor(r, COL_BG.r, COL_BG.g, COL_BG.b, COL_BG.a);
    SDL_RenderClear(r);

    // Верхняя панель: счёт, прогресс по сундукам, подсказка клавиш
    SDL_SetRenderDrawColor(r, COL_PANEL.r, COL_PANEL.g, COL_PANEL.b, COL_PANEL.a);
    SDL_Rect panel = {0, 0, WIN_W, TOPBAR};
    SDL_RenderFillRect(r, &panel);
    char buf[64];
    snprintf(buf, sizeof(buf), "SCORE: %d", g.score);
    drawText(r, 12, 8, buf, COL_TEXT, 3);
    snprintf(buf, sizeof(buf), "CHESTS: %d/%d", g.chestsCollected, NUM_CHESTS);
    drawText(r, WIN_W - textWidth(buf, 2) - 12, 14, buf, COL_CHEST, 2);
    drawText(r, INT_MIN, 38, "H HELP   P PAUSE   R RESTART   ESC QUIT", COL_TEXT, 1);

    // Сетка поля
    SDL_SetRenderDrawColor(r, COL_GRID.r, COL_GRID.g, COL_GRID.b, COL_GRID.a);
    for (int i = 0; i <= GRID; ++i) {
        SDL_RenderDrawLine(r, i*CELL, TOPBAR, i*CELL, TOPBAR + GRID*CELL);
        SDL_RenderDrawLine(r, 0, TOPBAR + i*CELL, WIN_W, TOPBAR + i*CELL);
    }

    // Ягоды (красные) — видны ВСЕГДА, появляются вместе с сундуками
    SDL_SetRenderDrawColor(r, COL_FOOD.r, COL_FOOD.g, COL_FOOD.b, COL_FOOD.a);
    SDL_Rect fd = { g.food.x*CELL+6, TOPBAR + g.food.y*CELL+6, CELL-12, CELL-12 };
    SDL_RenderFillRect(r, &fd);

    // Сундуки: в каждый момент виден только ОДИН (мигает каждые BLINK_STEPS шагов)
    if (g.visibleChest >= 0 && g.visibleChest < NUM_CHESTS &&
        !g.chestOpened[g.visibleChest]) {
        const Cell &c = g.chests[g.visibleChest];
        drawChest(r, c.x*CELL, TOPBAR + c.y*CELL);
    }

    // Змейка: голова ярче туловища
    for (int i = 0; i < g.length; ++i) {
        const Cell &c = g.body[(g.headIdx - i + GRID*GRID) % (GRID*GRID)];
        SDL_Color col = (i == 0) ? COL_HEAD : COL_SNAKE;
        SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
        SDL_Rect rc = { c.x*CELL+2, TOPBAR + c.y*CELL+2, CELL-4, CELL-4 };
        SDL_RenderFillRect(r, &rc);
    }

    // Оверлей справки (клавиша H; также показывается до начала игры)
    if (g.showHelp) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
        SDL_Rect box = { 30, 60, WIN_W-60, WIN_H-120 };
        SDL_RenderFillRect(r, &box);
        int y = 76;
        drawText(r, INT_MIN, y, "SNAKE - CHEST HUNT - HELP", COL_CHEST, 2); y += 34;
        drawText(r, INT_MIN, y, "GOAL: FIND AND OPEN ALL 5 CHESTS.", COL_TEXT, 1); y += 14;
        drawText(r, INT_MIN, y, "CHESTS ARE HIDDEN: ONLY ONE IS", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "VISIBLE AT A TIME (BLINKS EVERY 10 STEPS).", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "OPEN CHEST = STEP ON THE VISIBLE GOLDEN BOX.", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "CHEST: +2 SCORE, +1 LENGTH.", COL_CHEST, 1); y += 12;
        drawText(r, INT_MIN, y, "BERRY (RED): +1 SCORE, +1 LENGTH.", COL_FOOD, 1); y += 20;
        drawText(r, INT_MIN, y, "LOSE IF YOU HIT A WALL OR YOUR TAIL.", COL_TEXT, 1); y += 20;
        drawText(r, INT_MIN, y, "CONTROLS:", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "ARROWS / W A S D - MOVE", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "H - THIS HELP   P - PAUSE   R - RESTART", COL_TEXT, 1); y += 12;
        drawText(r, INT_MIN, y, "ESC - QUIT", COL_TEXT, 1); y += 16;
        drawText(r, INT_MIN, y, "PRESS ANY KEY TO START", COL_GOLD, 1);
    }

    // Экран проигрыша с итоговым счётом
    if (g.gameOver) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
        SDL_Rect box = { 0, TOPBAR, WIN_W, GRID*CELL };
        SDL_RenderFillRect(r, &box);
        const char *cause = (g.lastEvent == 4) ? "YOU BIT YOUR TAIL!" : "YOU HIT THE WALL!";
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 - 56, "GAME OVER!", COL_FOOD, 3);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 - 8, cause, COL_TEXT, 1);
        char sc[64]; snprintf(sc, sizeof(sc), "FINAL SCORE: %d  CHESTS: %d/%d",
                              g.score, g.chestsCollected, NUM_CHESTS);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 + 14, sc, COL_TEXT, 2);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 + 50, "PRESS R TO RESTART", COL_TEXT, 1);
    }

    // Экран победы
    if (g.win) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
        SDL_Rect box = { 0, TOPBAR, WIN_W, GRID*CELL };
        SDL_RenderFillRect(r, &box);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 - 56, "YOU WIN!", COL_CHEST, 3);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 - 8, "ALL 5 CHESTS FOUND!", COL_GOLD, 1);
        char sc[64]; snprintf(sc, sizeof(sc), "FINAL SCORE: %d", g.score);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 + 14, sc, COL_TEXT, 2);
        drawText(r, INT_MIN, TOPBAR + GRID*CELL/2 + 50, "PRESS R TO PLAY AGAIN", COL_TEXT, 1);
    }
}

// --------------------------------- main -------------------------------------
int runSdlApplication(int, char*[]) {
    srand((unsigned)time(nullptr));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        fprintf(stderr, "Проверьте, что установлен libsdl2-dev и есть дисплей"
                        " (в WSL2 нужен X/Wayland-сервер, например VcXsrv).\n");
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("Snake C++ (SDL2) - Chest Hunt",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED |
                                                      SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "CreateWindow/Renderer failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    Game game;
    game.showHelp = true;   // справка показывается перед началом игры
    bool running = true;
    Uint32 lastTick = SDL_GetTicks();

    // Главный игровой цикл
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_h: game.showHelp = !game.showHelp; break;
                    case SDLK_p: game.paused = !game.paused; break;
                    case SDLK_r: game.reset(); break;
                    case SDLK_UP: case SDLK_w:    game.turn(DIR_UP);    break;
                    case SDLK_DOWN: case SDLK_s:  game.turn(DIR_DOWN);  break;
                    case SDLK_LEFT: case SDLK_a:  game.turn(DIR_LEFT);  break;
                    case SDLK_RIGHT: case SDLK_d: game.turn(DIR_RIGHT); break;
                    default: break;
                }
                // любое нажатие скрывает стартовую справку (игра начинается)
                if (game.showHelp && e.key.keysym.sym != SDLK_h)
                    game.showHelp = false;
            }
        }

        // Пока открыта справка — время стоит (игра ждёт старта)
        bool stepping = !game.showHelp;

        Uint32 now = SDL_GetTicks();
        if (stepping && now - lastTick >= (Uint32)FPS_MS) {
            game.step();
            lastTick = now;
        }
        if (!stepping) lastTick = now; // не «догонять» после закрытия справки

        drawGame(ren, game);
        SDL_RenderPresent(ren);
        SDL_Delay(5); // чтобы не грузить CPU
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

}  // namespace snake
