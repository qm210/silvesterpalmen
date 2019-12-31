struct vec2
{
    float x,y;
    vec2(): x(0), y(0) {}
    vec2(float x, float y): x(x), y(y) {}
    float get_distance_to(vec2 v) {return sqrt(pow(v.x-x,2) + pow(v.y-y,2));}
};

const float distance_LED_in_cm = 100. / 60.;

struct Segment
{
    float origin_x;
    float origin_y;
    float direction; // 0 to 360 degrees
    int pixels;
    int type = 0;
    Segment(float x, float y, float to_x, float to_y) {set(x, y, to_x, to_y);}
    Segment(float x, float y, float to_x, float to_y, int _type) {set(x, y, to_x, to_y); type = _type;}
    void set(float x, float y, float to_x, float to_y)
    {
        x += originOffsetX;
        y += originOffsetY;
        to_x += originOffsetX;
        to_y += originOffsetY;
        set_origin(x, y);
        set_direction_to(to_x, to_y);
        set_length_to(to_x, to_y);
    }
    void set_origin(float x, float y) {origin_x = x; origin_y = y;}
    void set_direction(float dir) {direction = fmod(dir + 360., 360.);}
    void set_direction_to(float to_x, float to_y) {set_direction(180./PI*atan2(origin_y - to_y, to_x - origin_x));}
    void set_length(float l) {pixels = (int)(round((l + 1) / distance_LED_in_cm));}
    void set_length_to(float to_x, float to_y) {set_length(sqrt(pow(to_x - origin_x, 2) + pow(to_y - origin_y, 2)));}
    float get_length() {return pixels * distance_LED_in_cm;}
    vec2 get_pixel(int i)
    {
        return vec2(
            origin_x + i * distance_LED_in_cm * cos(direction * PI/180.),
            origin_y - i * distance_LED_in_cm * sin(direction * PI/180.)
        );
    }
    vec2 get_last_pixel_plus_one() {return get_pixel(pixels);}
    vec2 get_last_pixel() {return get_pixel(pixels - 1);}
    float get_rightmost_x() {return direction > 90 && direction < 270 ? origin_x : get_last_pixel_plus_one().x;}
    float get_bottommost_y() {return direction < 180 ? origin_y : get_last_pixel_plus_one().y;}

    void move_horizontally(float inc)
    {
        origin_x += inc;
    }
    void move_vertically(float inc)
    {
        origin_y += inc;
    }
    void turn(float inc)
    {
        set_direction(direction + inc);
    }
    void change_length(int inc)
    {
        pixels = max(pixels + inc, 1);
    }
    void flip()
    {
        set_origin(get_last_pixel_plus_one().x, get_last_pixel_plus_one().y);
        set_direction(direction + 180.);
    }
};

class Pixel
{
    public:
        float x = 0;
        float y = 0;
        LED L = LED(1, 1);
        int segcount = -1;
        
    Pixel() {}
    
    Pixel(float x, float y) : x(x), y(y) {}
    
    Pixel(vec2 v) {x = v.x; y = v.y;}

    Pixel(vec2 v, int count) {x = v.x; y = v.y; segcount = count;}
    
    Pixel(float x, float y, LED L) : x(x), y(y), L(L) {}
    
    void set(LED newL)
    {
        L = newL;
    }
    
    vec2 get_coord() {return vec2(x, y);}
};
