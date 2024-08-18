#include <SDL.h>
#include <SDL_image.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>

#define WIN_WIDTH       800
#define WIN_HEIGHT      600
#define WIN_MIN_WIDTH   400
#define WIN_MIN_HEIGHT  200
#define MAX_MOUSE_BUTTONS   3
#define MAX_FILES           1024
#define FILEPATH_MAX        PATH_MAX

typedef SDL_Window   win_t;
typedef SDL_Renderer ren_t;
typedef SDL_FRect    rectf_t;

typedef struct file_t {
    ino_t           ino;
    rectf_t         rect;
    float           nat_w, nat_h;
    SDL_Texture     *tex;
    char            *path;
    struct file_t   *prev;
    struct file_t   *next;
} file_t;

typedef enum {
    MBUTTON_LEFT = 1,
    MBUTTON_MIDDLE,
    MBUTTON_RIGHT,
} mbutton_type_t;

typedef struct {
    float x, y;
    int pressed;
} mbutton_t;

typedef struct {
    win_t       *win;
    ren_t       *ren;
    int         quit;
    float       win_w, win_h;
    float       screen_w, screen_h;
    mbutton_t   mbuttons[MAX_MOUSE_BUTTONS];
    struct {
        file_t  *head;
        file_t  *current;
        file_t  *tail;
        size_t  total;
    } files;
} app_t;

typedef struct {
    DIR                 *dir;
    app_t               *app;
    const char          *path;
    pthread_mutex_t     lock;
    size_t              i;
    size_t              max;
} app_ldt_ctx_t;

// Misc 
int has_ext(const char *str, const char *ext);
int is_image(const char *path);

// App
void app_fail(const char *label);
void app_update_size_data(app_t *app);
void app_free_files(app_t *app);
void app_render_file(app_t *app);
void app_to_screen(const app_t *app, float x, float y, float *dst_x, float *dst_y);
void app_load_dir(app_t *app, const char *path, size_t max);
void app_set_file(app_t *app, size_t file_idx);
void app_render_random_file(app_t *app);
void app_render_prev_file(app_t *app);
void app_render_next_file(app_t *app);

// Renderer
void ren_set_draw_color(ren_t *ren, uint32_t hex);

// File
file_t *file_load(ren_t *ren, const char *path, float x, float y, float w, float h);
void file_free(file_t *file);
void file_render(ren_t *ren, file_t *file);

// Events
int  SDLCALL ev_handler(void *data, SDL_Event *ev);
void ev_multigesture(const SDL_MultiGestureEvent *ev, app_t *app);
void ev_window(const SDL_WindowEvent *ev, app_t *app);
void ev_mousedown(const SDL_MouseButtonEvent *ev, app_t *app);
void ev_mouseup(const SDL_MouseButtonEvent *ev, app_t *app);
void ev_mousemotion(const SDL_MouseMotionEvent *ev, app_t *app);
void ev_mousewheel(const SDL_MouseWheelEvent *ev, app_t *app);
void ev_keyup(const SDL_KeyboardEvent *ev, app_t *app);

int main(int argc, char *argv[])
{
    srand(time(NULL));

    char *dir = NULL;
    char *file_name = NULL;
    size_t max = 0;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (strcmp(arg, "-d") == 0) {
            dir = argv[++i];
        } else if (SDL_strcmp(arg, "-f") == 0) {
            file_name = argv[++i];
        } else if (SDL_strcmp(arg, "-m") == 0) {
            max = SDL_atoi(argv[++i]);
        }
    }

    if (dir == NULL && file_name == NULL) {
        SDL_Log("Expected at least -d or -f flag");
        exit(1);
    }

    max = max == 0 ? MAX_FILES : max;

    app_t app;
    SDL_memset(&app, 0, sizeof(app));

    SDL_Init(SDL_INIT_EVERYTHING);
    
    app.win = SDL_CreateWindow("",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WIN_WIDTH, WIN_HEIGHT,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (app.win == NULL) app_fail("CreateWindow");

    app.ren = SDL_CreateRenderer(app.win, -1, SDL_RENDERER_ACCELERATED);
    if (app.ren == NULL) app_fail("CreateRenderer");

    SDL_SetWindowMinimumSize(app.win, WIN_MIN_WIDTH, WIN_MIN_HEIGHT);
    SDL_AddEventWatch(ev_handler, &app);

    if (SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1") == SDL_FALSE) {
        app_fail("SetHint");
    }
    if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2") == SDL_FALSE) {
        app_fail("SetHint");
    }

    if (dir != NULL) {
        app_load_dir(&app, dir, max);
        if (app.files.head == NULL) {
            SDL_Log("No files found\n");
            exit(1);
        }
    } else {
        SDL_Log("TODO: load single file");
        exit(1);
    }

    // Find initial file
    if (file_name == NULL) {
        app.files.current = app.files.head;
    } else {
        file_t *file = app.files.head;
        while (file != NULL) {
            if (SDL_strcmp(file->path, file_name) == 0) {
                app.files.current = file;
                break;
            }
            file = file->next;
        }
    }

    if (app.files.current == NULL) {
        SDL_Log("Can't find any file");
        exit(1);
    }

    app_update_size_data(&app);
    app_render_file(&app);
    SDL_RenderPresent(app.ren);

    SDL_Event ev;

    while (!app.quit) {
        while (SDL_PollEvent(&ev));
        SDL_Delay(15);
    }

    SDL_DestroyRenderer(app.ren);
    SDL_DestroyWindow(app.win);
    app_free_files(&app);
    IMG_Quit();
    SDL_Quit();

    return 0;
}

