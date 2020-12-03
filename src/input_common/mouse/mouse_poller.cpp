// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include <utility>

#include "common/threadsafe_queue.h"
#include "input_common/mouse/mouse_input.h"
#include "input_common/mouse/mouse_poller.h"

namespace InputCommon {

class MouseButton final : public Input::ButtonDevice {
public:
    explicit MouseButton(u32 button_, const MouseInput::Mouse* mouse_input_)
        : button(button_), mouse_input(mouse_input_) {}

    bool GetStatus() const override {
        return mouse_input->GetMouseState(button).pressed;
    }

private:
    const u32 button;
    const MouseInput::Mouse* mouse_input;
};

MouseButtonFactory::MouseButtonFactory(std::shared_ptr<MouseInput::Mouse> mouse_input_)
    : mouse_input(std::move(mouse_input_)) {}

std::unique_ptr<Input::ButtonDevice> MouseButtonFactory::Create(
    const Common::ParamPackage& params) {
    const auto button_id = params.Get("button", 0);

    return std::make_unique<MouseButton>(button_id, mouse_input.get());
}

Common::ParamPackage MouseButtonFactory::GetNextInput() const {
    MouseInput::MouseStatus pad;
    Common::ParamPackage params;
    auto& queue = mouse_input->GetMouseQueue();
    while (queue.Pop(pad)) {
        // This while loop will break on the earliest detected button
        if (pad.button != MouseInput::MouseButton::Undefined) {
            params.Set("engine", "mouse");
            params.Set("button", static_cast<u16>(pad.button));
            return params;
        }
    }
    return params;
}

void MouseButtonFactory::BeginConfiguration() {
    polling = true;
    mouse_input->BeginConfiguration();
}

void MouseButtonFactory::EndConfiguration() {
    polling = false;
    mouse_input->EndConfiguration();
}

class MouseAnalog final : public Input::AnalogDevice {
public:
    explicit MouseAnalog(u32 port_, u32 axis_x_, u32 axis_y_, float deadzone_, float range_,
                         const MouseInput::Mouse* mouse_input_)
        : button(port_), axis_x(axis_x_), axis_y(axis_y_), deadzone(deadzone_), range(range_),
          mouse_input(mouse_input_) {}

    float GetAxis(u32 axis) const {
        std::lock_guard lock{mutex};
        const auto axis_value =
            static_cast<float>(mouse_input->GetMouseState(button).axis.at(axis));
        return axis_value / (100.0f * range);
    }

    std::pair<float, float> GetAnalog(u32 analog_axis_x, u32 analog_axis_y) const {
        float x = GetAxis(analog_axis_x);
        float y = GetAxis(analog_axis_y);

        // Make sure the coordinates are in the unit circle,
        // otherwise normalize it.
        float r = x * x + y * y;
        if (r > 1.0f) {
            r = std::sqrt(r);
            x /= r;
            y /= r;
        }

        return {x, y};
    }

    std::tuple<float, float> GetStatus() const override {
        const auto [x, y] = GetAnalog(axis_x, axis_y);
        const float r = std::sqrt((x * x) + (y * y));
        if (r > deadzone) {
            return {x / r * (r - deadzone) / (1 - deadzone),
                    y / r * (r - deadzone) / (1 - deadzone)};
        }
        return {0.0f, 0.0f};
    }

private:
    const u32 button;
    const u32 axis_x;
    const u32 axis_y;
    const float deadzone;
    const float range;
    const MouseInput::Mouse* mouse_input;
    mutable std::mutex mutex;
};

/// An analog device factory that creates analog devices from GC Adapter
MouseAnalogFactory::MouseAnalogFactory(std::shared_ptr<MouseInput::Mouse> mouse_input_)
    : mouse_input(std::move(mouse_input_)) {}

/**
 * Creates analog device from joystick axes
 * @param params contains parameters for creating the device:
 *     - "port": the nth gcpad on the adapter
 *     - "axis_x": the index of the axis to be bind as x-axis
 *     - "axis_y": the index of the axis to be bind as y-axis
 */
std::unique_ptr<Input::AnalogDevice> MouseAnalogFactory::Create(
    const Common::ParamPackage& params) {
    const auto port = static_cast<u32>(params.Get("port", 0));
    const auto axis_x = static_cast<u32>(params.Get("axis_x", 0));
    const auto axis_y = static_cast<u32>(params.Get("axis_y", 1));
    const auto deadzone = std::clamp(params.Get("deadzone", 0.0f), 0.0f, 1.0f);
    const auto range = std::clamp(params.Get("range", 1.0f), 0.50f, 1.50f);

    return std::make_unique<MouseAnalog>(port, axis_x, axis_y, deadzone, range, mouse_input.get());
}

void MouseAnalogFactory::BeginConfiguration() {
    polling = true;
    mouse_input->BeginConfiguration();
}

void MouseAnalogFactory::EndConfiguration() {
    polling = false;
    mouse_input->EndConfiguration();
}

Common::ParamPackage MouseAnalogFactory::GetNextInput() const {
    MouseInput::MouseStatus pad;
    Common::ParamPackage params;
    auto& queue = mouse_input->GetMouseQueue();
    while (queue.Pop(pad)) {
        // This while loop will break on the earliest detected button
        if (pad.button != MouseInput::MouseButton::Undefined) {
            params.Set("engine", "mouse");
            params.Set("port", static_cast<u16>(pad.button));
            params.Set("axis_x", 0);
            params.Set("axis_y", 1);
            return params;
        }
    }
    return params;
}

class MouseMotion final : public Input::MotionDevice {
public:
    explicit MouseMotion(u32 button_, const MouseInput::Mouse* mouse_input_)
        : button(button_), mouse_input(mouse_input_) {}

