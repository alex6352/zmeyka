// ============================================================================
//  Домашнее задание 1 — «Змейка на C++» (вариант задания: «Сундуки»)
//  Среда: VS Code + WSL (Linux). Компиляция:
//      g++ -std=c++17 -O2 -Wall -o snake snake.cpp
//  Запуск:  ./snake
//
//  Отличие от базовой программы (творческое задание):
//    - На поле спрятаны 5 сундуков ('$', ЖЁЛТЫЙ цвет). Сундуки появляются
//      вместе с обычной едой ('*', КРАСНЫЙ цвет) сразу с первого кадра.
//      «Спрятаны» — они поочерёдно мигают: в каждый момент времени видим
//      только один сундук (чередуются каждые несколько кадров), остальные
//      невидимы до следующего мерцания.
//    - Собрать сундук = в него въехать головой. Каждый сундук даёт +2 очка
//      и +1 к длине. Обычная еда даёт +1 очко и +1 к длине.
//    - Цель игры — найти и открыть все 5 сундуков (победа). Проигрыш — как
//      в базе: столкновение со стеной или собственным хвостом.
//    - Используется ANSI-цвет: жёлтые сундуки, красная еда, зелёная змейка.
//
//  Управление:
//      W / S / A / D  или стрелки — движение змейки
//      H              — вызвать справку (правила игры) в любой момент
//      Q              — выйти из игры
// ============================================================================

#include "snake.h"

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cmath>      // abs (для бота автотеста)
#include <ctime>
#include <unistd.h>   // read(), usleep()
#include <termios.h>  // настройка терминала (безэховый режим)
#ifdef SNAKE_TESTING
#include <sstream>
#include <cstring>
#include <climits>
#endif

namespace snake {

using std::cout;
using std::setw;
using std::ostringstream;
using std::streambuf;

// ----------------------------- Константы игры -------------------------------
const int FIELD_SIZE = 10;            // поле 10x10
const int MAX_LEN    = FIELD_SIZE * FIELD_SIZE; // максимум клеток (потолок длины)
const int DELAY_US   = 250000;        // задержка между ходами, микросекунды (0.25 с)

// --------------------------- Параметры варианта «Сундуки» --------------------
const int CHEST_COUNT     = 5;    // всего сундуков на поле
const int CHEST_SCORE     = 2;    // очков за один открытый сундук
const int CHEST_GROW      = 1;    // сегментов длины за один открытый сундук
const int BLINK_EVERY     = 10;   // мигание: каждые N ходов подсвечивается новый сундук
const int HIGHLIGHT_STEPS = 20;   // подсветка («послесвечение») сундука после мигания, ходов
                                  // в это время сундук НЕ виден, но его клетку можно забрать

// ----------------------------- ANSI-цвета -----------------------------------
// Терминал раскрашивает символы escape-последовательностями \033[...m.
// Сундуки — жёлтые, еда — красная, змейка — зелёная (по условию варианта).
const char *YC = "\033[1;33m";   // жёлтый яркого тона (сундуки '$')
const char *WC = "\033[0;33m";   // тёмно-жёлтый (подсветка «послесвечения» сундука)
const char *RC = "\033[1;31m";   // красный (еда '*')
const char *GC = "\033[1;32m";   // зелёный (змейка 'O'/'o')
const char *CC = "\033[1;36m";   // голубой (служебные надписи)
const char *RS = "\033[0m";      // сброс цвета

// Координаты клетки поля
struct Point {
    int x; // строка
    int y; // столбец
};

// Направления движения (индексы массива dirX/dirY)
enum Dir { DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3 };

// Векторы смещения для каждого направления
const int dirX[4] = {-1, 1, 0, 0};
const int dirY[4] = { 0, 0,-1, 1};

// --------------------------------- Терминал ---------------------------------
// Включаем режим без эха и посимвольного ввода (чтобы клавиши читались сразу)
void setupTerminal(termios &oldTio) {
    tcgetattr(STDIN_FILENO, &oldTio);          // сохраняем текущие настройки
    termios newTio = oldTio;
    newTio.c_lflag &= ~(ICANON | ECHO);        // выключаем канонический режим и эхо
    newTio.c_cc[VMIN]  = 0;                    // read не блокируется
    newTio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newTio); // применяем новые настройки
}