// Misc
int has_ext(const char *str, const char *ext)
{
    const char *this_ext = SDL_strrchr(str, '.');
    return this_ext == NULL ? 0 : SDL_strcmp(this_ext, ext) == 0;
}

int is_image(const char *path)
{
    return has_ext(path, ".png") ||
           has_ext(path, ".jpg") ||
           has_ext(path, ".jpeg");
}

// App
void app_fail(const char *label)
{
    SDL_Log("%s: %s\n", label, SDL_GetError());
    exit(1);   
}

void app_update_size_data(app_t *app)
{
    int win_w, win_h, screen_w, screen_h;
    SDL_GetWindowSize(app->win, &win_w, &win_h);
    SDL_GL_GetDrawableSize(app->win, &screen_w, &screen_h);
    app->win_w = win_w * 1.0;
    app->win_h = win_h * 1.0;
    app->screen_w = screen_w * 1.0;
    app->screen_h = screen_h * 1.0;
}

void app_free_files(app_t *app)
{
    file_t *file = app->files.head;
    while (file != NULL) {
        file_t *next = file->next;
        file_free(file);
        file = next;
    }
}

void app_render_file(app_t *app)
{
    SDL_SetWindowTitle(app->win, app->files.current->path);

    float tex_aspect = app->files.current->nat_w / app->files.current->nat_h;
    float screen_aspect = app->screen_w / app->screen_h;

    if (tex_aspect > screen_aspect) {
        // Texture is wider relative to its height than the container
        app->files.current->rect.w = app->screen_w;
        app->files.current->rect.h = app->screen_w / tex_aspect;
    } else {
        // Texture is taller relative to its width than the container
        app->files.current->rect.h = app->screen_h;
        app->files.current->rect.w = app->screen_h * tex_aspect;
    }

    app->files.current->rect.x = app->screen_w / 2 - app->files.current->rect.w / 2;
    app->files.current->rect.y = app->screen_h / 2 - app->files.current->rect.h / 2;
    
    file_render(app->ren, app->files.current);
}

void app_to_screen(const app_t *app, float x, float y, float *dst_x, float *dst_y)
{
    *dst_x = x * (app->screen_w / app->win_w);
    *dst_y = y * (app->screen_h / app->win_h);
}

void *app_load_dir_thread(void *data)
{
    app_ldt_ctx_t *ctx = (app_ldt_ctx_t *)data;
    struct dirent *ent;

    while (1) {
        pthread_mutex_lock(&ctx->lock);
        ent = readdir(ctx->dir);
        pthread_mutex_unlock(&ctx->lock);
        
        if (ent == NULL) break;
        
        if (ent->d_type == DT_REG && is_image(ent->d_name)) {
            char file_path[FILEPATH_MAX]; 
            SDL_snprintf(file_path, FILEPATH_MAX, "%s/%s", ctx->path, ent->d_name);
            SDL_Log("Loading %s\n", file_path);
            
            file_t *file = file_load(ctx->app->ren, file_path, 0, 0, 0, 0);

            if (file != NULL) {
                file->ino = ent->d_ino;

                pthread_mutex_lock(&ctx->lock);
                ctx->app->files.total++;

                if (ctx->app->files.head == NULL) {
                    ctx->app->files.head = file;
                    ctx->app->files.tail = file;
                    ctx->app->files.current = file;
                    file->prev = file->next = NULL;
                } else {
                    file->prev = ctx->app->files.tail;
                    ctx->app->files.tail->next = file;
                    ctx->app->files.tail = file;
                    ctx->app->files.current = file;
                }

                pthread_mutex_unlock(&ctx->lock);
            }
        }
    }

    return NULL;
}

