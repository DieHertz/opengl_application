#ifndef ui_vertical_layout_h
#define ui_vertical_layout_h

#include "widget.h"

namespace ui {

class vertical_layout : public widget {
public:
    vertical_layout(widget* parent = nullptr) : widget{parent} {}

    void set_offset(const float offset) { this->offset = offset; }

    void add_widget(widget* widget) {
        if (current_y_set) {
            current_y_set = true;
            current_y = pos.y;
        }

        widget->set_pos(pos.x + widget->get_pos().x, current_y);
        current_y += widget->get_size().h + offset;
    }

private:
    float offset{};
    float current_y{};
    bool current_y_set{};
};

} /* namespace ui */

#endif /* ui_vertical_layout_h */
