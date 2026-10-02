#pragma once

#include "InputEvent.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
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

    /// Maps an sf::Keyboard::Key to the controller's Key enum. Returns
    /// nullopt for keys the controller does not act on; the caller drops
    /// those events silently.
    inline std::optional<Key> toKey(sf::Keyboard::Key k)
    {
        switch (k)
        {
            case sf::Keyboard::Key::Z:
                return Key::Z;
            case sf::Keyboard::Key::X:
                return Key::X;
            case sf::Keyboard::Key::Y:
                return Key::Y;
            case sf::Keyboard::Key::Left:
                return Key::Left;
            case sf::Keyboard::Key::Right:
                return Key::Right;
            case sf::Keyboard::Key::Home:
                return Key::Home;
            case sf::Keyboard::Key::End:
                return Key::End;
            default:
                return std::nullopt;
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

        if (const auto* keyEvent = event.getIf<sf::Event::KeyPressed>())
        {
            const auto mapped = toKey(keyEvent->code);
            if (!mapped)
                return std::nullopt;

            return InputEvent::makeKeyPressed(*mapped, keyEvent->control, keyEvent->shift);
        }

        return std::nullopt;
    }

} // namespace Gui