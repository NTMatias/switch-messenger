# switch-messenger — servidor

Servidor WebSocket mínimo (Python + `websockets`) que reenvía mensajes de
chat entre todos los clientes conectados (Nintendo Switch y navegador) y
notifica presencia (online/offline). No guarda mensajes en disco ni en
base de datos: todo vive en memoria mientras el proceso corre.

## Ejecutar localmente

```bash
cd server
pip install -r requirements.txt
python app.py
```

Por defecto escucha en `0.0.0.0:8765`. Puedes cambiarlo con variables de
entorno:

```bash
HOST=0.0.0.0 PORT=9000 python app.py
```

## Desplegar en Render

Ver la sección "Despliegue en Render" del `README.md` de la raíz del
proyecto para la guía completa paso a paso.