void app_load_dir(app_t *app, const char *path, size_t max)
{
    DIR *dir = opendir(path);
    if (dir == NULL) app_fail("opendir");

    pthread_t tids[24] = {0};
    size_t cpus = (sysconf(_SC_NPROCESSORS_ONLN) + 2) % sizeof(tids);

    app_ldt_ctx_t ctx = {
        .dir  = dir,
        .app  = app,
        .path = path,
        .i    = 0,
        .max  = max % (MAX_FILES + 1),
    };

    pthread_mutex_init(&ctx.lock, NULL);
    
    for (size_t i = 0; i < cpus; i++) {
        if (pthread_create(&tids[i], NULL, app_load_dir_thread, &ctx) != 0) {
            SDL_Log("Couldn't create a thread\n");
            exit(1);
        }
    }

    for (size_t i = 0; i < cpus; i++) {
        if (pthread_join(tids[i], NULL) != 0) {
            SDL_Log("Couldn't wait for a thread\n");
            exit(1);
        }
    }

    closedir(dir);
}

void app_render_random_file(app_t *app)
{
    size_t idx = rand() % app->files.total;
    size_t i = 0;
    file_t *file = app->files.head;

    while (file != NULL) {
        if (idx == i++) break;
        file = file->next;
    }

    if (file != NULL) {
        SDL_RenderClear(app->ren);
        app->files.current = file;
        app_render_file(app);
        SDL_RenderPresent(app->ren);
    }
} 

void app_render_prev_file(app_t *app)
{
    if (app->files.current != NULL) {
        if (app->files.current == app->files.head) {
            app->files.current = app->files.tail;
        } else {
            app->files.current = app->files.current->prev;
        }
    }

    SDL_RenderClear(app->ren);
    app_render_file(app);
    SDL_RenderPresent(app->ren);
}

void app_render_next_file(app_t *app)
{
    if (app->files.current != NULL) {
        if (app->files.current == app->files.tail) {
            app->files.current = app->files.head;
        } else {
            app->files.current = app->files.current->next;
        }
    }

    SDL_RenderClear(app->ren);
    app_render_file(app);
    SDL_RenderPresent(app->ren);
}

// Renderer
void ren_set_draw_color(ren_t *ren, uint32_t hex) {
    uint8_t r, g, b, a;
    r = hex >> 16;
    g = hex >> 8;
    b = hex >> 0;
    a = hex >> 24;
    a = a ? a : 0;
    SDL_SetRenderDrawColor(ren, r, g, b, a);
}

// File
file_t *file_load(ren_t *ren, const char *path, float x, float y, float w, float h)
{
    file_t *file = SDL_calloc(1, sizeof(file_t));
    if (file == NULL) return NULL;

    file->tex = IMG_LoadTexture(ren, path);
    if (file->tex == NULL) goto free;
    
    int nat_w, nat_h;
    if (SDL_QueryTexture(file->tex, NULL, NULL, &nat_w, &nat_h) != 0) goto free;

    size_t path_len = sizeof(char) * (SDL_strlen(path) + 1);
    
    file->path = SDL_malloc(path_len);
    if (file->path == NULL) goto free;
    
    (void)SDL_strlcpy(file->path, path, path_len);
    
    file->rect.x = x;
    file->rect.y = y;
    file->rect.w = w;
    file->rect.h = h;
    file->nat_w = nat_w;
    file->nat_h = nat_h;
    file->prev = file->next = NULL;
    
    return file;

free:
    file_free(file);
    return NULL;
}

void file_free(file_t *file)
{
    SDL_DestroyTexture(file->tex);
    SDL_free(file->path);
    file->tex = NULL;
    file->path = NULL;
    SDL_free(file);
}

void file_render(ren_t *ren, file_t *file)
{
    SDL_RenderCopyF(ren, file->tex, NULL, &file->rect); 
}

