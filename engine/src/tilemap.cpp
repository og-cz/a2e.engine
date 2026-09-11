#include "a2e/tilemap.hpp"

#include <stdexcept>

namespace a2e {

TileMap::TileMap(int width, int height, double tile_size, std::uint32_t empty_color)
    : width_(width), height_(height), tile_size_(tile_size) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("tilemap dimensions must be positive");
    if (tile_size <= 0.0) throw std::invalid_argument("tile size must be positive");
    tiles_.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), empty_color);
}

void TileMap::set_tile(int x, int y, std::uint32_t color) { tiles_[index(x, y)] = color; }

std::uint32_t TileMap::tile(int x, int y) const { return tiles_[index(x, y)]; }

std::size_t TileMap::index(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) throw std::out_of_range("tile coordinates out of range");
    return static_cast<std::size_t>(y * width_ + x);
}

} // namespace a2e
