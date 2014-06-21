#ifndef ui_types_h
#define ui_types_h

namespace ui {

struct point {
    float x, y;
};

struct size {
    float w, h;
};

struct color {
    float r, g, b, a;
};

inline const float* gl_value_ptr(const point& p) { return &p.x; }
inline const float* gl_value_ptr(const size& s) { return &s.w; }
inline const float* gl_value_ptr(const color& c) { return &c.r; }

} /* namespace ui */

#endif /* ui_types_h */
