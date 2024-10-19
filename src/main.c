#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <time.h>
#include <errno.h>

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define RECT_WIDTH 100
#define RECT_HEIGHT 100
#define FPS 60

struct wl_display *display = NULL;
struct wl_registry *registry = NULL;
struct wl_compositor *compositor = NULL;
struct wl_surface *surface = NULL;
struct wl_shell *shell = NULL;
struct wl_shell_surface *shell_surface = NULL;
struct wl_shm *shm = NULL;

int rect_x = 0, rect_y = 0;
int velo_x = 10, velo_y = 10;

void *shm_data = NULL;
struct wl_buffer *buffer = NULL;

static void registry_handler(void *data, struct wl_registry *registry, uint32_t id,
                             const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shell") == 0) {
        shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_shm") == 0) {
        shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_handler,
    NULL
};

static struct wl_buffer* create_buffer() {
    int stride = WINDOW_WIDTH * 4;
    int size = stride * WINDOW_HEIGHT;

    int fd = memfd_create("buffer", 0);
    if (fd < 0) {
        perror("Failed to create memfd");
        exit(1);
    }

    if (ftruncate(fd, size) < 0) {
        perror("Failed to set size of memfd");
        close(fd);
        exit(1);
    }

    shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_data == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        exit(1);
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, WINDOW_WIDTH, WINDOW_HEIGHT, stride, WL_SHM_FORMAT_ARGB8888);

    wl_shm_pool_destroy(pool);
    close(fd);

    return buffer;
}

void draw_rect() {
    uint32_t *pixels = (uint32_t *)shm_data;
    
    // Clear the entire buffer with transparent color
    memset(pixels, 0, WINDOW_WIDTH * WINDOW_HEIGHT * 4);
    
    // Draw the white rectangle at new position
    for (int y = rect_y; y < rect_y + RECT_HEIGHT; y++) {
        for (int x = rect_x; x < rect_x + RECT_WIDTH; x++) {
            if (x >= 0 && x < WINDOW_WIDTH && y >= 0 && y < WINDOW_HEIGHT) {
                pixels[y * WINDOW_WIDTH + x] = 0xFFFFFFFF;  // White color
            }
        }
    }
}

void update_rect_position() {
    rect_x += velo_x;
    rect_y += velo_y;

    // Collision detection
    if (rect_x <= 0 || rect_x + RECT_WIDTH >= WINDOW_WIDTH) {
        velo_x = -velo_x;
    }
    if (rect_y <= 0 || rect_y + RECT_HEIGHT >= WINDOW_HEIGHT) {
        velo_y = -velo_y;
    }
}

int main(int argc, char **argv) {
    display = wl_display_connect(NULL);
    if (display == NULL) {
        fprintf(stderr, "Can't connect to display\n");
        exit(1);
    }
    printf("Connected to display\n");

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (compositor == NULL || shell == NULL || shm == NULL) {
        fprintf(stderr, "Can't find all required globals\n");
        exit(1);
    }

    surface = wl_compositor_create_surface(compositor);
    if (surface == NULL) {
        fprintf(stderr, "Can't create surface\n");
        exit(1);
    }

    shell_surface = wl_shell_get_shell_surface(shell, surface);
    if (shell_surface == NULL) {
        fprintf(stderr, "Can't create shell surface\n");
        exit(1);
    }

    wl_shell_surface_set_fullscreen(shell_surface, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, NULL);

    buffer = create_buffer();

    struct timespec start, end;
    long frame_duration = 1000000000 / FPS; // nanoseconds

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &start);

        update_rect_position();
        draw_rect();

        wl_surface_attach(surface, buffer, 0, 0);
        wl_surface_damage(surface, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        wl_surface_commit(surface);

        wl_display_dispatch_pending(display);
        wl_display_flush(display);

        clock_gettime(CLOCK_MONOTONIC, &end);
        long elapsed = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
        if (elapsed < frame_duration) {
            struct timespec remaining;
            remaining.tv_sec = 0;
            remaining.tv_nsec = frame_duration - elapsed;
            nanosleep(&remaining, NULL);
        }
    }

    wl_buffer_destroy(buffer);
    wl_shell_surface_destroy(shell_surface);
    wl_surface_destroy(surface);
    wl_shell_destroy(shell);
    wl_compositor_destroy(compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    printf("Disconnected from display\n");

    return 0;
}