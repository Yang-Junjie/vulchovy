#pragma once

namespace vulchovy {

class InputState;

/// @brief Maps input to camera changes.
class CameraController {
public:
    virtual ~CameraController() = default;

    /// @brief Applies input to the camera.
    /// @return True if the camera changed this frame.
    virtual bool update(const InputState& input, float dt) = 0;
};

} // namespace vulchovy
