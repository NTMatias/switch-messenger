#include "Notification.hpp"

namespace ui {

NotificationManager::~NotificationManager() {
    releaseCache();
}

void NotificationManager::releaseCache() {
    if (m_cachedTexture) {
        SDL_DestroyTexture(m_cachedTexture);
        m_cachedTexture = nullptr;
    }
    m_cachedKey.clear();
}

void NotificationManager::push(const std::string& sender, const std::string& text) {
    Entry entry;
    entry.sender = sender;
    entry.text = text;
    entry.expiresAtMs = 0; // se fija en update() cuando pasa a ser la activa
    m_queue.push_back(entry);
}

void NotificationManager::update(uint64_t nowMs) {
    if (m_queue.empty()) return;

    Entry& active = m_queue.front();
    if (active.expiresAtMs == 0) {
        active.expiresAtMs = nowMs + kDurationMs;
    }

    if (nowMs >= active.expiresAtMs) {
        m_queue.pop_front();
        releaseCache();
    }
}

void NotificationManager::render(SDL_Renderer* renderer, TTF_Font* font) {
    if (m_queue.empty() || !renderer || !font) return;

    const Entry& active = m_queue.front();

    const int boxW = 380;
    const int boxH = 90;
    const int x = 640 - boxW / 2;
    const int y = 20;

    SDL_Rect box{x, y, boxW, boxH};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 235);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 37, 211, 102, 255);
    SDL_RenderDrawRect(renderer, &box);

    std::string key = active.sender + "|" + active.text;
    if (key != m_cachedKey) {
        releaseCache();

        std::string title = "\xF0\x9F\x94\x94 " + active.sender; // etiqueta con campana
        SDL_Color white{255, 255, 255, 255};

        SDL_Surface* titleSurf = TTF_RenderUTF8_Blended(font, title.c_str(), white);
        SDL_Surface* bodySurf = TTF_RenderUTF8_Blended_Wrapped(font, active.text.c_str(), white, boxW - 20);

        if (titleSurf && bodySurf) {
            // Combina título y cuerpo en una sola textura para simplificar
            // el cacheo (una textura por notificación activa).
            SDL_Surface* combined = SDL_CreateRGBSurfaceWithFormat(
                0, boxW - 20, titleSurf->h + bodySurf->h + 8, 32, SDL_PIXELFORMAT_RGBA32);
            if (combined) {
                SDL_Rect dst1{0, 0, titleSurf->w, titleSurf->h};
                SDL_Rect dst2{0, titleSurf->h + 8, bodySurf->w, bodySurf->h};
                SDL_BlitSurface(titleSurf, nullptr, combined, &dst1);
                SDL_BlitSurface(bodySurf, nullptr, combined, &dst2);
                m_cachedTexture = SDL_CreateTextureFromSurface(renderer, combined);
                SDL_FreeSurface(combined);
            }
        }

        if (titleSurf) SDL_FreeSurface(titleSurf);
        if (bodySurf) SDL_FreeSurface(bodySurf);

        m_cachedKey = key;
    }

    if (m_cachedTexture) {
        int texW = 0, texH = 0;
        SDL_QueryTexture(m_cachedTexture, nullptr, nullptr, &texW, &texH);
        SDL_Rect dst{x + 10, y + 10, texW, texH};
        SDL_RenderCopy(renderer, m_cachedTexture, nullptr, &dst);
    }
}

} // namespace ui