    Input::MotionStatus GetStatus() const override {
        return mouse_input->GetMouseState(button).motion;
    }

private:
    const u32 button;
    const MouseInput::Mouse* mouse_input;
};

MouseMotionFactory::MouseMotionFactory(std::shared_ptr<MouseInput::Mouse> mouse_input_)
    : mouse_input(std::move(mouse_input_)) {}

std::unique_ptr<Input::MotionDevice> MouseMotionFactory::Create(
    const Common::ParamPackage& params) {
    const auto button_id = params.Get("button", 0);

    return std::make_unique<MouseMotion>(button_id, mouse_input.get());
}

Common::ParamPackage MouseMotionFactory::GetNextInput() const {
    MouseInput::MouseStatus pad;
    Common::ParamPackage params;
    auto& queue = mouse_input->GetMouseQueue();
    while (queue.Pop(pad)) {
        // This while loop will break on the earliest detected button
        if (pad.button != MouseInput::MouseButton::Undefined) {
            params.Set("engine", "mouse");
            params.Set("button", static_cast<u16>(pad.button));
            return params;
        }
    }
    return params;
}

void MouseMotionFactory::BeginConfiguration() {
    polling = true;
    mouse_input->BeginConfiguration();
}

void MouseMotionFactory::EndConfiguration() {
    polling = false;
    mouse_input->EndConfiguration();
}

class MouseTouch final : public Input::TouchDevice {
public:
    explicit MouseTouch(u32 button_, const MouseInput::Mouse* mouse_input_)
        : button(button_), mouse_input(mouse_input_) {}

    Input::TouchStatus GetStatus() const override {
        return mouse_input->GetMouseState(button).touch;
    }

private:
    const u32 button;
    const MouseInput::Mouse* mouse_input;
};

MouseTouchFactory::MouseTouchFactory(std::shared_ptr<MouseInput::Mouse> mouse_input_)
    : mouse_input(std::move(mouse_input_)) {}

std::unique_ptr<Input::TouchDevice> MouseTouchFactory::Create(const Common::ParamPackage& params) {
    const auto button_id = params.Get("button", 0);

    return std::make_unique<MouseTouch>(button_id, mouse_input.get());
}

Common::ParamPackage MouseTouchFactory::GetNextInput() const {
    MouseInput::MouseStatus pad;
    Common::ParamPackage params;
    auto& queue = mouse_input->GetMouseQueue();
    while (queue.Pop(pad)) {
        // This while loop will break on the earliest detected button
        if (pad.button != MouseInput::MouseButton::Undefined) {
            params.Set("engine", "mouse");
            params.Set("button", static_cast<u16>(pad.button));
            return params;
        }
    }
    return params;
}

void MouseTouchFactory::BeginConfiguration() {
    polling = true;
    mouse_input->BeginConfiguration();
}

void MouseTouchFactory::EndConfiguration() {
    polling = false;
    mouse_input->EndConfiguration();
}

} // namespace InputCommon
