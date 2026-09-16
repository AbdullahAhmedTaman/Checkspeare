#pragma once

#include <SFML/System/Vector2.hpp>

namespace Gui
{

    /// Backend-agnostic mouse button identifier. Deliberately smaller than
    /// sf::Mouse::Button so callers don't have to reason about buttons the
    /// controller will never care about.
    enum class MouseButton
    {
        Left,
        Right,
        Middle,
        Other
    };

    /// The only input vocabulary GuiGameController understands.
    ///
    /// This is the seam between "some windowing/UI library fired an event"
    /// and "the chess input state machine reacts to a pointer action or a
    /// loss of focus". Nothing in this file mentions SFML events, so the
    /// controller can be driven by hand (unit tests) or by a different
    /// backend without any change to GuiGameController itself.
    struct InputEvent
    {
        enum class Kind
        {
            PointerPressed,
            PointerReleased,
            FocusLost
        };

        Kind kind;

        // Only meaningful for PointerPressed / PointerReleased.
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
