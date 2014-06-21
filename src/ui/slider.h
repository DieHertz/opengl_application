#ifndef ui_slider_h
#define ui_slider_h

#include "widget.h"
#include <algorithm>
#include <functional>

const auto offset = 2;

namespace ui {

template<typename T>
class slider : public widget {
public:
    slider(widget* parent = nullptr) : widget(parent) {}

    void set_min_max(const T min, const T max, const T val = min + (max - min) / 2) {
        this->min = min;
        this->max = max;
        this->val = val;

        percentage = calc_percentage();

        update_required = true;
    }

    T get_value() const { return val; }

    template<typename Function>
    void on_change(Function&& fn) {
        on_change_callback = std::forward<Function>(fn);
    }

private:
    T min, max, val;
    float percentage;
    bool dragging;
    std::function<void(T)> on_change_callback;

    float calc_percentage() const {
        return static_cast<float>(val - min) / (max - min);
    }

    virtual void do_draw() override {
		glUniform4fv(0, 1, gl_value_ptr(color));
        glBindVertexArray(vao_id);
        glDrawArrays(GL_LINES, 0, 2);
        glDrawArrays(GL_TRIANGLES, 2, 6);
    }

    virtual void update_buffers() override {
        const auto bullet_x = pos.x + offset;
        const auto bullet_y = pos.y + offset;
        const auto bullet_size = size.h - 2 * offset;

        const auto effective_width = size.w - bullet_size - 2 * offset;
        const auto value_pos = effective_width * percentage;

        const GLfloat vertices[] {
            pos.x, pos.y + size.h / 2,
            pos.x + size.w, pos.y + size.h / 2,

            bullet_x + value_pos, bullet_y,
            bullet_x + value_pos, bullet_y + bullet_size,
            bullet_x + bullet_size + value_pos, bullet_y + bullet_size,
            bullet_x + bullet_size + value_pos, bullet_y + bullet_size,
            bullet_x + bullet_size + value_pos, bullet_y,
            bullet_x + value_pos, bullet_y,
        };

        glBindVertexArray(vao_id);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    virtual bool process_mouse_down(const int button, const int mods, const float x, const float y) override {
        if (button != 0) return false;

        const auto point_in_bbox = [this] (const float px, const float py) {
            return !(px < pos.x || px > pos.x + size.w || py < pos.y || py > pos.y + size.h);
        };

        dragging = point_in_bbox(x, y);

        return dragging;
    }

    virtual bool process_mouse_up(const int button, const int mods, const float x, const float y) override {
        if (!dragging) return false;

        percentage = calc_percentage();
        update_required = true;

        dragging = false;
        return true;
    }

    virtual bool process_mouse_move(const float x, const float y) override {
        if (!dragging) return false;

        const auto pos_offset = x - (pos.x + offset + size.h / 2);
        const auto effective_width = size.w - size.h - 2 * offset;

        percentage = std::max(std::min(pos_offset / effective_width, 1.0f), 0.0f);
		const auto val = static_cast<T>(min + (max - min) * percentage);

        update_required = true;
        if (this->val != val) {
            this->val = val;
            if (on_change_callback) on_change_callback(val);
        }

        return true;
    }
};

} /* namespace ui */

#endif /* ui_slider_h */
