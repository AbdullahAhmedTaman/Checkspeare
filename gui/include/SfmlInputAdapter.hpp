#pragma once

#include "InputEvent.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>

#include <optional>

namespace Gui
{

    inline MouseButton toMouseButton(sf::Mouse::Button b)
    {
        switch (b)
        {
            case sf::Mouse::Button::Left:
                return MouseButton::Left;
            case sf::Mouse::Button::Right:
                return MouseButton::Right;
            case sf::Mouse::Button::Middle:
                return MouseButton::Middle;
            default:
                return MouseButton::Other;
        }
    }

    /// Translates an sf::Event into the InputEvent vocabulary. Returns
    /// nullopt for events the controller does not act on.
    ///
    /// This is the only place allowed to know that the backend is SFML.
    inline std::optional<InputEvent> toInputEvent(const sf::Event& event)
    {
        if (event.is<sf::Event::FocusLost>())
            return InputEvent::makeFocusLost();

        if (const auto* press = event.getIf<sf::Event::MouseButtonPressed>())
        {
            return InputEvent::makePointerPressed(toMouseButton(press->button),
                                                  {static_cast<float>(press->position.x), static_cast<float>(press->position.y)});
        }

        if (const auto* release = event.getIf<sf::Event::MouseButtonReleased>())
        {
            return InputEvent::makePointerReleased(toMouseButton(release->button), {static_cast<float>(release->position.x),
                                                                                    static_cast<float>(release->position.y)});
        }

        return std::nullopt;
    }

} // namespace Gui