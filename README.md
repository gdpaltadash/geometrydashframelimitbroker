# FPS Unlocker (Geode, GD 2.2081, Windows)

Quita el limite de 240 FPS del juego sin tocar los 240 TPS nativos de la fisica.

## Como funciona
Hookea `CCDisplayLinkDirector::setAnimationInterval` y sustituye el intervalo que pide
el juego por `1 / FPS objetivo`. No modifica ninguna posicion, dt ni paso de fisica:
GD ya acumula sus pasos de fisica en 1/240s dentro de `GJBaseGameLayer::update` sin
importar cuantos FPS tenga el render, asi que esto basta para lograr mas FPS visuales
sin alterar la fisica, y es compatible con mods como Click Between Frames.

## Version anterior (v1.1.0)
Esa version incluia una opcion de "suavizado" que interpolaba visualmente al jugador
y la camara entre pasos de fisica para los frames por encima de 240 FPS. Se quito
porque dependia de un campo de memoria (`m_extraDelta`) cuyo significado exacto no se
pudo verificar sin depurar el juego en vivo, y en la practica rompia el juego
(movimiento erratico). No se va a volver a agregar sin poder probarla primero.

## Opciones
En los ajustes del mod (Geode) o desde el boton con el engranaje en el menu de pausa:
- Activar FPS Unlocker (al desactivarlo se restaura el limite original del juego)
- FPS objetivo (60 - 10000)

## Compilar
Usa **Geode SDK v5.11.2** (version con la que esta probado mod.json; es la que soporta 2.2081).

```
geode sdk install        # una vez
geode sdk update
geode sdk install-binaries   # una vez
set GEODE_SDK=C:\ruta\al\sdk
geode build
```
El resultado queda en `build/tuusuario.fps-unlocker.geode`. Copialo a `Geometry Dash/geode/mods/`.
Antes de compilar cambia `developer`/`id` en `mod.json` si quieres.

Para compilar en GitHub Actions usa el workflow en `.github/workflows/build.yml`
(pestana Actions -> Run workflow -> descargar el artifact "Build Output").

## Notas
- Si el contador no sube, desactiva V-Sync en las opciones del juego y en el panel de tu GPU.
- Tu monitor no mostrara mas Hz de los que tiene; el contador sube segun lo que aguanten tu CPU/GPU.
