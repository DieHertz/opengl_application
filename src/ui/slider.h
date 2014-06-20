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
    slider(widget* parent = nullptr) : widget(parent) {
    }

    void set_min_max(const T min, const T max, const T val = min + (max - min) / 2) {
        this->min = min;
        this->max = max;
        this->val = val;

        update_required = true;
    }

    T get_value() const { return val; }

    template<typename Function>
    void on_change(Function&& fn) {
        on_change_callback = std::forward<Function>(fn);
    }

private:
    T min, max, val;
    bool dragging;
    std::function<void(T)> on_change_callback;

    virtual void do_draw() override {
        glUniform4fv(0, 1, color);

        glBindVertexArray(vao_id);
        glDrawArrays(GL_LINES, 0, 4);
        glDrawArrays(GL_TRIANGLES, 4, 6);
    }

    virtual void update_buffers() override {
        const auto bullet_x = x + offset;
        const auto bullet_y = y + offset;
        const auto bullet_size = h - 2 * offset;

        const auto percentage = static_cast<float>(val - min) / (max - min);
        const auto effective_width = w - bullet_size - 2 * offset;

        const auto value_pos = effective_width * percentage;

        const GLfloat vertices[] {
            x, y,
            x + w, y,
            x, y + h,
            x + w, y + h,

            bullet_x + value_pos, bullet_y - 1,
            bullet_x + value_pos, bullet_y + bullet_size,
            bullet_x + bullet_size + value_pos + 1, bullet_y + bullet_size,
            bullet_x + bullet_size + value_pos + 1, bullet_y + bullet_size,
            bullet_x + bullet_size + value_pos + 1, bullet_y - 1,
            bullet_x + value_pos, bullet_y - 1,
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
            return !(px < this->x || px > this->x + w || py < this->y || py > this->y + h);
        };

        dragging = point_in_bbox(x, y);

        return dragging;
    }

    virtual bool process_mouse_up(const int button, const int mods, const float x, const float y) override {
        if (!dragging) return false;

        dragging = false;
        return true;
    }

    virtual bool process_mouse_move(const float x, const float y) override {
        if (!dragging) return false;

        const auto pos_offset = x - (this->x + offset + h / 2);
        const auto effective_width = w - h - 2 * offset;

        const auto percentage = std::max(std::min(pos_offset / effective_width, 1.0f), 0.0f);
		const auto val = static_cast<T>(min + (max - min) * percentage);

        if (this->val != val) {
            this->val = val;
            update_required = true;
            if (on_change_callback) on_change_callback(val);
        }

        return true;
    }
};

} /* namespace ui */

#endif /* ui_slider_h */
