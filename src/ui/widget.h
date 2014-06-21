#ifndef ui_widget_h
#define ui_widget_h

#include "types.h"
#include "gl/gl_include.h"
#include "ext.h"
#include <vector>
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
        pos = { x, y };
        update_required = true;
    }
    ui::point get_pos() const { return pos; }

    void set_size(const float w, const float h) {
        size = { w, h };
        update_required = true;
    }
    ui::size get_size() const { return size; }

    bool on_mouse_down(const int button, const int mods, const float x, const float y) {
        for (auto& child : children) {
            if (child->on_mouse_down(button, mods, x, y)) return true;
        }

        return process_mouse_down(button, mods, x, y);
    }

    bool on_mouse_up(const int button, const int mods, const float x, const float y) {
        for (auto& child : children) {
            if (child->on_mouse_up(button, mods, x, y)) return true;
        }

        return process_mouse_up(button, mods, x, y);
    }

    bool on_mouse_move(const float x, const float y) {
        for (auto& child : children) {
            if (child->on_mouse_move(x, y)) return true;
        }

        return process_mouse_move(x, y);
    }

private:
    virtual void do_draw() {
        glUniform4fv(4, 1, gl_value_ptr(color));

        glBindVertexArray(vao_id);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    virtual void update_buffers() {
        const GLfloat vertices[] {
            pos.x, pos.y,
            pos.x, pos.y + size.h,
            pos.x + size.w, pos.y + size.h,
            pos.x + size.w, pos.y + size.h,
            pos.x + size.w, pos.y,
            pos.x, pos.y
        };

        glBindVertexArray(vao_id);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    virtual bool process_mouse_down(const int button, const int mods, const float x, const float y) {
        return false;
    }

    virtual bool process_mouse_up(const int button, const int mods, const float x, const float y) {
        return false;
    }

    virtual bool process_mouse_move(const float x, const float y) {
        return false;
    }

protected:
    bool update_required = true;
    GLuint vao_id;
    GLuint vbo_id;

    ui::color color = ui::color{ 0, 0, 0, 0.7f };
    ui::point pos = ui::point{};
    ui::size size = ui::size{};

private:
    widget* parent;
    std::vector<std::unique_ptr<widget>> children;
};

} /* namespace ui */

#endif /* ui_widget_h */