// Возвращаем исходные настройки терминала (обязательно перед завершением!)
void restoreTerminal(const termios &oldTio) {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTio);
}

// ---------------------------------- Справка ----------------------------------
// Краткое описание цели игры, правил и клавиш управления.
// Вызывается перед началом игры и по клавише H во время игры.
void printHelp() {
    cout << "\n=================== СПРАВКА: ЗМЕЙКА-СОКРОВИЩА ===================\n"
         << "Цель игры: найти и открыть все 5 сундуков, спрятанных на поле.\n"
         << "Каждый открытый сундук даёт +2 очка и +1 к длине змейки.\n"
         << "Обычная еда даёт +1 очко и +1 к длине.\n\n"
         << "Обозначения на поле:\n"
         << "  " << YC << "$" << RS << "  — сундук (жёлтый, МИГАЕТ). Сундуки СПРЯТАНЫ: они поочерёдно\n"
         << "     мигают — в каждый момент виден только один. Въедешь в него\n"
         << "     головой, пока он мигает — откроешь сразу.\n"
         << "  " << WC << "#" << RS << "  — «послесвечение»: сундук перестал мигать, но его клетка\n"
         << "     ещё подсвечена " << HIGHLIGHT_STEPS << " ходов — за неё тоже можно забрать сундук!\n"
         << "  " << RC << "*" << RS << "  — ягода/еда (красная). Появляется сразу вместе с\n"
         << "     сундуками и после каждой съеденной ягоды.\n"
         << "  " << GC << "O/o" << RS << "  — голова/тело змейки (зелёная).\n\n"
         << "Правила:\n"
         << "  - Поле 10x10, змейка стартует из центра поля длиной 1.\n"
         << "  - Еда и 5 сундуков появляются случайно на свободных клетках\n"
         << "    с первого кадра игры.\n"
         << "  - Мигание сундуков переключается каждые " << BLINK_EVERY << " хода.\n"
         << "  - После мигания клетка сундука подсвечена ещё " << HIGHLIGHT_STEPS
         << " ходов: сундук можно\n    забрать и по «послесвечению» (символ '" << WC << "#" << RS << "'), даже когда он не виден.\n"
         << "  - Открыто всех сундуков: 5 из 5 = ПОБЕДА.\n"
         << "  - Столкновение со стеной или собственным хвостом = проигрыш.\n"
         << "  - Нельзя развернуться на 180 градусов.\n\n"
         << "Клавиши управления:\n"
         << "  W или стрелка вверх    — движение вверх\n"
         << "  S или стрелка вниз     — движение вниз\n"
         << "  A или стрелка влево    — движение влево\n"
         << "  D или стрелка вправо   — движение вправо\n"
         << "  H                      — показать эту справку\n"
         << "  Q                      — выйти из игры\n"
         << "=================================================================\n";
}

// ------------------------------ Чтение ввода --------------------------------
// Читает нажатую клавишу (неблокирующе). Поддерживает WASD, стрелки, H, Q.
// Возвращает код направления, либо -1 (ничего/ошибка), либо специальные коды:
//   HELP_CODE = 100, EXIT_CODE = 101
const int HELP_CODE = 100;
const int EXIT_CODE = 101;

int readKey() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1; // нет ввода

    if (c == 'w' || c == 'W') return DIR_UP;
    if (c == 's' || c == 'S') return DIR_DOWN;
    if (c == 'a' || c == 'A') return DIR_LEFT;
    if (c == 'd' || c == 'D') return DIR_RIGHT;
    if (c == 'h' || c == 'H') return HELP_CODE;
    if (c == 'q' || c == 'Q') return EXIT_CODE;

    // Экранированные последовательности стрелок: ESC [ A/B/C/D
    if (c == '\x1b') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) == 1 &&
            read(STDIN_FILENO, &seq[1], 1) == 1 && seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return DIR_UP;
                case 'B': return DIR_DOWN;
                case 'C': return DIR_RIGHT;
                case 'D': return DIR_LEFT;
            }
        }
    }
    return -1; // неизвестный символ — игнорируем
}

