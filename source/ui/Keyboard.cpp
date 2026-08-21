#include "Keyboard.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace ui {

VirtualKeyboard::VirtualKeyboard() {
    m_rows = {
        {'Q','W','E','R','T','Y','U','I','O','P'},
        {'A','S','D','F','G','H','J','K','L'},
        {'Z','X','C','V','B','N','M'},
    };
}

void VirtualKeyboard::open() {
    m_open = true;
    m_text.clear();
    m_row = 0;
    m_col = 0;
}

void VirtualKeyboard::close() {
    m_open = false;
}

void VirtualKeyboard::moveSelection(int dRow, int dCol) {
    int newRow = m_row + dRow;
    if (newRow < 0) newRow = static_cast<int>(m_rows.size()) - 1;
    if (newRow >= static_cast<int>(m_rows.size())) newRow = 0;
    m_row = newRow;

    int rowLen = static_cast<int>(m_rows[m_row].size());
    int newCol = m_col + dCol;
    if (newCol < 0) newCol = rowLen - 1;
    if (newCol >= rowLen) newCol = 0;
    m_col = newCol;
}

char VirtualKeyboard::selectedChar() const {
    return m_rows[m_row][m_col];
}

bool VirtualKeyboard::handleInput(uint64_t keysDown) {
    if (!m_open) return false;

#ifdef __SWITCH__
    if (keysDown & HidNpadButton_Left)  moveSelection(0, -1);
    if (keysDown & HidNpadButton_Right) moveSelection(0, 1);
    if (keysDown & HidNpadButton_Up)    moveSelection(-1, 0);
    if (keysDown & HidNpadButton_Down)  moveSelection(1, 0);

    if (keysDown & HidNpadButton_A) {
        if (m_text.size() < kMaxLength) {
            m_text += selectedChar();
        }
    }

    if (keysDown & HidNpadButton_X) {
        if (!m_text.empty()) {
            m_text.pop_back();
        }
    }

    if (keysDown & HidNpadButton_Y) {
        if (m_text.size() < kMaxLength) {
            m_text += ' ';
        }
    }

    if (keysDown & HidNpadButton_B) {
        close();
        return false;
    }

    if (keysDown & HidNpadButton_Plus) {
        if (!m_text.empty()) {
            return true; // el llamador debe leer currentText() y luego limpiar/close()
        }
    }
#endif

    return false;
}

void VirtualKeyboard::render(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_open || !renderer || !font) return;

    const int panelH = 260;
    SDL_Rect panel{0, 720 - panelH, 1280, panelH};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 15, 15, 15, 245);
    SDL_RenderFillRect(renderer, &panel);

    // Barra de texto actual.
    SDL_Rect textBar{20, panel.y + 10, 1240, 40};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderFillRect(renderer, &textBar);

    if (!m_text.empty()) {
        SDL_Color white{255, 255, 255, 255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, m_text.c_str(), white);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst{textBar.x + 8, textBar.y + 6, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    // Teclas.
    const int keyW = 60;
    const int keyH = 48;
    const int startY = panel.y + 70;

    for (size_t r = 0; r < m_rows.size(); ++r) {
        int rowWidth = static_cast<int>(m_rows[r].size()) * keyW;
        int startX = (1280 - rowWidth) / 2;

        for (size_t c = 0; c < m_rows[r].size(); ++c) {
            SDL_Rect key{
                startX + static_cast<int>(c) * keyW,
                startY + static_cast<int>(r) * (keyH + 6),
                keyW - 6, keyH
            };

            bool selected = (static_cast<int>(r) == m_row && static_cast<int>(c) == m_col);
            if (selected) {
                SDL_SetRenderDrawColor(renderer, 37, 211, 102, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 45, 45, 45, 255);
            }
            SDL_RenderFillRect(renderer, &key);

            char label[2] = { m_rows[r][c], '\0' };
            SDL_Color textColor = selected ? SDL_Color{0, 0, 0, 255} : SDL_Color{220, 220, 220, 255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, label, textColor);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    SDL_Rect dst{
                        key.x + (key.w - surf->w) / 2,
                        key.y + (key.h - surf->h) / 2,
                        surf->w, surf->h
                    };
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }
    }
}

} // namespace ui
