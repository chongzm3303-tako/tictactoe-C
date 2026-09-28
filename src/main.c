#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_SIZE 600
#define CELL_SIZE   (WINDOW_SIZE / 3)
#define LINE_THICK  6
#define MARK_THICK  10
#define PADDING     35
#define AI_DELAY_MS 400

typedef enum {
    STATE_SIDE_SELECT,
    STATE_PLAYING
} GameState;

/* board values: 0 = empty, 1 = X, 2 = O */
int board[3][3];
int current_player = 1;
int game_over = 0;
GameState state = STATE_SIDE_SELECT;

int human_player = 0; /* 1 = X, 2 = O -- set once the player picks a side */
int ai_player = 0;
int ai_thinking = 0;
Uint32 ai_move_at = 0;

/* ---------- drawing helpers ---------- */

void draw_thick_line(SDL_Renderer* r, int x1, int y1, int x2, int y2, int thickness) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double len = sqrt(dx * dx + dy * dy);
    if (len == 0) return;
    double nx = -dy / len;
    double ny = dx / len;

    for (int i = -thickness / 2; i <= thickness / 2; i++) {
        int ox = (int)round(nx * i);
        int oy = (int)round(ny * i);
        SDL_RenderDrawLine(r, x1 + ox, y1 + oy, x2 + ox, y2 + oy);
    }
}

void draw_circle(SDL_Renderer* r, int cx, int cy, int radius) {
    int x = radius, y = 0, err = 0;
    while (x >= y) {
        SDL_RenderDrawPoint(r, cx + x, cy + y);
        SDL_RenderDrawPoint(r, cx + y, cy + x);
        SDL_RenderDrawPoint(r, cx - y, cy + x);
        SDL_RenderDrawPoint(r, cx - x, cy + y);
        SDL_RenderDrawPoint(r, cx - x, cy - y);
        SDL_RenderDrawPoint(r, cx - y, cy - x);
        SDL_RenderDrawPoint(r, cx + y, cy - x);
        SDL_RenderDrawPoint(r, cx + x, cy - y);
        if (err <= 0) { y++; err += 2 * y + 1; }
        if (err > 0) { x--; err -= 2 * x + 1; }
    }
}

void draw_thick_circle(SDL_Renderer* r, int cx, int cy, int radius, int thickness) {
    for (int rad = radius - thickness / 2; rad <= radius + thickness / 2; rad++) {
        draw_circle(r, cx, cy, rad);
    }
}

void draw_grid(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
    draw_thick_line(r, CELL_SIZE, 0, CELL_SIZE, WINDOW_SIZE, LINE_THICK);
    draw_thick_line(r, 2 * CELL_SIZE, 0, 2 * CELL_SIZE, WINDOW_SIZE, LINE_THICK);
    draw_thick_line(r, 0, CELL_SIZE, WINDOW_SIZE, CELL_SIZE, LINE_THICK);
    draw_thick_line(r, 0, 2 * CELL_SIZE, WINDOW_SIZE, 2 * CELL_SIZE, LINE_THICK);
}

/* draws an X inside the square starting at (cx0,cy0) with the given side length */
void draw_x_mark(SDL_Renderer* r, int cx0, int cy0, int size, int pad) {
    SDL_SetRenderDrawColor(r, 220, 60, 60, 255);
    draw_thick_line(r, cx0 + pad, cy0 + pad, cx0 + size - pad, cy0 + size - pad, MARK_THICK);
    draw_thick_line(r, cx0 + size - pad, cy0 + pad, cx0 + pad, cy0 + size - pad, MARK_THICK);
}

/* draws an O centered at (cx,cy) with the given radius */
void draw_o_mark(SDL_Renderer* r, int cx, int cy, int radius) {
    SDL_SetRenderDrawColor(r, 60, 100, 220, 255);
    draw_thick_circle(r, cx, cy, radius, MARK_THICK);
}

void draw_x(SDL_Renderer* r, int row, int col) {
    draw_x_mark(r, col * CELL_SIZE, row * CELL_SIZE, CELL_SIZE, PADDING);
}

void draw_o(SDL_Renderer* r, int row, int col) {
    draw_o_mark(r, col * CELL_SIZE + CELL_SIZE / 2, row * CELL_SIZE + CELL_SIZE / 2, CELL_SIZE / 2 - PADDING);
}

/* left half = pick X, right half = pick O */
void draw_side_select(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
    draw_thick_line(r, WINDOW_SIZE / 2, 0, WINDOW_SIZE / 2, WINDOW_SIZE, LINE_THICK);

    int big_pad = 90;
    draw_x_mark(r, 0, 0, WINDOW_SIZE / 2, big_pad);
    draw_o_mark(r, WINDOW_SIZE / 2 + WINDOW_SIZE / 4, WINDOW_SIZE / 2, WINDOW_SIZE / 4 - big_pad);
}

/* ---------- game logic ---------- */

