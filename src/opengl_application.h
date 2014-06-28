#ifndef opengl_application_h
#define opengl_application_h
#include "gl/gl_include.h"
#include <GLFW/glfw3.h>
#include <cassert>
#include <stdexcept>

template<class Handler> class opengl_application {
    GLFWwindow* p_window;
    Handler handler;

public:
    opengl_application(
        const int ogl_ver_major, const int ogl_ver_minor,
        const int width, const int height, const int samples = 1,
        const char* caption = ""
    ) {
        if (!glfwInit()) throw std::runtime_error{"glfwInit() failed"};

        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, ogl_ver_major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, ogl_ver_minor);
        glfwWindowHint(GLFW_SAMPLES, samples);

        p_window = glfwCreateWindow(width, height, caption, nullptr, nullptr);
        if (!p_window) throw std::runtime_error{"glfwCreateWindow() failed"};

        const auto ver_major = glfwGetWindowAttrib(p_window, GLFW_CONTEXT_VERSION_MAJOR);
        const auto ver_minor = glfwGetWindowAttrib(p_window, GLFW_CONTEXT_VERSION_MINOR);
        if (ver_major < ogl_ver_major || ver_major == ogl_ver_major && ver_minor < ogl_ver_minor) {
            throw std::runtime_error{"could not create opengl context of requested version"};
        }

        glfwSetWindowUserPointer(p_window, &handler);
        glfwSetCursorPosCallback(p_window, [] (GLFWwindow* w, double x, double y) {
            auto p_handler = static_cast<Handler*>(glfwGetWindowUserPointer(w));
            assert(p_handler);
            p_handler->onCursorMove(static_cast<float>(x), static_cast<float>(y));
        });
        glfwSetScrollCallback(p_window, [] (GLFWwindow* w, double dx, double dy) {
            auto p_handler = static_cast<Handler*>(glfwGetWindowUserPointer(w));
            assert(p_handler);
			p_handler->onScroll(static_cast<float>(dx), static_cast<float>(dy));
        });
        glfwSetMouseButtonCallback(p_window, [] (GLFWwindow* w, int button, int action, int mods) {
            auto p_handler = static_cast<Handler*>(glfwGetWindowUserPointer(w));
            assert(p_handler);
            double x, y;
            glfwGetCursorPos(w, &x, &y);
			p_handler->onMouseButton(button, action, mods, static_cast<float>(x), static_cast<float>(y));
        });
        glfwSetFramebufferSizeCallback(p_window, [] (GLFWwindow* w, int width, int height) {
            auto p_handler = static_cast<Handler*>(glfwGetWindowUserPointer(w));
            assert(p_handler);

            int window_width, window_height;
            glfwGetWindowSize(w, &window_width, &window_height);

            p_handler->onFramebufferResize(width, height, window_width, window_height);
        });

        glfwMakeContextCurrent(p_window);

        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) throw std::runtime_error{"glewInit() failed"};

        int fb_width, fb_height;
        int window_width, window_height;
        glfwGetFramebufferSize(p_window, &fb_width, &fb_height);
        glfwGetWindowSize(p_window, &window_width, &window_height);

        handler.onContextCreated(fb_width, fb_height, window_width, window_height);
        handler.onFramebufferResize(fb_width, fb_height, window_width, window_height);
    }

    ~opengl_application() {
        glfwDestroyWindow(p_window);
        glfwTerminate();
    }

    void run() {
        auto last_timestamp = glfwGetTime();
        while (!glfwWindowShouldClose(p_window)) {
            const auto elapsed = glfwGetTime() - last_timestamp;
            last_timestamp += elapsed;
			handler.onUpdate(static_cast<float>(last_timestamp), static_cast<float>(elapsed));
            handler.onRender();

            glfwSwapBuffers(p_window);
            glfwPollEvents();
        }
    }
};

#endif /* opengl_application_h */
