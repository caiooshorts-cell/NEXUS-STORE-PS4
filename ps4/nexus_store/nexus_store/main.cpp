#include <SDL2/SDL.h>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <cstring>

static const int SCREEN_W = 1920;
static const int SCREEN_H = 1080;

static const SDL_Color BG       = { 5, 12, 24, 255 };
static const SDL_Color PANEL    = { 10, 24, 42, 255 };
static const SDL_Color PANEL2   = { 15, 35, 58, 255 };
static const SDL_Color PANEL3   = { 19, 47, 73, 255 };
static const SDL_Color CYAN     = { 44, 194, 238, 255 };
static const SDL_Color MINT     = { 49, 220, 178, 255 };
static const SDL_Color WHITE    = { 235, 246, 255, 255 };
static const SDL_Color MUTED    = { 132, 158, 183, 255 };
static const SDL_Color ORANGE   = { 255, 173, 91, 255 };
static const SDL_Color BORDER   = { 30, 65, 94, 255 };

static const char* NAV_ITEMS[] = {
    "INICIO", "JOGOS", "APLICATIVOS", "EMULADORES", "FERRAMENTAS"
};

static const char* NAV_INFO[] = {
    "PAINEL PRINCIPAL DA NEXUS STORE",
    "A AREA DE JOGOS HOMEBREW SERA CARREGADA PELO CATALOGO ONLINE",
    "APLICATIVOS E UTILITARIOS DA COMUNIDADE",
    "EMULADORES E PROJETOS COMPATIVEIS COM HOME BREW",
    "FERRAMENTAS E RECURSOS PARA O SEU CONSOLE"
};

static void setColor(SDL_Renderer* r, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void fill(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c)
{
    SDL_Rect rect = { x, y, w, h };
    setColor(r, c);
    SDL_RenderFillRect(r, &rect);
}

static void outline(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c)
{
    SDL_Rect rect = { x, y, w, h };
    setColor(r, c);
    SDL_RenderDrawRect(r, &rect);
}

static const uint8_t* glyph(char c)
{
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    static const uint8_t font[37][7] = {
        {0,0,0,0,0,0,0},
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
        {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
        {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
        {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
    };

    c = (char)toupper((unsigned char)c);
    if (c == ' ') return font[0];
    if (c >= 'A' && c <= 'Z') return font[1 + c - 'A'];
    if (c >= '0' && c <= '9') return font[27 + c - '0'];

    static const uint8_t dash[7] = {0,0,0,31,0,0,0};
    static const uint8_t dot[7] = {0,0,0,0,0,12,12};
    static const uint8_t colon[7] = {0,12,12,0,12,12,0};
    static const uint8_t slash[7] = {1,2,2,4,8,8,16};
    static const uint8_t plus[7] = {0,4,4,31,4,4,0};
    static const uint8_t exclamation[7] = {4,4,4,4,4,0,4};
    switch (c) {
        case '-': return dash;
        case '.': return dot;
        case ':': return colon;
        case '/': return slash;
        case '+': return plus;
        case '!': return exclamation;
        default: return blank;
    }
}

static void drawText(SDL_Renderer* r, int x, int y, const char* value, int scale, SDL_Color color)
{
    int startX = x;
    setColor(r, color);
    for (size_t i = 0; value && value[i]; ++i) {
        if (value[i] == '\n') {
            x = startX;
            y += 9 * scale;
            continue;
        }
        const uint8_t* rows = glyph(value[i]);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row] & (1u << (4 - col))) {
                    SDL_Rect pixel = { x + col * scale, y + row * scale, scale, scale };
                    SDL_RenderFillRect(r, &pixel);
                }
            }
        }
        x += 6 * scale;
    }
}

static void drawLogo(SDL_Renderer* r, int x, int y, int size)
{
    fill(r, x, y, size, size, CYAN);
    SDL_Color dark = { 5, 24, 43, 255 };
    fill(r, x + size/5, y + size/5, size/7, size*3/5, dark);
    fill(r, x + size*3/5, y + size/5, size/7, size*3/5, dark);
    for (int i = 0; i < size*3/5; ++i) {
        int dx = size/5 + i * 2 / 3;
        fill(r, x + dx, y + size/5 + i, size/7, 3, dark);
    }
}

