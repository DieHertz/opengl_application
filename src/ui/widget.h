#ifndef widget_h
#define widget_h

#include "gl/gl_include.h"
#include "ext.h"
#include <list>
#include <memory>

namespace ui {

class widget {
public:
    widget(widget* parent = nullptr) : parent{parent} {
        if (parent) {
            parent->children.push_back(std::unique_ptr<widget>(this));
        }

        glGenVertexArrays(1, &vao_id);
        glGenBuffers(1, &vbo_id);

		color[0] = color[1] = color[2] = color[3] = 1;
    }

    ~widget() {
        glDeleteVertexArrays(1, &vao_id);
        glDeleteBuffers(1, &vbo_id);
    }

    widget(const widget&) = delete;
    widget& operator=(const widget&) = delete;

    void draw() {
        if (update_required) {
            update_buffers();
            update_required = false;
        }

        do_draw();
        for (auto& child : children) child->draw();
    };

    void set_pos(const float x, const float y) {
        this->x = x;
        this->y = y;
        update_required = true;
    }

    void set_size(const float w, const float h) {
        this->w = w;
        this->h = h;
        update_required = true;
    }

private:
    virtual void do_draw() {
        glUniform4fv(0, 1, color);

        glBindVertexArray(vao_id);
        glDrawArrays(GL_TRIANGLES, 0, vertex_count);
    }

    virtual void update_buffers() {
        const GLfloat data[] {
            x, y,
            x, y + h,
            x + w, y + h,
            x + w, y + h,
            x + w, y,
            x, y
        };

        glBindVertexArray(vao_id);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

        vertex_count = array_length(data) / 2;
    }

    bool update_required = true;
    GLuint vao_id;
    GLuint vbo_id;
    GLuint vertex_count;

    GLfloat color[4];
    float x, y;
    float w, h;
    widget* parent;
    std::list<std::unique_ptr<widget>> children;
};

} /* namespace ui */

#endif /* widget_h */