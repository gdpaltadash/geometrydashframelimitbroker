# FPS Unlocker (Geode, GD 2.2081, Windows)

Quita el limite de 240 FPS y suaviza el render por encima de 240 FPS sin tocar la fisica (240 TPS).
Pensado para usarse junto con Click Between Frames.

## Como funciona
1. **FPS**: hookea `CCDisplayLinkDirector::setAnimationInterval` y usa `1 / FPS objetivo`.
2. **Suavizado**: la fisica de GD avanza en pasos fijos de 1/240 s. Con mas de 240 FPS hay frames sin paso de
   fisica. En esos frames solo se mueve la posicion *visual* del jugador y de la camara entre los dos ultimos
   pasos reales (interpolar) o prediciendo hacia adelante (extrapolar). Justo antes del siguiente
   `GJBaseGameLayer::update` se restauran las posiciones reales, asi la fisica, las colisiones y otros mods
   (como CBF) siempre ven los valores reales.

## Opciones
En los ajustes del mod (y el interruptor tambien esta en el menu de pausa, abajo a la izquierda):
- Activar FPS Unlocker (al desactivarlo se restaura el limite original del juego)
- FPS objetivo (60 - 10000)
- Suavizar frames sobre 240 FPS
- Modo: Interpolar (exacto, hasta 4.2 ms de retraso visual) / Extrapolar (sin retraso, puede errar en saltos y clicks)

## Compilar
Usa **Geode SDK v5.x** (el que soporta 2.2081; v5.11.2 probado contra los bindings).

```
geode sdk install        # una vez
geode sdk update
geode sdk install-binaries   # una vez
set GEODE_SDK=C:\ruta\al\sdk
geode build
```
El resultado queda en `build/tuusuario.fps-unlocker.geode`. Copialo a `Geometry Dash/geode/mods/`.
Antes de compilar cambia `developer`/`id` en `mod.json` si quieres.

## Limitaciones conocidas
- Solo se suavizan jugador 1/2 y la capa de objetos (camara). Fondo/suelo parallax, triggers de movimiento y
  objetos animados siguen a 240 Hz.
- No se suaviza durante muerte, saltos grandes de posicion (portales de teletransporte) ni con menos de 240 FPS.
- Si `m_extraDelta` no tiene el significado esperado, el suavizado se omite y se escribe un aviso en el log de Geode.
- Con V-Sync activo el limite lo pone el monitor: desactivalo para ver FPS altos.
