#ifndef ui_slider_h
#define ui_slider_h

#include "widget.h"
#include "text.h"
#include <algorithm>
#include <functional>

const auto offset = 2;

namespace ui {

template<typename T>
class slider : public widget {
public:
    slider(
        const std::string& text, const std::shared_ptr<ui::font>& p_font,
        widget* parent = nullptr
    ) : widget{parent}, label{new ui::text{text, p_font, this}},
        value_text{new ui::text{"", p_font, this}}
    {
        label->update_buffers();
        label->set_color({ 1, 1, 1, 1 });
    }

    void set_min_max(const T min, const T max, const T val) {
        this->min = min;
        this->max = max;
        this->val = val;
        value_text->set_string(" : " + std::to_string(val));

        percentage = calc_percentage();

        update_required = true;
    }

    void set_min_max(const T min, const T max) {
        set_min_max(min, max, min + (max - min) / 2);
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
    ui::text* label;
    ui::text* value_text;
    std::function<void(T)> on_change_callback;

    float calc_percentage() const {
        return static_cast<float>(val - min) / (max - min);
    }

    virtual void do_draw(const GLuint program_id) override {
        glUniform4fv(glGetUniformLocation(program_id, "color"), 1, gl_value_ptr(color));

        glBindVertexArray(vao_id);
        glDrawArrays(GL_LINES, 0, 2);
        glDrawArrays(GL_TRIANGLES, 2, 6);
    }

    virtual void do_update_buffers() override {
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

        label->set_pos(pos.x + 1.05f * size.w, pos.y - 0.2f * size.h);
        value_text->set_pos(label->get_pos().x + 1.05f * label->get_size().w, label->get_pos().y);
    }

    virtual bool process_mouse_down(const int button, const int mods, const float x, const float y) override {
        if (button != 0) return false;

        const auto point_in_bbox = [this] (const float px, const float py) {
            return !(px < pos.x || px > pos.x + size.w || py < pos.y || py > pos.y + size.h);
        };

        dragging = point_in_bbox(x, y);
        if (dragging) cursor_at(x, y);

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

        cursor_at(x, y);

        return true;
    }

    void cursor_at(const float x, const float y) {
        const auto pos_offset = x - (pos.x + offset + size.h / 2);
        const auto effective_width = size.w - size.h - 2 * offset;

        percentage = std::max(std::min(pos_offset / effective_width, 1.0f), 0.0f);
        const auto val = static_cast<T>(min + (max - min) * percentage);

        update_required = true;
        if (this->val != val) {
            this->val = val;
            value_text->set_string(" : " + std::to_string(val));
            if (on_change_callback) on_change_callback(val);
        }
    }
};

} /* namespace ui */

#endif /* ui_slider_h */