// ------------------------------- Отрисовка ----------------------------------
// Печатает игровое поле. Массивы тела змейки передаются как указатели
// (использование массивов: body[] — кольцевой буфер, grid[][] — карта поля,
//  chests[]/chestTaken[] — массив координат сундуков и их статусов).
// visibleChest — индекс сундука, который «мигает» (виден) в этом кадре.
// highlightedChest — клетка сундука с «послесвечением»: сам сундук уже не
// мигает ('$' не рисуется), но клетка подсвечена символом '#' тёмно-жёлтым
// цветом — по ней тоже можно забрать сундук (условие варианта).
// Клетки раскрашиваются ANSI-кодами: '$' — жёлтый, '*' — красный, змейка —
// зелёный. Перед каждым кадром экран полностью очищается (\033[2J\033[H),
// поэтому игра выглядит как живая анимация, а не поток текста.
void drawField(const Point *body, int headIdx, int length, const Point &food,
               int score, const Point *chests, const bool *chestTaken,
               int visibleChest, bool blinkOn, int collected,
               int highlightedChest = -1) {
    char grid[FIELD_SIZE][FIELD_SIZE]; // вспомогательная карта для отрисовки

    // 1. пустое поле
    for (int i = 0; i < FIELD_SIZE; ++i)
        for (int j = 0; j < FIELD_SIZE; ++j)
            grid[i][j] = '.';

    // 2. размечаем еду ('*' — красная ягода)
    if (food.x >= 0) grid[food.x][food.y] = '*';

    // 3. размечаем ВИДИМЫЙ (мигающий) сундук — остальные спрятаны
    if (visibleChest >= 0 && blinkOn && !chestTaken[visibleChest])
        grid[chests[visibleChest].x][chests[visibleChest].y] = '$';

    // 3b. «послесвечение» — клетка сундука после окончания мигания
    if (highlightedChest >= 0 && !chestTaken[highlightedChest] &&
        grid[chests[highlightedChest].x][chests[highlightedChest].y] == '.')
        grid[chests[highlightedChest].x][chests[highlightedChest].y] = '#';

    // 4. размечаем тело змейки (голова — 'O', хвост — 'o')
    for (int k = 0; k < length; ++k) {
        int idx = (headIdx - k + MAX_LEN * 2) % MAX_LEN; // идём от головы к хвосту
        grid[body[idx].x][body[idx].y] = (k == 0) ? 'O' : 'o';
    }

    // 5. выводим поле с координатной сеткой (с очисткой экрана)
    cout << "\033[2J\033[H";
    cout << "  Юг (S / вниз)\n";
    cout << "    ";
    for (int j = 0; j < FIELD_SIZE; ++j) cout << j;
    cout << "\n";
    for (int i = 0; i < FIELD_SIZE; ++i) {
        cout << setw(2) << i << " ";
        for (int j = 0; j < FIELD_SIZE; ++j) {
            char c = grid[i][j];
            switch (c) {
                case '$': cout << YC << "$" << RS; break;  // жёлтый сундук (мигает)
                case '#': cout << WC << "#" << RS; break;  // «послесвечение» сундука
                case '*': cout << RC << "*" << RS; break;  // красная еда
                case 'O': cout << GC << "O" << RS; break;  // голова змейки
                case 'o': cout << GC << "o" << RS; break;  // тело змейки
                default:  cout << CC << "." << RS; break;  // пустая клетка
            }
            cout << ' ';
        }
        if (i == 0) cout << CC << " <- Север (W / вверх)" << RS;
        cout << "\n";
    }
    cout << CC << "Запад (A)                          Восток (D)" << RS << "\n";
    cout << "Счёт: " << score
         << "   Длина: " << length
         << "   Сундуки: " << collected << "/" << CHEST_COUNT;
    if (blinkOn && visibleChest >= 0)
        cout << "   [" << YC << "виден сундук #" << (visibleChest + 1) << " '$'" << RS << "]";
    else if (highlightedChest >= 0 && !chestTaken[highlightedChest])
        cout << "   [" << WC << "сундук #" << (highlightedChest + 1)
             << " подсвечен '" << WC << "#" << RS << "' — ещё можно забрать!" << RS << "]";
    cout << "   [H — справка, Q — выход]\n";
}

