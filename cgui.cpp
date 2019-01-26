//
// Project: cliblisp
// Created by bajdcc
//

#include <GL/freeglut.h>
#include <regex>
#include <iostream>
#include <fstream>
#include <sstream>
#include "cgui.h"
#include "cexception.h"

#define LOG_AST 0

#define ENTRY_FILE "/sys/entry"

extern int g_argc;
extern char **g_argv;

namespace clib {

    cgui::cgui() {
        buffer.resize(GUI_SIZE);
        std::fill(buffer.begin(), buffer.end(), 0);
    }

    cgui &cgui::singleton() {
        static clib::cgui gui;
        return gui;
    }

    string_t cgui::load_file(const string_t &name) {
        static string_t pat_path{ R"((/[A-Za-z0-9_]+)+)" };
        static std::regex re_path(pat_path);
        static string_t pat_bin{ R"([A-Za-z0-9_]+)" };
        static std::regex re_bin(pat_bin);
        std::smatch res;
        string_t path;
        if (std::regex_match(name, res, re_path)) {
            path = "../code" + res[0].str() + ".cpp";

        } else if (std::regex_match(name, res, re_bin)) {
            path = "../code/bin/" + res[0].str() + ".cpp";
        }
        if (path.empty())
            error("file not exists: " + name);
        std::ifstream t(path);
        if (t) {
            std::stringstream buffer;
            buffer << t.rdbuf();
            return buffer.str();
        }
        error("file not exists: " + name);
    }

    void cgui::draw(bool paused) {
        if (!paused) {
            for (int i = 0; i < ticks; ++i) {
                tick();
            }
        }
        draw_text();
    }

