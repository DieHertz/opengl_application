#ifndef ui_font_h
#define ui_font_h

#include "gl/util.h"
#include <cstdio>
#include <unordered_map>
#include <istream>
#include <string>
#include <iostream>

namespace ui {

template<typename T> struct char_info {
    T code;
    int x, y;
    int width, height;
    int xoffset, yoffset;
    int xadvance;
};

class font {
public:
    font(std::istream& in) {
        read_from_stream(in);
    }

    ~font() {
        glDeleteTextures(1, &tex_id);
    }

    const char_info<char>& get_char_info(char c) const {
        auto it = charmap.find(c);
        return it != charmap.end() ? it->second : it_default->second;
    }

    GLuint get_tex_id() const { return tex_id; }
    ui::size get_tex_size() const { return { static_cast<float>(scale_w), static_cast<float>(scale_h) }; }
    int get_line_height() const { return line_height; }



private:
    void read_from_stream(std::istream& in) {
        read_info(in);
        read_common(in);
        read_page(in);
        read_chars(in);

        tex_id = gl::load_png_texture(("fonts/" + file_name).data());
    }

    void read_info(std::istream& in) {
        auto buffer = std::string{};
        std::getline(in, buffer) >> std::ws;
        auto tmp = std::string(buffer.size(), 0);

        int size, bold, italic, unicode, stretchH, smooth, aa;
        int padding, spacing, outline;

        const auto result = sscanf(buffer.data(),
            "info face=\"%[^\"]\" size=%d bold=%d italic=%d charset=\"\" " \
            "unicode=%d stretchH=%d smooth=%d aa=%d padding=%d,%d,%d,%d spacing=%d,%d outline=%d",
            &tmp[0], &size, &bold, &italic, &unicode, &stretchH, &smooth, &aa,
            &padding, &padding, &padding, &padding, &spacing, &spacing, &outline
        );
        if (result != 15) throw std::runtime_error{"could not parse .fnt info line"};

        face_name = tmp;
    }

    void read_common(std::istream& in) {
        auto buffer = std::string{};
        std::getline(in, buffer) >> std::ws;

        int pages, packed, alpha_chnl, red_chnl, green_chnl, blue_chnl;

        const auto result = sscanf(buffer.data(),
            "common lineHeight=%d base=%d scaleW=%d scaleH=%d pages=%d packed=%d " \
            "alphaChnl=%d redChnl=%d greenChnl=%d blueChnl=%d",
            &line_height, &base, &scale_w, &scale_h, &pages, &packed,
            &alpha_chnl, &red_chnl, &green_chnl, &blue_chnl
        );
        if (result != 10) throw std::runtime_error{"could not parse .fnt common line"};
        if (pages != 1) throw std::runtime_error{"only single-paged fonts supported"};
    }

    void read_page(std::istream& in) {
        auto str = std::string{};
        
        in >> str >> std::ws;
        if (str != "page") throw std::runtime_error{"could not parse .fnt page line"};

        in >> str >> std::ws;
        std::getline(in, str);

        const auto quote_pos = str.find_first_of('"');
        file_name = str.substr(quote_pos + 1, str.find_last_of('"') - quote_pos - 1);
    }

    void read_chars(std::istream& in) {
        auto buffer = std::string{};
        std::getline(in, buffer) >> std::ws;

        int chars_count;
        const auto result = sscanf(buffer.data(), "chars count=%d", &chars_count);
        if (result != 1) throw std::runtime_error{"could not parse .fnt chars line"};

        for (auto i = 0; i < chars_count; ++i) {
            std::getline(in, buffer) >> std::ws;
            auto ci = char_info<char>{};

            int dummy;

            const auto result = sscanf(buffer.data(),
                "char id=%d\tx=%d\ty=%d\twidth=%d\theight=%d\txoffset=%d\tyoffset=%d\txadvance=%d\t" \
                "page=%d\tchnl=%d",
                &ci.code, &ci.x, &ci.y, &ci.width, &ci.height, &ci.xoffset, &ci.yoffset,
                &ci.xadvance, &dummy, &dummy
            );
            if (result != 10) throw std::runtime_error{"could not parse .fnt char line"};

            charmap.emplace(ci.code, ci);
        }

        it_default = charmap.find('?');
    }

    std::unordered_map<char, char_info<char>> charmap;
    std::unordered_map<char, char_info<char>>::iterator it_default;
    std::string face_name;

    int line_height;
    int base;
    int scale_w;
    int scale_h;
    std::string file_name;
    GLuint tex_id{};
};

} /* namespace ui */

#endif /* ui_font_h */
