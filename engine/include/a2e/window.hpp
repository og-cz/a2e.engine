#pragma once

#include "a2e/input.hpp"
#include "a2e/texture.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace a2e {

class RenderTarget {
public:
    virtual ~RenderTarget() = default;
    virtual std::int32_t width() const = 0;
    virtual std::int32_t height() const = 0;
    virtual void clear(std::uint32_t color) = 0;
    virtual void fill_rectangle(double left, double top, double right, double bottom, std::uint32_t color) = 0;
    virtual void fill_polygon(const std::vector<std::pair<double, double>>& points,
                              std::uint32_t color) = 0;
    virtual void draw_texture(const Texture& texture, int source_x, int source_y,
                              int source_width, int source_height,
                              double left, double top, double right, double bottom) = 0;
    virtual void present() = 0;
};

class Window : public RenderTarget {
public:
    virtual ~Window() = default;
    virtual bool process_events() = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;
    virtual const InputState& input() const = 0;
    virtual std::int32_t width() const = 0;
    virtual std::int32_t height() const = 0;
};

std::unique_ptr<Window> create_window(std::int32_t width, std::int32_t height,
                                      const std::string& title, std::uint32_t background_color);

} // namespace a2e
