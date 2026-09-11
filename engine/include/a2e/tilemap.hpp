#pragma once

#include <cstdint>
#include <vector>

namespace a2e {

class TileMap {
public:
    TileMap(int width, int height, double tile_size, std::uint32_t empty_color = 0);

    int width() const { return width_; }
    int height() const { return height_; }
    double tile_size() const { return tile_size_; }
    void set_tile(int x, int y, std::uint32_t color);
    std::uint32_t tile(int x, int y) const;

private:
    std::size_t index(int x, int y) const;

    int width_;
    int height_;
    double tile_size_;
    std::vector<std::uint32_t> tiles_;
};

} // namespace a2e
