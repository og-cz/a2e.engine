#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace a2e {

class Texture {
public:
    Texture(int width, int height, std::vector<std::uint32_t> pixels)
        : width_(width), height_(height), pixels_(std::move(pixels)) {
        if (width <= 0 || height <= 0) throw std::invalid_argument("texture dimensions must be positive");
        if (pixels_.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
            throw std::invalid_argument("texture pixel count does not match dimensions");
        }
    }

    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<std::uint32_t>& pixels() const { return pixels_; }
    std::uint32_t pixel(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) throw std::out_of_range("texture coordinates out of range");
        return pixels_[static_cast<std::size_t>(y * width_ + x)];
    }

private:
    int width_;
    int height_;
    std::vector<std::uint32_t> pixels_;
};

struct Sprite {
    std::shared_ptr<const Texture> texture;
    int source_x = 0;
    int source_y = 0;
    int source_width = 0;
    int source_height = 0;
};

} // namespace a2e
