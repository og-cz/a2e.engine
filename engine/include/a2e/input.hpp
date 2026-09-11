#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace a2e {

enum class Key : std::size_t {
    Escape,
    Left,
    Right,
    Up,
    Down,
    Space,
    A,
    D,
    W,
    S,
    ZoomIn,
    ZoomOut,
    Count
};

enum class MouseButton : std::size_t {
    Left,
    Right,
    Middle,
    Count
};

enum class GamepadButton : std::size_t {
    South,
    East,
    West,
    North,
    Start,
    Select,
    LeftShoulder,
    RightShoulder,
    Count
};

class InputState {
public:
    void begin_frame() {
        previous_ = current_;
        previous_mouse_buttons_ = mouse_buttons_;
    }
    void set_key(Key key, bool down) { current_[index(key)] = down; }

    bool is_down(Key key) const { return current_[index(key)]; }
    bool was_down(Key key) const { return previous_[index(key)]; }
    bool pressed(Key key) const { return is_down(key) && !was_down(key); }
    bool released(Key key) const { return !is_down(key) && was_down(key); }

    void set_mouse_position(int x, int y) {
        mouse_x_ = x;
        mouse_y_ = y;
    }
    void set_mouse_button(MouseButton button, bool down) { mouse_buttons_[button_index(button)] = down; }
    int mouse_x() const { return mouse_x_; }
    int mouse_y() const { return mouse_y_; }
    bool is_mouse_down(MouseButton button) const { return mouse_buttons_[button_index(button)]; }
    bool was_mouse_down(MouseButton button) const { return previous_mouse_buttons_[button_index(button)]; }
    bool mouse_pressed(MouseButton button) const { return is_mouse_down(button) && !was_mouse_down(button); }
    bool mouse_released(MouseButton button) const { return !is_mouse_down(button) && was_mouse_down(button); }

private:
    static constexpr std::size_t index(Key key) { return static_cast<std::size_t>(key); }
    static constexpr std::size_t button_index(MouseButton button) { return static_cast<std::size_t>(button); }

    std::array<bool, static_cast<std::size_t>(Key::Count)> current_{};
    std::array<bool, static_cast<std::size_t>(Key::Count)> previous_{};
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> mouse_buttons_{};
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> previous_mouse_buttons_{};
    int mouse_x_ = 0;
    int mouse_y_ = 0;
};

class GamepadState {
public:
    void begin_frame() { previous_buttons_ = buttons_; }
    void set_button(GamepadButton button, bool down) { buttons_[button_index(button)] = down; }
    void set_left_stick(double x, double y) { left_stick_x_ = x; left_stick_y_ = y; }

    bool is_down(GamepadButton button) const { return buttons_[button_index(button)]; }
    bool pressed(GamepadButton button) const { return is_down(button) && !previous_buttons_[button_index(button)]; }
    bool released(GamepadButton button) const { return !is_down(button) && previous_buttons_[button_index(button)]; }
    double left_stick_x() const { return left_stick_x_; }
    double left_stick_y() const { return left_stick_y_; }

private:
    static constexpr std::size_t button_index(GamepadButton button) { return static_cast<std::size_t>(button); }

    std::array<bool, static_cast<std::size_t>(GamepadButton::Count)> buttons_{};
    std::array<bool, static_cast<std::size_t>(GamepadButton::Count)> previous_buttons_{};
    double left_stick_x_ = 0.0;
    double left_stick_y_ = 0.0;
};

class InputMap {
public:
    void bind(std::string action, Key key) { bindings_[std::move(action)].push_back(key); }
    void bind_mouse(std::string action, MouseButton button) {
        mouse_bindings_[std::move(action)].push_back(button);
    }
    void bind_gamepad(std::string action, GamepadButton button) {
        gamepad_bindings_[std::move(action)].push_back(button);
    }
    void clear_action(const std::string& action) {
        bindings_.erase(action);
        mouse_bindings_.erase(action);
        gamepad_bindings_.erase(action);
    }

    bool is_action_down(const InputState& input, const std::string& action) const {
        return any_key(input, action, [](const InputState& state, Key key) { return state.is_down(key); }) ||
               any_mouse(input, action, [](const InputState& state, MouseButton button) {
                   return state.is_mouse_down(button);
               });
    }

    bool is_action_pressed(const InputState& input, const std::string& action) const {
        return any_key(input, action, [](const InputState& state, Key key) { return state.pressed(key); }) ||
               any_mouse(input, action, [](const InputState& state, MouseButton button) {
                   return state.mouse_pressed(button);
               });
    }

    bool is_action_released(const InputState& input, const std::string& action) const {
        return any_key(input, action, [](const InputState& state, Key key) { return state.released(key); }) ||
               any_mouse(input, action, [](const InputState& state, MouseButton button) {
                   return state.mouse_released(button);
               });
    }

    bool is_action_down(const GamepadState& gamepad, const std::string& action) const {
        return any_gamepad(gamepad, action, [](const GamepadState& state, GamepadButton button) {
            return state.is_down(button);
        });
    }

    bool is_action_pressed(const GamepadState& gamepad, const std::string& action) const {
        return any_gamepad(gamepad, action, [](const GamepadState& state, GamepadButton button) {
            return state.pressed(button);
        });
    }

    bool is_action_released(const GamepadState& gamepad, const std::string& action) const {
        return any_gamepad(gamepad, action, [](const GamepadState& state, GamepadButton button) {
            return state.released(button);
        });
    }

private:
    template <typename Predicate>
    bool any_key(const InputState& input, const std::string& action, Predicate predicate) const {
        const auto found = bindings_.find(action);
        if (found == bindings_.end()) return false;
        for (const Key key : found->second) {
            if (predicate(input, key)) return true;
        }
        return false;
    }

    template <typename Predicate>
    bool any_mouse(const InputState& input, const std::string& action, Predicate predicate) const {
        const auto found = mouse_bindings_.find(action);
        if (found == mouse_bindings_.end()) return false;
        for (const MouseButton button : found->second) {
            if (predicate(input, button)) return true;
        }
        return false;
    }

    template <typename Predicate>
    bool any_gamepad(const GamepadState& gamepad, const std::string& action, Predicate predicate) const {
        const auto found = gamepad_bindings_.find(action);
        if (found == gamepad_bindings_.end()) return false;
        for (const GamepadButton button : found->second) {
            if (predicate(gamepad, button)) return true;
        }
        return false;
    }

    std::unordered_map<std::string, std::vector<Key>> bindings_;
    std::unordered_map<std::string, std::vector<MouseButton>> mouse_bindings_;
    std::unordered_map<std::string, std::vector<GamepadButton>> gamepad_bindings_;
};

} // namespace a2e