static void drawCard(SDL_Renderer* r, int x, int y, const char* title,
                     const char* description, char symbol, bool active)
{
    fill(r, x, y, 465, 235, active ? PANEL3 : PANEL2);
    outline(r, x, y, 465, 235, active ? CYAN : BORDER);
    fill(r, x + 24, y + 24, 72, 72, active ? CYAN : PANEL3);
    char letter[2] = { symbol, 0 };
    drawText(r, x + 44, y + 42, letter, 5, active ? BG : MINT);
    drawText(r, x + 24, y + 122, title, 3, WHITE);
    drawText(r, x + 24, y + 174, description, 2, MUTED);
    if (active) {
        fill(r, x + 415, y + 26, 18, 5, MINT);
        fill(r, x + 428, y + 26, 5, 18, MINT);
    }
}

static void render(SDL_Renderer* r, int selected, bool dialog, bool controllerConnected)
{
    setColor(r, BG);
    SDL_RenderClear(r);

    // Header
    fill(r, 0, 0, SCREEN_W, 104, PANEL);
    fill(r, 0, 102, SCREEN_W, 2, BORDER);
    drawLogo(r, 40, 21, 62);
    drawText(r, 126, 23, "NEXUS STORE", 4, WHITE);
    drawText(r, 128, 69, "HOME BREW MARKET", 2, CYAN);
    fill(r, 1550, 28, 2, 48, BORDER);
    drawText(r, 1590, 31, controllerConnected ? "DUALSHOCK 4 OK" : "AGUARDANDO CONTROLE", 2,
             controllerConnected ? MINT : ORANGE);
    drawText(r, 1590, 60, "BUILD 0.1 - PREVIEW", 1, MUTED);

    // Sidebar
    fill(r, 0, 104, 300, 976, {7,17,31,255});
    drawText(r, 38, 145, "MENU PRINCIPAL", 2, MUTED);
    for (int i = 0; i < 5; ++i) {
        int y = 198 + i * 100;
        if (i == selected) {
            fill(r, 20, y - 16, 260, 72, PANEL3);
            fill(r, 20, y - 16, 6, 72, CYAN);
        }
        drawText(r, 48, y, NAV_ITEMS[i], 2, i == selected ? WHITE : MUTED);
        if (i == selected) {
            fill(r, 245, y + 7, 10, 10, MINT);
        }
    }
    fill(r, 36, 755, 228, 2, BORDER);
    drawText(r, 38, 790, "NAVEGACAO", 2, MUTED);
    drawText(r, 38, 830, "USE AS SETAS", 2, WHITE);
    drawText(r, 38, 866, "DO CONTROLE", 2, WHITE);
    drawText(r, 38, 938, "NEXUS", 3, CYAN);
    drawText(r, 38, 975, "STORE", 3, WHITE);

    // Main hero
    fill(r, 340, 138, 1535, 350, PANEL);
    outline(r, 340, 138, 1535, 350, BORDER);
    fill(r, 340, 138, 10, 350, CYAN);
    drawText(r, 390, 180, "HOME BREW ECOSYSTEM", 2, MINT);
    drawText(r, 390, 235, "SUA LOJA NO PS4", 7, WHITE);
    drawText(r, 394, 335, "JOGOS, APLICATIVOS E FERRAMENTAS", 3, CYAN);
    drawText(r, 394, 385, "TUDO ORGANIZADO EM UM SO LUGAR", 2, MUTED);

    // Decorative brand panel
    fill(r, 1490, 170, 335, 285, {8,31,53,255});
    outline(r, 1490, 170, 335, 285, {25,100,137,255});
    drawLogo(r, 1605, 210, 105);
    drawText(r, 1545, 345, "NEXUS STORE", 2, WHITE);
    drawText(r, 1553, 390, "VERSAO INICIAL", 2, MINT);

    // Categories
    drawText(r, 345, 535, "EXPLORE POR CATEGORIA", 3, WHITE);
    drawText(r, 1480, 545, "SELECIONE COM X", 2, MUTED);
    drawCard(r, 350, 590, "JOGOS HOMEBREW", "PROJETOS DA COMUNIDADE", 'J', selected == 1);
    drawCard(r, 850, 590, "APLICATIVOS", "UTILITARIOS E APPS", 'A', selected == 2);
    drawCard(r, 1350, 590, "EMULADORES", "RETRO E COMPATIBILIDADE", 'E', selected == 3);

    // Online catalogue status
    fill(r, 350, 855, 1465, 132, {8,22,38,255});
    outline(r, 350, 855, 1465, 132, BORDER);
    fill(r, 350, 855, 8, 132, MINT);
    drawText(r, 385, 884, "CATALOGO REMOTO", 2, MINT);
    drawText(r, 385, 925, "A INTERFACE NATIVA ESTA PRONTA.", 2, WHITE);
    drawText(r, 385, 958, "A CONEXAO COM O CATALOGO E OS DOWNLOADS ENTRA NA PROXIMA ETAPA.", 2, MUTED);
    fill(r, 1515, 895, 260, 46, PANEL3);
    outline(r, 1515, 895, 260, 46, {35,107,137,255});
    drawText(r, 1540, 910, "MODO DEMONSTRACAO", 1, CYAN);

    // Footer control hints
    fill(r, 0, 1034, SCREEN_W, 46, {4,10,20,255});
    fill(r, 0, 1034, SCREEN_W, 2, BORDER);
    drawText(r, 350, 1050, "X  ABRIR", 1, WHITE);
    drawText(r, 560, 1050, "O  VOLTAR", 1, WHITE);
    drawText(r, 790, 1050, "L1 / R1  TROCAR AREA", 1, WHITE);
    drawText(r, 1510, 1050, "NEXUS STORE - PS4", 1, MUTED);

    if (dialog) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        fill(r, 0, 0, SCREEN_W, SCREEN_H, {0,0,0,190});
        fill(r, 530, 350, 860, 360, PANEL2);
        outline(r, 530, 350, 860, 360, CYAN);
        fill(r, 530, 350, 860, 8, CYAN);
        drawText(r, 600, 405, "NEXUS STORE", 4, WHITE);
        drawText(r, 600, 490, NAV_ITEMS[selected], 3, MINT);
        drawText(r, 600, 550, "AREA SELECIONADA COM SUCESSO.", 2, WHITE);
        drawText(r, 600, 593, NAV_INFO[selected], 2, MUTED);
        drawText(r, 600, 660, "APERTE O CIRCULO PARA VOLTAR", 2, CYAN);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        std::printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);

    SDL_Window* window = SDL_CreateWindow(
        "NEXUS STORE",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_W,
        SCREEN_H,
        0
    );

    if (!window) {
        std::printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if (!surface) {
        std::printf("Window surface failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }

    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer) {
        std::printf("Software renderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }

    SDL_Joystick* controller = NULL;
    if (SDL_NumJoysticks() > 0) {
        controller = SDL_JoystickOpen(0);
    }

    int selected = 0;
    bool dialog = false;
    bool running = true;

    while (running) {
        SDL_JoystickUpdate();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_JOYHATMOTION) {
                if (event.jhat.value & SDL_HAT_UP) {
                    selected = (selected + 4) % 5;
                    dialog = false;
                } else if (event.jhat.value & SDL_HAT_DOWN) {
                    selected = (selected + 1) % 5;
                    dialog = false;
                }
                if (event.jhat.value & SDL_HAT_LEFT) {
                    selected = (selected + 4) % 5;
                    dialog = false;
                } else if (event.jhat.value & SDL_HAT_RIGHT) {
                    selected = (selected + 1) % 5;
                    dialog = false;
                }
            } else if (event.type == SDL_JOYBUTTONDOWN) {
                if (event.jbutton.button == 0) {
                    dialog = true;
                } else if (event.jbutton.button == 1) {
                    dialog = false;
                } else if (event.jbutton.button == 4) {
                    selected = (selected + 4) % 5;
                    dialog = false;
                } else if (event.jbutton.button == 5) {
                    selected = (selected + 1) % 5;
                    dialog = false;
                } else if (event.jbutton.button == 3) {
                    dialog = true;
                }
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_UP || key == SDLK_LEFT) {
                    selected = (selected + 4) % 5;
                    dialog = false;
                } else if (key == SDLK_DOWN || key == SDLK_RIGHT) {
                    selected = (selected + 1) % 5;
                    dialog = false;
                } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    dialog = true;
                } else if (key == SDLK_ESCAPE || key == SDLK_BACKSPACE) {
                    dialog = false;
                }
            }
        }

        render(renderer, selected, dialog, controller != NULL);
        SDL_UpdateWindowSurface(window);
        SDL_Delay(16);
    }

    if (controller) SDL_JoystickClose(controller);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