    void cgui::draw_text() {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        int w = glutGet(GLUT_WINDOW_WIDTH);
        int h = glutGet(GLUT_WINDOW_HEIGHT);
        int width = cols * GUI_FONT_W;
        int height = rows * GUI_FONT_H;
        gluOrtho2D(0, w, h, 0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glColor3f(0.9f, 0.9f, 0.9f);
        int x = std::max((w - width) / 2, 0);
        int y = std::max((h - height) / 2, 0);

        for (auto i = 0; i < rows; ++i) {
            glRasterPos2i(x, y);
            for (auto j = 0; j < cols; ++j) {
                if (std::isprint(buffer[i * cols + j]))
                    glutBitmapCharacter(GUI_FONT, buffer[i * cols + j]);
                else
                    glutBitmapCharacter(GUI_FONT, ' ');
            }
            y += GUI_FONT_H;
        }

        if (input_state) {
            input_ticks++;
            if (input_ticks > GUI_INPUT_CARET) {
                input_caret = !input_caret;
                input_ticks = 0;
            }
            if (input_caret) {
                if (ptr_x <= cols - 1) {
                    int _x = std::max((w - width) / 2, 0) + ptr_x * GUI_FONT_W;
                    int _y = std::max((h - height) / 2, 0) + ptr_y * GUI_FONT_H;
                    glRasterPos2i(_x, _y);
                    glutBitmapCharacter(GUI_FONT, '_');
                } else if (ptr_y < rows - 1) {
                    int _x = std::max((w - width) / 2, 0);
                    int _y = std::max((h - height) / 2, 0) + (ptr_y + 1) * GUI_FONT_H;
                    glRasterPos2i(_x, _y);
                    glutBitmapCharacter(GUI_FONT, '_');
                }
            }
        }

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }

    void cgui::tick() {
        auto c = 0;
        if (exited)
            return;
        if (running) {
            try {
                if (!vm->run(cycle, c)) {
                    running = false;
                    exited = true;
                    put_string("\n[!] clibos exited.");
                    vm.reset();
                    gen.reset();
                }
            } catch (const cexception &e) {
                std::cout << "[SYSTEM] ERR  | RUNTIME ERROR: " << e.message() << std::endl;
                vm.reset();
                gen.reset();
                running = false;
            }
        } else {
            if (!vm) {
                vm = std::make_unique<cvm>();
                std::vector<string_t> args;
                if (g_argc > 0) {
                    args.emplace_back(ENTRY_FILE);
                    for (int i = 1; i < g_argc; ++i) {
                        args.emplace_back(g_argv[i]);
                    }
                }
                if (compile(ENTRY_FILE, args) != -1) {
                    running = true;
                }
            }
        }
    }

    void cgui::put_string(const string_t &str) {
        for (auto &s : str) {
            put_char(s);
        }
    }

    void cgui::put_char(char c) {
        if (c == 0)
            return;
        if (c == '\n') {
            if (ptr_y == rows - 1) {
                new_line();
            } else {
                ptr_x = 0;
                ptr_y++;
            }
        } else if (c == '\b') {
            if (ptr_mx + ptr_my * cols < ptr_x + ptr_y * cols) {
                if (ptr_y == 0) {
                    if (ptr_x != 0) {
                        ptr_x--;
                        draw_char('\u0000');
                    }
                } else {
                    if (ptr_x != 0) {
                        ptr_x--;
                        draw_char('\u0000');
                    } else {
                        ptr_x = cols - 1;
                        ptr_y--;
                        draw_char('\u0000');
                    }
                }
            }
        } else if (c == '\u0002') {
            ptr_x--;
            while (ptr_x >= 0) {
                draw_char('\u0000');
                ptr_x--;
            }
            ptr_x = 0;
        } else if (c == '\r') {
            ptr_x = 0;
        } else if (c == '\f') {
            ptr_x = 0;
            ptr_y = 0;
            ptr_mx = 0;
            ptr_my = 0;
            std::fill(buffer.begin(), buffer.end(), 0);
        } else if (ptr_x == cols - 1) {
            if (ptr_y == rows - 1) {
                draw_char(c);
                new_line();
            } else {
                draw_char(c);
                ptr_x = 0;
                ptr_y++;
            }
        } else {
            draw_char(c);
            ptr_x++;
        }
    }

    void cgui::put_int(int number) {
        static char str[256];
        sprintf(str, "%d", number);
        auto s = str;
        while (*s)
            put_char(*s++);
    }

    void cgui::put_hex(int number) {
        static char str[256];
        sprintf(str, "0x%p", (void*) number);
        auto s = str;
        while (*s)
            put_char(*s++);
    }

    void cgui::new_line() {
        ptr_x = 0;
        for (int i = 0; i < rows - 1; ++i) {
            std::copy(buffer.begin() + (cols * (i + 1)),
                      buffer.begin() + (cols * (i + 2)),
                      buffer.begin() + (cols * (i)));
        }
        std::fill(buffer.begin() + (cols * (rows - 1)),
                  buffer.begin() + (cols * (rows)),
                  0);
    }

    void cgui::draw_char(const char &c) {
        buffer[ptr_y * cols + ptr_x] = c;
    }

    void cgui::error(const string_t &str) {
        throw cexception(ex_gui, str);
    }

    void cgui::set_cycle(int cycle) {
        this->cycle = cycle;
    }

    void cgui::set_ticks(int ticks) {
        this->ticks = ticks;
    }

    void cgui::resize(int r, int c) {
        auto old_rows = rows;
        auto old_cols = cols;
        rows = std::max(10, std::min(r, 60));
        cols = std::max(20, std::min(c, 200));
        printf("[SYSTEM] GUI  | Resize: from (%d, %d) to (%d, %d)\n", old_rows, old_cols, rows, cols);
        size = rows * cols;
        auto old_buffer = buffer;
        buffer.resize((uint) size);
        std::fill(buffer.begin(), buffer.end(), 0);
        auto min_rows = std::min(old_rows, rows);
        auto min_cols = std::min(old_cols, cols);
        auto delta_rows = old_rows - min_rows;
        for (int i = 0; i < min_rows; ++i) {
            for (int j = 0; j < min_cols; ++j) {
                buffer[i * cols + j] = old_buffer[(delta_rows + i) * old_cols + j];
            }
        }
        ptr_x = std::min(ptr_x, cols);
        ptr_y = std::min(ptr_y, rows);
        ptr_mx = std::min(ptr_mx, cols);
        ptr_my = std::min(ptr_my, rows);
    }

    int cgui::compile(const string_t &path, const std::vector<string_t> &args) {
        if (path.empty())
            return -1;
        auto fail_errno = -1;
        try {
            auto c = cache.find(path);
            if (c != cache.end()) {
                return vm->load(c->second, args);
            }
            auto code = load_file(path);
            fail_errno = -2;
            gen.reset();
            auto root = p.parse(code, &gen);
#if LOG_AST
            cast::print(root, 0, std::cout);
#endif
            gen.gen(root);
            auto file = gen.file();
            cache.insert(std::make_pair(path, file));
            return vm->load(file, args);
        } catch (const cexception &e) {
            gen.reset();
            std::cout << "[SYSTEM] ERR  | PATH: " << path << ", ";
            std::cout << e.message() << std::endl;
            return fail_errno;
        }
    }

    void cgui::input_set(bool valid) {
        if (valid) {
            input_state = true;
            ptr_mx = ptr_x;
            ptr_my = ptr_y;
        } else {
            input_state = false;
            ptr_mx = -1;
            ptr_my = -1;
        }
        input_ticks = 0;
        input_caret = false;
        input_string.clear();
    }

    void cgui::input(unsigned char c) {
        if (!input_state)
            return;
        if (c == '\0') {
            return;
        }
        if (c == '\b') {
            if (!input_string.empty()) {
                put_char('\b');
                input_string.pop_back();
            }
            return;
        }
        if (c == '\r') {
            put_char('\n');
            cvm::global_state.input_content = string_t(input_string.begin(), input_string.end());
            cvm::global_state.input_read_ptr = 0;
            cvm::global_state.input_success = true;
            return;
        }
        put_char(c);
        input_string.push_back(c);
    }
}
