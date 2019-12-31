// helpers
double max(double a, int b) { return a < b ? b : a; }
float max(int a, float b) { return a < b ? b : a; }
float pseudorandom(float x) { return fmod((sin(x) * 12.9898 + 78.233) * 43758.5453, 1.); }

double smoothstep(double a, double b, double x)
{
    x = constrain((x - a) / (b - a), 0, 1);
    return x * x * (3 - 2 * x);
}

#define LIMIT_BRIGHTNESS 70                   // maximum brightness in %
#define WHITE_SUPPRESSION 5                   // power to favor high saturations

// STORING ADDRESSES
#define MAX_STORE_SIZE 20
#define STORE_PATTERN 0
#define STORE_BRIGHTNESS 4
#define STORE_HUE 8
#define STORE_WHITE 12
#define STORE_ON_NEXT_BOOT 16

// THIS IS THE ACTIVE PATTERN ORDER
#define N_PATTERNS 2
uint8_t PATTERN[N_PATTERNS] = {0, 1};

// for all dynamic pattern-specific variables (cf. somewhere in main program)
#define SLOTS 4

#define WATER_HUE 224
#define WATER_SCALE .7
#define WATER_SPEED .01
#define WATER_SPEED_RND 0.003
#define WATER_BG 0.6
#define WATER_WHITE_MAX 1
#define WATER_WHITE_EXPONENT 1.6
#define WATER_GRADIENT_EXPONENT 1.8
#define WATER_Y_OFFSET 0.3
#define WATER_Y_HEIGHT (1-WATER_Y_OFFSET)