int check_win(int player) {
    for (int i = 0; i < 3; i++) {
        if (board[i][0] == player && board[i][1] == player && board[i][2] == player) return 1;
        if (board[0][i] == player && board[1][i] == player && board[2][i] == player) return 1;
    }
    if (board[0][0] == player && board[1][1] == player && board[2][2] == player) return 1;
    if (board[0][2] == player && board[1][1] == player && board[2][0] == player) return 1;
    return 0;
}

int check_draw(void) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            if (board[r][c] == 0) return 0;
    return 1;
}

void reset_game(void) {
    memset(board, 0, sizeof(board));
    current_player = 1; /* X always moves first */
    game_over = 0;
}

/* returns a 0-8 cell index where `player` would win immediately, or -1 if none */
int find_winning_move(int player) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            if (board[r][c] == 0) {
                board[r][c] = player;
                int wins = check_win(player);
                board[r][c] = 0;
                if (wins) return r * 3 + c;
            }
        }
    }
    return -1;
}

/* simple heuristic AI: take a win, else block, else center, else corner, else edge */
int ai_pick_move(int ai, int human) {
    int move = find_winning_move(ai);
    if (move != -1) return move;

    move = find_winning_move(human);
    if (move != -1) return move;

    if (board[1][1] == 0) return 4;

    int corners[4] = { 0, 2, 6, 8 };
    int start = rand() % 4;
    for (int i = 0; i < 4; i++) {
        int idx = corners[(start + i) % 4];
        if (board[idx / 3][idx % 3] == 0) return idx;
    }

    int edges[4] = { 1, 3, 5, 7 };
    start = rand() % 4;
    for (int i = 0; i < 4; i++) {
        int idx = edges[(start + i) % 4];
        if (board[idx / 3][idx % 3] == 0) return idx;
    }

    return -1;
}

void update_title(SDL_Window* window) {
    if (game_over) return; /* end-of-game titles are set directly where the game ends */
    char title[96];
    if (current_player == human_player) {
        snprintf(title, sizeof(title), "Your turn (%s) - R to change sides", human_player == 1 ? "X" : "O");
    }
    else {
        snprintf(title, sizeof(title), "Computer is thinking...");
    }
    SDL_SetWindowTitle(window, title);
}

void start_ai_turn(SDL_Window* window) {
    current_player = ai_player;
    ai_thinking = 1;
    ai_move_at = SDL_GetTicks() + AI_DELAY_MS;
    SDL_SetWindowTitle(window, "Computer is thinking...");
}

/* ---------- main ---------- */

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    srand((unsigned int)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Tic Tac Toe - Click a side: X (left) or O (right)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_SIZE, WINDOW_SIZE, SDL_WINDOW_SHOWN);

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r) {
                state = STATE_SIDE_SELECT;
                ai_thinking = 0;
                SDL_SetWindowTitle(window, "Tic Tac Toe - Click a side: X (left) or O (right)");
            }
            else if (state == STATE_SIDE_SELECT &&
                e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                human_player = (e.button.x < WINDOW_SIZE / 2) ? 1 : 2;
                ai_player = (human_player == 1) ? 2 : 1;
                reset_game();
                state = STATE_PLAYING;
                if (current_player == ai_player) {
                    start_ai_turn(window);
                }
                else {
                    update_title(window);
                }
            }
            else if (state == STATE_PLAYING && !game_over && !ai_thinking &&
                current_player == human_player &&
                e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int col = e.button.x / CELL_SIZE;
                int row = e.button.y / CELL_SIZE;
                if (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == 0) {
                    board[row][col] = human_player;

                    if (check_win(human_player)) {
                        game_over = 1;
                        SDL_SetWindowTitle(window, "You win! Press R to choose sides again");
                    }
                    else if (check_draw()) {
                        game_over = 1;
                        SDL_SetWindowTitle(window, "It's a draw! Press R to try again");
                    }
                    else {
                        start_ai_turn(window);
                    }
                }
            }
        }

        /* resolve the AI's move once its short "thinking" delay has elapsed */
        if (state == STATE_PLAYING && !game_over && ai_thinking && SDL_GetTicks() >= ai_move_at) {
            int move = ai_pick_move(ai_player, human_player);
            if (move >= 0) board[move / 3][move % 3] = ai_player;
            ai_thinking = 0;

            if (check_win(ai_player)) {
                game_over = 1;
                SDL_SetWindowTitle(window, "Computer wins! Press R to try again");
            }
            else if (check_draw()) {
                game_over = 1;
                SDL_SetWindowTitle(window, "It's a draw! Press R to try again");
            }
            else {
                current_player = human_player;
                update_title(window);
            }
        }

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        SDL_RenderClear(renderer);

        if (state == STATE_SIDE_SELECT) {
            draw_side_select(renderer);
        }
        else {
            draw_grid(renderer);
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    if (board[r][c] == 1) draw_x(renderer, r, c);
                    else if (board[r][c] == 2) draw_o(renderer, r, c);
                }
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}