// --------------------------- Появление новой еды ----------------------------
// Ищем все свободные клетки и выбираем случайную (еда никогда не появляется
// на теле змейки или на ещё не открытых сундуках). Используется массив
// свободных клеток freeCells[].
Point spawnFood(const Point *body, int headIdx, int length,
                const Point *chests, const bool *chestTaken) {
    Point freeCells[MAX_LEN];
    int freeCount = 0;

    // Карта занятых клеток
    bool taken[FIELD_SIZE][FIELD_SIZE] = {};
    for (int k = 0; k < length; ++k) {
        int idx = (headIdx - k + MAX_LEN * 2) % MAX_LEN;
        taken[body[idx].x][body[idx].y] = true;
    }
    // Сундуки тоже считаются занятыми клетками, пока не открыты
    for (int c = 0; c < CHEST_COUNT; ++c)
        if (!chestTaken[c]) taken[chests[c].x][chests[c].y] = true;

    for (int i = 0; i < FIELD_SIZE; ++i)
        for (int j = 0; j < FIELD_SIZE; ++j)
            if (!taken[i][j])
                freeCells[freeCount++] = Point{i, j};

    // Если свободных клеток нет — поле заполнено (защита от выхода за границы)
    if (freeCount == 0) return Point{-1, -1};

    return freeCells[rand() % freeCount];
}

// ------------------------- Расстановка сундуков ------------------------------
// 5 сундуков расставляются случайно на разных свободных клетках (не в центре,
// где стартует змейка). Координаты складываются в массив chests[].
void placeChests(Point *chests, const Point &start) {
    int placed = 0;
    while (placed < CHEST_COUNT) {
        Point p{ rand() % FIELD_SIZE, rand() % FIELD_SIZE };
        if (p.x == start.x && p.y == start.y) continue; // не на старте змейки
        bool dup = false;
        for (int i = 0; i < placed; ++i)              // не на уже поставленный
            if (chests[i].x == p.x && chests[i].y == p.y) { dup = true; break; }
        if (!dup) chests[placed++] = p;
    }
}

#ifdef SNAKE_TESTING
static bool g_testMode = false;       // режим автотестов: без usleep
#else
static const bool g_testMode = false; // в обычной сборке задержки всегда включены
#endif

