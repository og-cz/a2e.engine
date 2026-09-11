#include "a2e/window.hpp"

#include <stdexcept>
#include <optional>
#include <utility>

#ifdef _WIN32
#include <windows.h>

namespace a2e {
namespace {

COLORREF to_colorref(std::uint32_t color) {
    const auto red = static_cast<BYTE>((color >> 16) & 0xff);
    const auto green = static_cast<BYTE>((color >> 8) & 0xff);
    const auto blue = static_cast<BYTE>(color & 0xff);
    return RGB(red, green, blue);
}

class Win32Window final : public Window {
public:
    Win32Window(std::int32_t width, std::int32_t height, const std::string& title,
                std::uint32_t background_color)
        : width_(width), height_(height), background_color_(background_color) {
        static const wchar_t class_name[] = L"A2EWindowClass";
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = &Win32Window::window_proc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.lpszClassName = class_name;
        window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&window_class);

        std::wstring wide_title(title.begin(), title.end());
        hwnd_ = CreateWindowExW(0, class_name, wide_title.c_str(), WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, width_, height_, nullptr, nullptr,
                                window_class.hInstance, this);
        if (!hwnd_) throw std::runtime_error("could not create A2E Win32 window");
        dc_ = GetDC(hwnd_);
        backbuffer_ = CreateCompatibleDC(dc_);
        bitmap_ = CreateCompatibleBitmap(dc_, width_, height_);
        previous_bitmap_ = static_cast<HBITMAP>(SelectObject(backbuffer_, bitmap_));
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
    }

    ~Win32Window() override { close(); }

    bool process_events() override {
        input_.begin_frame();
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                open_ = false;
                return false;
            }
            if (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN) {
                if (const auto key = to_key(message.wParam)) input_.set_key(*key, true);
            } else if (message.message == WM_KEYUP || message.message == WM_SYSKEYUP) {
                if (const auto key = to_key(message.wParam)) input_.set_key(*key, false);
            } else if (message.message == WM_MOUSEMOVE) {
                input_.set_mouse_position(static_cast<short>(LOWORD(message.lParam)),
                                          static_cast<short>(HIWORD(message.lParam)));
            } else if (message.message == WM_LBUTTONDOWN || message.message == WM_LBUTTONUP ||
                       message.message == WM_RBUTTONDOWN || message.message == WM_RBUTTONUP ||
                       message.message == WM_MBUTTONDOWN || message.message == WM_MBUTTONUP) {
                const bool down = message.message == WM_LBUTTONDOWN || message.message == WM_RBUTTONDOWN ||
                                  message.message == WM_MBUTTONDOWN;
                input_.set_mouse_button(mouse_button(message.message), down);
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return open_;
    }

    void clear(std::uint32_t color) override {
        RECT rect{};
        rect.right = width_;
        rect.bottom = height_;
        HBRUSH brush = CreateSolidBrush(to_colorref(color));
        FillRect(backbuffer_, &rect, brush);
        DeleteObject(brush);
    }

    void fill_rectangle(double left, double top, double right, double bottom, std::uint32_t color) override {
        RECT rect{static_cast<LONG>(left), static_cast<LONG>(top), static_cast<LONG>(right), static_cast<LONG>(bottom)};
        HBRUSH brush = CreateSolidBrush(to_colorref(color));
        FillRect(backbuffer_, &rect, brush);
        DeleteObject(brush);
    }

    void fill_polygon(const std::vector<std::pair<double, double>>& points, std::uint32_t color) override {
        std::vector<POINT> native_points;
        native_points.reserve(points.size());
        for (const auto& [x, y] : points) native_points.push_back({static_cast<LONG>(x), static_cast<LONG>(y)});
        HBRUSH brush = CreateSolidBrush(to_colorref(color));
        SelectObject(backbuffer_, brush);
        Polygon(backbuffer_, native_points.data(), static_cast<int>(native_points.size()));
        SelectObject(backbuffer_, GetStockObject(NULL_BRUSH));
        DeleteObject(brush);
    }

    void present() override {
        BitBlt(dc_, 0, 0, width_, height_, backbuffer_, 0, 0, SRCCOPY);
    }

    void close() override {
        if (!open_) return;
        open_ = false;
        if (bitmap_) {
            SelectObject(backbuffer_, previous_bitmap_);
            DeleteObject(bitmap_);
            bitmap_ = nullptr;
        }
        if (backbuffer_) {
            DeleteDC(backbuffer_);
            backbuffer_ = nullptr;
        }
        if (dc_) {
            ReleaseDC(hwnd_, dc_);
            dc_ = nullptr;
        }
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    bool is_open() const override { return open_; }
    const InputState& input() const override { return input_; }
    std::int32_t width() const override { return width_; }
    std::int32_t height() const override { return height_; }

private:
    static std::optional<Key> to_key(WPARAM key) {
        switch (key) {
        case VK_ESCAPE: return Key::Escape;
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_SPACE: return Key::Space;
        case 'A': return Key::A;
        case 'D': return Key::D;
        case 'W': return Key::W;
        case 'S': return Key::S;
        case VK_OEM_PLUS:
        case VK_ADD: return Key::ZoomIn;
        case VK_OEM_MINUS:
        case VK_SUBTRACT: return Key::ZoomOut;
        default: return std::nullopt;
        }
    }

    static MouseButton mouse_button(UINT message) {
        if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) return MouseButton::Right;
        if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) return MouseButton::Middle;
        return MouseButton::Left;
    }

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<Win32Window*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self && message == WM_CLOSE) {
            self->open_ = false;
            DestroyWindow(hwnd);
            return 0;
        }
        if (message == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    HWND hwnd_ = nullptr;
    HDC dc_ = nullptr;
    HDC backbuffer_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HBITMAP previous_bitmap_ = nullptr;
    std::int32_t width_;
    std::int32_t height_;
    std::uint32_t background_color_;
    InputState input_;
    bool open_ = true;
};

} // namespace

std::unique_ptr<Window> create_window(std::int32_t width, std::int32_t height,
                                      const std::string& title, std::uint32_t background_color) {
    return std::make_unique<Win32Window>(width, height, title, background_color);
}

} // namespace a2e
#else

namespace a2e {
std::unique_ptr<Window> create_window(std::int32_t, std::int32_t, const std::string&, std::uint32_t) {
    throw std::runtime_error("A2E Phase 1 window backend requires Windows");
}
} // namespace a2e
#endif
