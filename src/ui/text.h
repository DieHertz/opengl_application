#ifndef ui_text_h
#define ui_text_h

#include "widget.h"
#include "font.h"
#include "scope_exit.h"
#include <string>
#include <memory>
#include <vector>

namespace ui {

class text : public widget {
public:
    text(const std::string& str, const std::shared_ptr<font>& p_font, widget* parent = nullptr)
    : widget{parent}, str{str}, p_font{p_font} {}

    void set_string(const std::string& str) {
        this->str = str;
        update_required = true;
    }

private:
    virtual void do_draw(const GLuint program_id) override {
        glUniform4fv(glGetUniformLocation(program_id, "color"), 1, gl_value_ptr(color));
        const auto is_font_loc = glGetUniformLocation(program_id, "is_font");
        glUniform1i(is_font_loc, 1);
        scope_exit({ glUniform1i(is_font_loc, 0); });

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, p_font->get_tex_id());

        glBindVertexArray(vao_id);
        glDrawArrays(GL_TRIANGLES, 0, num_vertices);
    }

    virtual void do_update_buffers() override {
        num_vertices = str.size() * 6;
        vertices = {};
        vertices.reserve(num_vertices);
        const auto& tex_size = p_font->get_tex_size();
        const auto u_scale = 1.0f / tex_size.w;
        const auto v_scale = 1.0f / tex_size.h;

        auto cursor = pos;
        for (auto& c : str) {
            const auto& ci = p_font->get_char_info(c);

            const auto top_left = vertex_attrib{
                cursor.x + ci.xoffset, cursor.y + ci.yoffset, ci.x * u_scale, ci.y * v_scale
            };
            const auto bottom_left = vertex_attrib{
                top_left.x, top_left.y + ci.height, top_left.u, top_left.v + ci.height * v_scale
            };
            const auto bottom_right = vertex_attrib{
                bottom_left.x + ci.width, bottom_left.y, bottom_left.u + ci.width * u_scale, bottom_left.v
            };
            const auto top_right = vertex_attrib{
                bottom_right.x, top_left.y, bottom_right.u, top_left.v
            };

            vertices.push_back(top_left);
            vertices.push_back(bottom_left);
            vertices.push_back(top_right);
            vertices.push_back(bottom_left);
            vertices.push_back(bottom_right);
            vertices.push_back(top_right);

            cursor.x += ci.xadvance;
        }

        glBindVertexArray(vao_id);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_attrib) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_attrib), nullptr);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_attrib),
            reinterpret_cast<void*>(2 * sizeof(decltype(vertex_attrib::x))));

        vertices = {};

        set_size(cursor.x - pos.x, static_cast<float>(p_font->get_line_height()));

        update_required = false;
    }

    struct vertex_attrib {
        GLfloat x, y, u, v;
    };

    std::shared_ptr<font> p_font;
    int num_vertices{};
    std::string str;
    std::vector<vertex_attrib> vertices;
};

} /* namespace ui */

#endif /* ui_text_h */
