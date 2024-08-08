#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>

#define WIN_TITLE "File viewer"
#define WIN_WIDTH  800
#define WIN_HEIGHT 600

static int app_running;
static SDL_Window *win;
static SDL_Renderer *ren;

void err(const char *msg)
{
    printf("ERROR: %s\n", msg);
    exit(1);
}

void init(void)
{
    SDL_Init(SDL_INIT_EVERYTHING);
    
    win = SDL_CreateWindow(
            WIN_TITLE, 0, 0, 
            WIN_WIDTH, WIN_HEIGHT,
            SDL_WINDOW_RESIZABLE);
    
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    if (win == NULL) err("Window creation fail");
    if (ren == NULL) err("Renderer creation fail");

    app_running = 1;
}

void cleanup(void)
{
    SDL_DestroyWindow(win);
    SDL_DestroyRenderer(ren);
    SDL_Quit();
}

void handle_events(void)
{
    SDL_Event ev;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            app_running = 0;
            break;
        }
    }
}

int main(void)
{
    init();

    while (app_running) {
        handle_events();
    }

    cleanup();

    return 0;
}
