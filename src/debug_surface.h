#ifndef debug_surface_h
#define debug_surface_h

#include "gl/gl_include.h"
#include <glm/vec4.hpp>

class debug_surface {
    debug_surface(const debug_surface&) = delete;
    debug_surface& operator=(const debug_surface&) = delete;

    GLuint vao_id = 0;
    GLuint vbo_id = 0;
    GLuint program_id = 0;

public:
    debug_surface() = default;
    ~debug_surface();

    void init();

    void draw(GLuint tex_id, glm::vec4 rect) const;
};

#endif /* debug_surface_h */
