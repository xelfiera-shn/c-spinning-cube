#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#define _USE_MATH_DEFINES
#include <math.h>

#ifdef _WIN32
    #include <Windows.h>
    #define sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define sleep_ms(ms) usleep((ms) * 1000)
#endif

#define SQRT3 1.73205080756887729352
#define clamp(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))

#define DISPLAY_WIDTH 100
#define DISPLAY_HEIGHT 44
#define CUBE_SIZE 70
#define POINT_OFFSET 0.5f
#define CAM_DIST_FROM_SCREEN 50
#define CAM_DIST_FROM_ORIGIN 150
#define MIN_DIST_FROM_CAM ((CAM_DIST_FROM_ORIGIN) - (CUBE_SIZE) * SQRT3 / 2)
#define MAX_DIST_FROM_CAM ((CAM_DIST_FROM_ORIGIN) + (CUBE_SIZE) * SQRT3 / 2)
#define ASCII_CHARS "@$#*!=;:~-,."

float* zBuffer;
char* displayOld;
char* displayNew;

float alpha, beta, theta;
float sinA, cosA, sinB, cosB, sinC, cosC;

void cleanup(int signal) {
    if (!zBuffer || !displayOld || !displayNew) {
        free(zBuffer);
        free(displayOld);
        free(displayNew);
    }

    printf("\033[2J\033[H");
    printf("\033[?25h");
    fflush(stdout);
    exit(0);
}

static inline float rotateX(float x, float y, float z) {
    return x * cosB * cosC -
           y * cosB * sinC +
           z * sinB;
}

static inline float rotateY(float x, float y, float z) {
    return x * (sinA * sinB * cosC + cosA * sinC) +
           y * (cosA * cosC - sinA * sinB * sinC) -
           z * sinA * cosB;
}

static inline float rotateZ(float x, float y, float z) {
    return x * (sinA * sinC - cosA * sinB * cosC) +
           y * (cosA * sinB * sinC + sinA * cosC) +
           z * cosA * cosB;
}

static inline float euclideanDistance(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;

    return sqrt(dx * dx + dy * dy + dz * dz);
}

static inline void calculatePoint(float x, float y, float z) {
    float rx = rotateX(x, y, z);
    float ry = rotateY(x, y, z);
    float rz = rotateZ(x, y, z);

    float ooz = 1 / (rz + CAM_DIST_FROM_ORIGIN);
    int px = (int)(DISPLAY_WIDTH / 2 + rx * CAM_DIST_FROM_SCREEN * ooz);
    int py = (int)(DISPLAY_HEIGHT / 2 + ry * CAM_DIST_FROM_SCREEN * ooz);

    if (px >= 0 && px < DISPLAY_WIDTH && py >= 0 && py < DISPLAY_HEIGHT) {
        int idx = py * DISPLAY_WIDTH + px;

        if (ooz > zBuffer[idx]) {
            zBuffer[idx] = ooz;

            float distFromCam = euclideanDistance(0, 0, -CAM_DIST_FROM_ORIGIN, rx, ry, rz);
            int opacityIdx = (int)(11 * (distFromCam - MIN_DIST_FROM_CAM) / (MAX_DIST_FROM_CAM - MIN_DIST_FROM_CAM));
            opacityIdx = clamp(opacityIdx, 0, 11);
            displayNew[idx] = ASCII_CHARS[opacityIdx];
        }
    }
}

int main(int argc, const char* argv[]) {
    // cleanup signals for force exit with CTRL+C
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    zBuffer = malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(float));
    displayOld = calloc(DISPLAY_WIDTH * DISPLAY_HEIGHT, sizeof(char));
    displayNew = malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(char));

    if (!zBuffer || !displayOld || !displayNew) {
        free(zBuffer);
        free(displayOld);
        free(displayNew);
        
        fprintf(stderr, "Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }

    printf("\033[2J\033[H\033[?25l"); // clear screen, move cursor to the upper-left, hide cursor
    while (1) {
        memcpy(displayOld, displayNew, DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(char));
        memset(zBuffer, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(float));
        memset(displayNew, ' ', DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(char));

        sinA = sin(alpha); cosA = cos(alpha);
        sinB = sin(beta); cosB = cos(beta);
        sinC = sin(theta); cosC = cos(theta);

        for (float x = -CUBE_SIZE / 2; x <= CUBE_SIZE / 2; x += POINT_OFFSET) {
            for (float y = -CUBE_SIZE / 2; y <= CUBE_SIZE / 2; y += POINT_OFFSET) {
                calculatePoint(x, y, -CUBE_SIZE / 2); // front
                calculatePoint(x, y, CUBE_SIZE / 2); // back
            }
        }

        for (float x = -CUBE_SIZE / 2; x <= CUBE_SIZE / 2; x += POINT_OFFSET) {
            for (float z = -CUBE_SIZE / 2; z <= CUBE_SIZE / 2; z += POINT_OFFSET) {
                calculatePoint(x, CUBE_SIZE / 2, z); // up
                calculatePoint(x, -CUBE_SIZE / 2, z); // down
            }
        }

        for (float y = -CUBE_SIZE / 2; y <= CUBE_SIZE / 2; y += POINT_OFFSET) {
            for (float z = -CUBE_SIZE / 2; z <= CUBE_SIZE / 2; z += POINT_OFFSET) {
                calculatePoint(-CUBE_SIZE / 2, y, z); // left
                calculatePoint(CUBE_SIZE / 2, y, z); // right
            }
        }

        for (int displayX = 0; displayX < DISPLAY_WIDTH; displayX++) {
            for (int displayY = 0; displayY < DISPLAY_HEIGHT; displayY++) {
                int idx = displayY * DISPLAY_WIDTH + displayX;
                if (displayNew[idx] != displayOld[idx]) {
                    printf("\033[%d;%dH", displayY + 1, displayX + 1);
                    putchar(displayNew[idx]);
                }
            }
        }

        alpha = fmod(alpha + 0.1f, 2 * M_PI);
        beta = fmod(beta + 0.2f, 2 * M_PI);
        theta = fmod(theta + 0.2f, 2 * M_PI);

        fflush(stdout);
        sleep_ms(60);
    }

    free(displayNew);
    free(displayOld);
    free(zBuffer);
}