#pragma once

#include <SFML/System/Vector2.hpp>

namespace Gui
{

    enum class MouseButton
    {
        Left,
        Right,
        Middle,
        Other
    };

    /// The only input vocabulary GuiGameController understands. Backend
    /// events are translated into this at the SfmlInputAdapter boundary, so
    /// the controller can be driven by tests or by a different backend.
    struct InputEvent
    {
        enum class Kind
        {
            PointerPressed,
            PointerReleased,
            FocusLost
        };

        Kind kind = Kind::PointerPressed;

        MouseButton button = MouseButton::Other;
        sf::Vector2f position{0.0f, 0.0f};

        static InputEvent makePointerPressed(MouseButton button, sf::Vector2f position)
        {
            InputEvent e;
            e.kind = Kind::PointerPressed;
            e.button = button;
            e.position = position;
            return e;
        }

        static InputEvent makePointerReleased(MouseButton button, sf::Vector2f position)
        {
            InputEvent e;
            e.kind = Kind::PointerReleased;
            e.button = button;
            e.position = position;
            return e;
        }

        static InputEvent makeFocusLost()
        {
            InputEvent e;
            e.kind = Kind::FocusLost;
            return e;
        }
    };

} // namespace Gui