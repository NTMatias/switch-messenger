#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <deque>
#include <cstdint>

namespace ui {

// Notificación tipo overlay que se muestra sobre cualquier pantalla actual
// de la aplicación durante unos segundos y luego desaparece sola. No
// cambia de pantalla ni interrumpe la navegación: solo se dibuja encima.
class NotificationManager {
public:
    NotificationManager() = default;
    ~NotificationManager();

    // Encola una notificación para un mensaje nuevo recibido por WebSocket.
    void push(const std::string& sender, const std::string& text);

    // Debe llamarse una vez por frame para expirar notificaciones antiguas.
    void update(uint64_t nowMs);

    // Dibuja la notificación activa (si hay alguna) sobre lo que ya se
    // haya renderizado en este frame.
    void render(SDL_Renderer* renderer, TTF_Font* font);

private:
    struct Entry {
        std::string sender;
        std::string text;
        uint64_t    expiresAtMs;
    };

    static constexpr uint64_t kDurationMs = 4000;

    std::deque<Entry> m_queue;

    // Textura cacheada para no recrearla cada frame mientras la
    // notificación activa no cambie de contenido.
    SDL_Texture* m_cachedTexture = nullptr;
    std::string  m_cachedKey;

    void releaseCache();
};

} // namespace ui