// Events
int SDLCALL ev_handler(void *data, SDL_Event *ev)
{
    app_t *app = (app_t *)data;
    
    switch (ev->type) {
        case SDL_QUIT:
            app->quit = 1;
            break;
        case SDL_WINDOWEVENT:
            ev_window((SDL_WindowEvent *)ev, app);
            break;
        case SDL_MOUSEBUTTONDOWN:
            ev_mousedown((SDL_MouseButtonEvent *)ev, app);
            break;
        case SDL_MOUSEBUTTONUP:
            ev_mouseup((SDL_MouseButtonEvent *)ev, app);
            break;
        case SDL_MOUSEMOTION:
            ev_mousemotion((SDL_MouseMotionEvent *)ev, app);
            break;
        case SDL_MOUSEWHEEL:
            ev_mousewheel((SDL_MouseWheelEvent *)ev, app);
            break;
        case SDL_MULTIGESTURE:
            ev_multigesture((SDL_MultiGestureEvent *)ev, app);
            break;
        case SDL_KEYUP:
            ev_keyup((SDL_KeyboardEvent *)ev, app);
            break;
    }

    return 0;
}

void ev_multigesture(const SDL_MultiGestureEvent *ev, app_t *app)
{
    if (ev->numFingers == 2 && !app->mbuttons[MBUTTON_LEFT].pressed) {
        float scale_factor = 1.035;
        float scale = 1;
        
        if (ev->dDist > 0.002) scale = scale_factor;
        else if (ev->dDist < -0.002) scale /= scale_factor;

        int win_mouse_x, win_mouse_y;
        float mouse_x, mouse_y;
        SDL_GetMouseState(&win_mouse_x, &win_mouse_y);
        app_to_screen(app, win_mouse_x, win_mouse_y, &mouse_x, &mouse_y);

        app->files.current->rect.w *= scale;
        app->files.current->rect.h *= scale;
        app->files.current->rect.x = mouse_x - (mouse_x - app->files.current->rect.x) * scale;
        app->files.current->rect.y = mouse_y - (mouse_y - app->files.current->rect.y) * scale;

        SDL_RenderClear(app->ren);
        file_render(app->ren, app->files.current);
        SDL_RenderPresent(app->ren);
    }
}

void ev_window(const SDL_WindowEvent *ev, app_t *app)
{
    if (ev->event == SDL_WINDOWEVENT_RESIZED) {
        SDL_RenderClear(app->ren);
        app_update_size_data(app);
        app_render_file(app);
        SDL_RenderPresent(app->ren);
    }
}

void ev_mousedown(const SDL_MouseButtonEvent *ev, app_t *app)
{
    mbutton_t *btn = &app->mbuttons[ev->button];
    btn->pressed = 1;
    app_to_screen(app, ev->x, ev->y, &btn->x, &btn->y);
}

void ev_mouseup(const SDL_MouseButtonEvent *ev, app_t *app)
{
    app->mbuttons[ev->button].pressed = 0;
}

void ev_mousemotion(const SDL_MouseMotionEvent *ev, app_t *app)
{
    (void)ev;

    mbutton_t *btn_l = &app->mbuttons[MBUTTON_LEFT];

    if (btn_l->pressed) {
        float mouse_x, mouse_y;
        app_to_screen(app, ev->x, ev->y, &mouse_x, &mouse_y);

        app->files.current->rect.x += (mouse_x - btn_l->x);
        app->files.current->rect.y += (mouse_y - btn_l->y);
        btn_l->x = mouse_x;
        btn_l->y = mouse_y;
        SDL_RenderClear(app->ren);
        file_render(app->ren, app->files.current);
        SDL_RenderPresent(app->ren);
    }
}

void ev_mousewheel(const SDL_MouseWheelEvent *ev, app_t *app)
{
    float scale_factor = 1.1;
    float scale = ev->y < 0 ? scale_factor : 1/scale_factor;
    
    float mouse_x, mouse_y;
    app_to_screen(app, ev->mouseX, ev->mouseY, &mouse_x, &mouse_y);

    app->files.current->rect.w *= scale;
    app->files.current->rect.h *= scale;
    app->files.current->rect.x = mouse_x - (mouse_x - app->files.current->rect.x) * scale;
    app->files.current->rect.y = mouse_y - (mouse_y - app->files.current->rect.y) * scale;

    SDL_RenderClear(app->ren);
    file_render(app->ren, app->files.current);
    SDL_RenderPresent(app->ren);
}

void ev_keyup(const SDL_KeyboardEvent *ev, app_t *app)
{
    switch (ev->keysym.sym) {
        case SDLK_LEFT:
        case SDLK_a:
            app_render_prev_file(app);
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            app_render_next_file(app);
            break;
        case SDLK_r:
            app_render_random_file(app);
            break;
    }
}