// ------------------------------ Основная игра -------------------------------
// Реализует цикл: ввод -> ход -> проверка столкновений -> отрисовка.
// Змейка хранится в массиве-«кольцевом буфере» body[] (голова двигается,
// хвост остаётся, при росте просто не сдвигаем индекс хвоста).
bool runGame() {
    Point body[MAX_LEN];       // массив сегментов тела змейки
    int headIdx = 0;           // индекс головы в кольцевом буфере
    int length = 1;            // текущая длина змейки
    int score  = 0;            // счёт игрока

    // Змейка стартует в центре поля, направление — вправо
    Point start = {FIELD_SIZE / 2, FIELD_SIZE / 2};
    body[0] = start;
    Dir dir = DIR_RIGHT;

    // --- Вариант задания: расставляем 5 спрятанных сундуков ---
    Point chests[CHEST_COUNT];              // массив координат сундуков
    bool chestTaken[CHEST_COUNT] = {};      // массив статусов: открыт ли сундук
    placeChests(chests, start);
    int collected = 0;                      // сколько сундуков уже открыто
    int visibleChest = 0;                   // какой сундук «мигает» (виден) —
                                            // с ПЕРВОГО кадра показываем #1
    int movesSinceBlink = 0;                // счётчик ходов для мигания
    int highlightedChest = -1;              // сундук с «послесвечением»: он уже
                                            // перестал мигать, но его клетку ещё
                                            // можно забрать (HIGHLIGHT_STEPS ходов)
    int highlightLeft = 0;                  // сколько ходов осталось подсветке

    Point food = spawnFood(body, headIdx, length, chests, chestTaken);

    bool gameOver = false;
    bool won = false;

    while (!gameOver) {
        // Мигание: каждые BLINK_EVERY хода выбираем следующий НЕОТКРЫТЫЙ
        // сундук и показываем его на несколько кадров (остальные скрыты).
        // Сундук, который ТОЛЬКО что перестал мигать, получает «послесвечение»
        // на HIGHLIGHT_STEPS ходов — по его клетке всё ещё можно забрать приз.
        if (movesSinceBlink >= BLINK_EVERY) {
            movesSinceBlink = 0;
            if (visibleChest >= 0 && !chestTaken[visibleChest]) {
                highlightedChest = visibleChest;   // запускаем послесвечение
                highlightLeft    = HIGHLIGHT_STEPS;
            }
            int next = visibleChest;
            for (int attempt = 0; attempt < CHEST_COUNT; ++attempt) {
                next = (next + 1) % CHEST_COUNT;   // идём по кругу массива chests[]
                if (!chestTaken[next]) break;      // пропускаем уже открытые
            }
            if (collected < CHEST_COUNT && !chestTaken[next]) visibleChest = next;
            else visibleChest = -1;                // все открыты — мигать нечему
        }
        // Половину цикла мигания сундук виден ('$'), половину — нет.
        bool blinkOn = (visibleChest >= 0) && (movesSinceBlink % 2 == 0);

        drawField(body, headIdx, length, food, score,
                  chests, chestTaken, visibleChest, blinkOn, collected,
                  highlightedChest);

        // --- Опрос ввода до следующего хода ---
        int nextDir = -1;
        bool exitRequested = false;
        while (true) {
            int key = readKey();
            if (key == -1) break;             // новых нажатий нет
            if (key == HELP_CODE) {
                printHelp();                  // справка по клавише H
                continue;
            }
            if (key == EXIT_CODE) {           // выход по клавише Q
                exitRequested = true;
                break;
            }
            nextDir = key;                    // направление движения
        }
        if (exitRequested) {
            cout << "\nИгра прервана по клавише Q. Счёт: " << score << "\n";
            return true;
        }

        // Разворот на 180 градусов запрещён (иначе змейка сразу в себя врежется)
        if (nextDir != -1) {
            bool opposite = (nextDir == DIR_UP   && dir == DIR_DOWN) ||
                            (nextDir == DIR_DOWN && dir == DIR_UP)   ||
                            (nextDir == DIR_LEFT && dir == DIR_RIGHT)||
                            (nextDir == DIR_RIGHT&& dir == DIR_LEFT);
            if (!opposite) dir = static_cast<Dir>(nextDir);
        }

        // --- Шаг змейки: вычисляем новую голову ---
        Point newHead{body[headIdx].x + dirX[dir], body[headIdx].y + dirY[dir]};

        // Проверка столкновения со стеной
        if (newHead.x < 0 || newHead.x >= FIELD_SIZE ||
            newHead.y < 0 || newHead.y >= FIELD_SIZE) {
            gameOver = true;
            cout << "\n*** СТОЛКНОВЕНИЕ СО СТЕНОЙ! Игра окончена. ***\n";
        }

        // Проверка столкновения с собственным хвостом
        // (хвост сдвинется, если еда не съедена — его трогать можно)
        if (!gameOver) {
            bool ate = (newHead.x == food.x && newHead.y == food.y);
            int checkLen = ate ? length : length - 1; // без последней клетки хвоста
            for (int k = 0; k < checkLen; ++k) {
                int idx = (headIdx - k + MAX_LEN * 2) % MAX_LEN;
                if (body[idx].x == newHead.x && body[idx].y == newHead.y) {
                    gameOver = true;
                    cout << "\n*** СТОЛКНОВЕНИЕ С ХВОСТОМ! Игра окончена. ***\n";
                    break;
                }
            }
        }

        if (!gameOver) {
            // Двигаем голову в кольцевом буфере
            headIdx = (headIdx + 1) % MAX_LEN;
            body[headIdx] = newHead;
            ++movesSinceBlink; // ход сделан — учитываем для мигания сундуков

            // --- Сундук: голова оказалась на клетке сундука ---
            // Условие варианта: сундук открывается, если въехать в него
            // головой, пока он МИГАЕТ ('$'), ИЛИ по его «послесвечению» ('#') —
            // клетке, подсвеченной HIGHLIGHT_STEPS ходов после мигания.
            // Награда: +2 очка и +1 к длине.
            bool gotChest = false;
            int openIdx = -1;                        // какой сундук открыли
            if (visibleChest >= 0 && blinkOn && !chestTaken[visibleChest] &&
                newHead.x == chests[visibleChest].x &&
                newHead.y == chests[visibleChest].y) {
                openIdx = visibleChest;              // взятие «на глазах»
            } else if (highlightedChest >= 0 && highlightLeft > 0 &&
                       !chestTaken[highlightedChest] &&
                       newHead.x == chests[highlightedChest].x &&
                       newHead.y == chests[highlightedChest].y) {
                openIdx = highlightedChest;          // взятие по послесвечению
            }
            if (openIdx >= 0) {
                chestTaken[openIdx] = true;          // помечаем сундук открытым
                ++collected;
                score  += CHEST_SCORE;               // +2 очка
                length += CHEST_GROW;                // +1 к длине
                gotChest = true;
                cout << "\n*** СУНДУК #" << (openIdx + 1)
                     << (openIdx == highlightedChest && !blinkOn ?
                         " (ПО ПОСЛЕСВЕЧЕНИЮ)" : "")
                     << " ОТКРЫТ! +"
                     << CHEST_SCORE << " очка, длина +1. Всего собрано: "
                     << collected << " из " << CHEST_COUNT << ". ***\n";
                if (!g_testMode) usleep(DELAY_US);   // пауза, чтобы прочитать
                if (collected >= CHEST_COUNT) {      // цель варианта достигнута
                    won = true;
                    gameOver = true;
                    cout << "\n*** ПОБЕДА! Все " << CHEST_COUNT
                         << " сундуков найдены и открыты! ***\n";
                }
                // Открытый сундук больше не мигает и не подсвечивается
                if (visibleChest == openIdx) { visibleChest = -1; movesSinceBlink = BLINK_EVERY; }
                if (highlightedChest == openIdx) { highlightedChest = -1; highlightLeft = 0; }
            }

            // Тик «послесвечения»: подсветка клетки сундука гаснет через
            // HIGHLIGHT_STEPS ходов (или раньше, если сундук уже открыт).
            if (highlightLeft > 0 && --highlightLeft == 0)
                highlightedChest = -1;

            if (!gameOver && (newHead.x == food.x && newHead.y == food.y)) {
                // Еда съедена: длина +1, счёт +1, новая еда
                ++length;
                ++score;
                food = spawnFood(body, headIdx, length, chests, chestTaken);
                if (food.x == -1) { // поле полностью заполнено
                    won = true;
                    gameOver = true;
                    cout << "\n*** ПОБЕДА! Поле заполнено полностью! ***\n";
                }
            } else if (!gotChest) {
                // Не съели — хвост «освобождается»: длина не растёт.
                // В кольцевом буфере старые клетки просто перезаписываются,
                // поэтому достаточно не увеличивать length.
            }
            if (!g_testMode) usleep(DELAY_US); // темп игры
        }
    }

    // Финальный кадр и итоговый счёт
    drawField(body, headIdx, length, food, score,
              chests, chestTaken, -1, false, collected);
    cout << "\nИтоговый счёт: " << score << "\n";
    cout << "Открыто сундуков: " << collected << " из " << CHEST_COUNT << "\n";
    if (won) cout << "Вы победили!\n";
    return true;
}

