#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
// #include <drm_fourcc.h>
#include "/home/xjt/_Workspace_/VOC/System/RV1126_RV1109_LINUX_SDK_V2.2.5.1_20230530/buildroot/output/rockchip_rv1126_rv1109/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include/drm/drm_fourcc.h"

#define RECT_WIDTH 100
#define RECT_HEIGHT 100
#define NUM_RECTS 8
#define FPS 60

struct Rectangle {
    int x, y;
    int velo_x, velo_y;
    uint32_t color;
};

struct Rectangle rects[NUM_RECTS];

int fd;
drmModeConnector *connector;
drmModeEncoder *encoder;
drmModeModeInfo mode;
uint32_t crtc_id;
struct drm_mode_create_dumb create_dumb = {0};
struct drm_mode_map_dumb map_dumb = {0};
uint32_t fb_id;
uint8_t *map;
int width, height;        // Actual display dimensions
int buffer_width, buffer_height;  // Buffer dimensions (aligned)
int stride;               // Bytes per row in the buffer

void initialize_rects() {
    uint32_t colors[NUM_RECTS] = {
        0xFFFF0000,  // Red
        0xFF00FF00,  // Green
        0xFF0000FF,  // Blue
        0xFFFFFF00,  // Yellow
        0xFFFF00FF,  // Purple
        0xFF00FFFF,  // Cyan
        0xFF000000,  // Black
        0xFFFFFFFF   // White
    };

    for (int i = 0; i < NUM_RECTS; i++) {
        rects[i].x = rand() % (width - RECT_WIDTH);
        rects[i].y = rand() % (height - RECT_HEIGHT);
        rects[i].velo_x = (rand() % 10) + 5;
        rects[i].velo_y = (rand() % 10) + 5;
        rects[i].color = colors[i];
    }
}

void update_rect_positions() {
    for (int i = 0; i < NUM_RECTS; i++) {
        rects[i].x += rects[i].velo_x;
        rects[i].y += rects[i].velo_y;

        if (rects[i].x <= 0 || rects[i].x + RECT_WIDTH >= width) {
            rects[i].velo_x = -rects[i].velo_x;
        }
        if (rects[i].y <= 0 || rects[i].y + RECT_HEIGHT >= height) {
            rects[i].velo_y = -rects[i].velo_y;
        }
    }
}

void draw_rects() {
    uint32_t *pixels = (uint32_t *)map;
    
    // Clear the entire buffer
    for (int y = 0; y < buffer_height; y++) {
        for (int x = 0; x < buffer_width; x++) {
            pixels[y * (stride / 4) + x] = 0;
        }
    }
    
    // Draw all rectangles
    for (int i = 0; i < NUM_RECTS; i++) {
        for (int y = rects[i].y; y < rects[i].y + RECT_HEIGHT; y++) {
            for (int x = rects[i].x; x < rects[i].x + RECT_WIDTH; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    pixels[y * (stride / 4) + x] = rects[i].color;
                }
            }
        }
    }
}

int main() {
    srand(time(NULL));

    fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    drmModeRes *resources = drmModeGetResources(fd);
    if (!resources) {
        perror("drmModeGetResources");
        return 1;
    }

    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED) {
            break;
        }
        drmModeFreeConnector(connector);
    }

    if (!connector) {
        fprintf(stderr, "No connected connector found\n");
        return 1;
    }

    mode = connector->modes[0];
    width = mode.hdisplay;
    height = mode.vdisplay;
    buffer_width = (width + 15) & ~15;  // Align to 16 pixels
    buffer_height = height;

    for (int i = 0; i < resources->count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, resources->encoders[i]);
        if (encoder->encoder_id == connector->encoder_id) {
            break;
        }
        drmModeFreeEncoder(encoder);
    }

    if (!encoder) {
        fprintf(stderr, "No encoder found\n");
        return 1;
    }

    crtc_id = encoder->crtc_id;

    create_dumb.width = buffer_width;
    create_dumb.height = buffer_height;
    create_dumb.bpp = 32;
    drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);

    stride = create_dumb.pitch;

    drmModeAddFB(fd, buffer_width, buffer_height, 24, 32, stride, create_dumb.handle, &fb_id);

    map_dumb.handle = create_dumb.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);

    map = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_dumb.offset);

    drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &connector->connector_id, 1, &mode);

    initialize_rects();

    struct timespec start, end;
    long frame_duration = 1000000000 / FPS; // nanoseconds

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &start);

        update_rect_positions();
        draw_rects();

        drmModePageFlip(fd, crtc_id, fb_id, DRM_MODE_PAGE_FLIP_EVENT, NULL);

        clock_gettime(CLOCK_MONOTONIC, &end);
        long elapsed = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
        if (elapsed < frame_duration) {
            struct timespec remaining;
            remaining.tv_sec = 0;
            remaining.tv_nsec = frame_duration - elapsed;
            nanosleep(&remaining, NULL);
        }
    }

    // Clean up (this part is never reached in this example)
    drmModeRmFB(fd, fb_id);
    munmap(map, create_dumb.size);
    struct drm_mode_destroy_dumb destroy_dumb = { .handle = create_dumb.handle };
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    close(fd);

    return 0;
}