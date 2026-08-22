#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

namespace ui {

// Teclado virtual controlado enteramente con los Joy-Con:
//   D-Pad  = mover selección
//   A      = seleccionar carácter
//   X      = borrar
//   Y      = espacio
//   PLUS   = enviar mensaje (confirma el texto actual)
//   B      = cerrar teclado sin enviar
class VirtualKeyboard {
public:
    static constexpr size_t kMaxLength = 200;

    VirtualKeyboard();

    // prompt es un texto opcional que se muestra encima de la caja de
    // texto (por ejemplo "Escribe la IP del servidor"). Se usa en la
    // pantalla de configuración inicial; para el chat normal se deja
    // vacío.
    void open(const std::string& prompt = "");
    void close();
    bool isOpen() const { return m_open; }

    // Procesa la entrada de un frame. Devuelve true si el usuario confirmó
    // el envío del mensaje (PLUS) en este frame; en ese caso consumedText()
    // ya contiene el texto final (y el buffer interno queda vacío).
    bool handleInput(uint64_t keysDown);

    const std::string& currentText() const { return m_text; }

    void render(SDL_Renderer* renderer, TTF_Font* font);

private:
    bool m_open = false;
    std::string m_text;
    std::string m_prompt;

    int m_row = 0;
    int m_col = 0;

    std::vector<std::vector<char>> m_rows;

    void moveSelection(int dRow, int dCol);
    char selectedChar() const;
};

} // namespace ui