// ----------------------------------- Main -----------------------------------
#ifdef SNAKE_TESTING
// Автотест (только при сборке с -DSNAKE_TESTING): запускает игру на фиксированном
// сиде. Ищет все 5 сундуков по их координатам и печатает результат для проверки.
// Логика мигания/послесвечения здесь зеркалит runGame(), поэтому тест проверяет,
// что цель «собрать 5 сундуков» достижима в новых правилах.
int runAutoTest(int seed) {
    srand(seed);
    g_testMode = true;

    // Перехватываем вывод в память
    std::ostringstream cap;
    std::streambuf *old = cout.rdbuf(cap.rdbuf());

    bool win = false;
    int tries = 0;
    while (!win && tries < 40) {
        ++tries;
        // Сброс: новый запуск игры с текущим rand()
        Point body[MAX_LEN]; int headIdx = 0, length = 1, score = 0;
        Point start{FIELD_SIZE/2, FIELD_SIZE/2};
        body[0] = start; Dir dir = DIR_RIGHT;
        Point chests[CHEST_COUNT]; bool chestTaken[CHEST_COUNT] = {};
        placeChests(chests, start);
        int collected = 0, visibleChest = 0, movesSinceBlink = 0;
        int highlightedChest = -1, highlightLeft = 0;   // послесвечение (как в игре)
        Point food = spawnFood(body, headIdx, length, chests, chestTaken);
        bool gameOver = false;

        int guard = 0;
        while (!gameOver && guard++ < 5000) {
            if (movesSinceBlink >= BLINK_EVERY) {
                movesSinceBlink = 0;
                if (visibleChest >= 0 && !chestTaken[visibleChest]) {
                    highlightedChest = visibleChest;
                    highlightLeft    = HIGHLIGHT_STEPS;
                }
                int next = visibleChest;
                for (int a = 0; a < CHEST_COUNT; ++a) { next = (next + 1) % CHEST_COUNT; if (!chestTaken[next]) break; }
                visibleChest = (collected < CHEST_COUNT && !chestTaken[next]) ? next : -1;
            }
            bool blinkOn = (visibleChest >= 0) && (movesSinceBlink % 2 == 0);

            // --- Выбор направления ботом ---
            // Цель: ближайший доступный сундук — либо мигающий (видимый),
            // либо подсвеченный «послесвечением» (его можно забрать сразу).
            int target = visibleChest;                  // приоритет: мигающий
            if (target < 0 || chestTaken[target]) target = highlightedChest; // затем подсветка
            if (target < 0 || chestTaken[target]) {     // иначе ждём любое мигание
                target = -1;
                for (int c = 0; c < CHEST_COUNT; ++c) if (!chestTaken[c]) { target = c; break; }
            }
            Point head = body[headIdx];
            int bestDir = dir;
            int bestScore = INT_MIN;
            for (int d = 0; d < 4; ++d) {
                if ((d==DIR_UP&&dir==DIR_DOWN)||(d==DIR_DOWN&&dir==DIR_UP)||
                    (d==DIR_LEFT&&dir==DIR_RIGHT)||(d==DIR_RIGHT&&dir==DIR_LEFT)) continue;
                Point nx{head.x + dirX[d], head.y + dirY[d]};
                if (nx.x<0||nx.x>=FIELD_SIZE||nx.y<0||nx.y>=FIELD_SIZE) continue;
                // Нельзя вставать на клетку текущего тела (кроме хвоста, он уйдёт)
                bool hitsBody = false;
                for (int k=0;k<length-1;++k){int idx=(headIdx-k+MAX_LEN*2)%MAX_LEN; if(body[idx].x==nx.x&&body[idx].y==nx.y) hitsBody=true;}
                if (hitsBody) continue;
                // На скрытый (не мигающий и не подсвеченный) сундук не идём —
                // дождаться его видимости дешевле, чем потерять темп
                bool onHiddenChest = false;
                for (int c = 0; c < CHEST_COUNT; ++c)
                    if (!chestTaken[c] && chests[c].x==nx.x && chests[c].y==nx.y) onHiddenChest = true;
                bool grabbableNow =
                    (blinkOn && nx.x==chests[visibleChest].x && nx.y==chests[visibleChest].y) ||
                    (highlightedChest>=0 && highlightLeft>0 &&
                     nx.x==chests[highlightedChest].x && nx.y==chests[highlightedChest].y);
                if (onHiddenChest && !grabbableNow) continue;
                int distNow = abs(head.x-chests[target].x)+abs(head.y-chests[target].y);
                int distNew = abs(nx.x-chests[target].x)+abs(nx.y-chests[target].y);
                int sc = (distNow - distNew) * 100;         // приближаемся к цели
                if (nx.x==food.x && nx.y==food.y) sc += 30; // по пути съесть еду
                if (d == dir) sc += 5;                      // меньше лишних поворотов
                if (sc > bestScore) { bestScore = sc; bestDir = d; }
            }
            dir = (Dir)bestDir;
            Point newHead{body[headIdx].x + dirX[dir], body[headIdx].y + dirY[dir]};
            if (newHead.x<0||newHead.x>=FIELD_SIZE||newHead.y<0||newHead.y>=FIELD_SIZE) { gameOver=true; break; }
            // Сундук открывается при попадании на мигающий ИЛИ на подсвеченный
            bool willChest = false; int openIdx = -1;
            if (visibleChest>=0 && blinkOn && !chestTaken[visibleChest] &&
                newHead.x==chests[visibleChest].x && newHead.y==chests[visibleChest].y) {
                willChest = true; openIdx = visibleChest;
            } else if (highlightedChest>=0 && highlightLeft>0 && !chestTaken[highlightedChest] &&
                newHead.x==chests[highlightedChest].x && newHead.y==chests[highlightedChest].y) {
                willChest = true; openIdx = highlightedChest;
            }
            bool ate = (newHead.x==food.x && newHead.y==food.y);
            if (ate || willChest) { int checkLen = length; for (int k=0;k<checkLen;++k){int idx=(headIdx-k+MAX_LEN*2)%MAX_LEN; if(body[idx].x==newHead.x&&body[idx].y==newHead.y){gameOver=true;break;}} }
            else { int checkLen = length-1; for (int k=0;k<checkLen;++k){int idx=(headIdx-k+MAX_LEN*2)%MAX_LEN; if(body[idx].x==newHead.x&&body[idx].y==newHead.y){gameOver=true;break;}} }
            if (gameOver) break;
            headIdx = (headIdx+1)%MAX_LEN; body[headIdx]=newHead; ++movesSinceBlink;
            if (willChest) {
                chestTaken[openIdx]=true; ++collected; score+=CHEST_SCORE; length+=CHEST_GROW;
                if (collected>=CHEST_COUNT) { win=true; break; }
                if (visibleChest==openIdx) { visibleChest=-1; movesSinceBlink=BLINK_EVERY; }
                if (highlightedChest==openIdx) { highlightedChest=-1; highlightLeft=0; }
            }
            if (highlightLeft>0 && --highlightLeft==0) highlightedChest=-1;
            if (!gameOver && ate) { ++length; ++score; food=spawnFood(body,headIdx,length,chests,chestTaken); if(food.x==-1) break; }
        }
    }

    cout.rdbuf(old);
    printf("AUTOTEST seed=%d tries=%d result=%s\n", seed, tries, win ? "WIN_ALL_CHESTS" : "NO_WIN");
    return win ? 0 : 1;
}
#endif

int runApplication(int argc, char** argv) {
#ifndef SNAKE_TESTING
    (void)argc;
    (void)argv;
#endif
#ifdef SNAKE_TESTING
    if (argc > 1 && strcmp(argv[1], "--autotest") == 0)
        return runAutoTest(argc > 2 ? atoi(argv[2]) : 1);
#endif
    srand(static_cast<unsigned>(time(nullptr))); // сид для rand()

    cout << "Добро пожаловать в игру «Змейка-Сокровища»!\n";
    printHelp(); // обязательная справка перед началом игры

    cout << "\nНажмите любую клавишу (кроме Q) для старта...\n";

    termios oldTio;
    setupTerminal(oldTio);                 // безэховый посимвольный ввод

    // Ждём любое нажатие для старта (Q — сразу выход)
    int key;
    do {
        key = readKey();
        if (key == -1) usleep(50000);
    } while (key == -1);

    bool ok = true;
    if (key == EXIT_CODE) {
        cout << "\nВыход. До встречи!\n";
        ok = false;
    } else {
        ok = runGame();
    }

    restoreTerminal(oldTio);               // обязательно восстанавливаем терминал
    return ok ? 0 : 1;
}

}  // namespace snake
