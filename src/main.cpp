// FPS Unlocker v1.3.0
//
//  1) Quita el limite de FPS (hook de CCDisplayLinkDirector::setAnimationInterval).
//  2) Interpolacion SOLO VISUAL del jugador y la camara entre pasos de fisica,
//     para que por encima de 240 FPS el cubo se vea fluido.
//
// Como se evita romper la fisica (el problema de la v1.1.0):
//   - No se toca ningun campo interno del juego (nada de m_extraDelta para calcular).
//   - Cada frame, ANTES de PlayLayer::update se devuelven los nodos a su posicion
//     real, asi la fisica siempre corre con posiciones reales.
//   - DESPUES de PlayLayer::update se desplazan los nodos (solo su transform de
//     render, con CCNode::setPosition) a una posicion interpolada.
//   - Si otro codigo movio el nodo mientras estaba desplazado, no se restaura
//     (se asume que esa posicion es la correcta) y se descarta el historial.

#include <Geode/Geode.hpp>
#include <Geode/modify/CCDisplayLinkDirector.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr double kStep = 1.0 / 240.0;     // paso de fisica nativo
    constexpr float  kTeleportDist = 60.f;    // saltos mayores no se interpolan

    bool g_unlock    = true;
    int  g_targetFps = 480;
    bool g_interp    = true;
    bool g_debug     = false;

    double g_gameInterval = 1.0 / 60.0;       // intervalo que pide el juego
    bool   g_reapplying   = false;

    void reapplyInterval() {
        g_reapplying = true;
        CCDirector::get()->setAnimationInterval(g_gameInterval);
        g_reapplying = false;
    }

    struct Track {
        CCNode* node = nullptr;
        CCPoint before, after;   // posicion real antes/despues del update de este frame
        CCPoint prev, cur;       // posiciones reales de los dos ultimos pasos de fisica
        CCPoint shown;           // ultima posicion interpolada que pusimos
        bool shownValid = false;
        bool primed = false;
    };

    struct Interp {
        PlayLayer* owner = nullptr;
        Track t[3];              // 0 = player1, 1 = player2, 2 = m_objectLayer (camara)
        double L = 0.0;          // estimacion del sobrante del acumulador de pasos
        unsigned frame = 0;

        static CCNode* pick(PlayLayer* pl, int i) {
            switch (i) {
                case 0:  return pl->m_player1;
                case 1:  return pl->m_player2;
                default: return pl->m_objectLayer;
            }
        }

        // Devuelve los nodos a su posicion real.
        void restore() {
            for (auto& k : t) {
                if (k.node && k.shownValid) {
                    CCPoint p = k.node->CCNode::getPosition();
                    if (p.x == k.shown.x && p.y == k.shown.y) {
                        k.node->CCNode::setPosition(k.cur);
                    } else {
                        k.primed = false; // alguien lo movio, historial invalido
                    }
                }
                k.shownValid = false;
            }
        }

        void invalidate() {
            for (auto& k : t) k.primed = false;
            L = 0.0;
        }

        void begin(PlayLayer* pl) {
            if (pl != owner) {
                owner = pl;
                for (auto& k : t) k = Track{};
                L = 0.0;
            }
            restore();
            for (int i = 0; i < 3; ++i) {
                CCNode* n = pick(pl, i);
                if (n != t[i].node) { t[i] = Track{}; t[i].node = n; }
                if (n) t[i].before = n->CCNode::getPosition();
            }
        }

        void end(PlayLayer* pl, float dt) {
            bool moved = false;
            for (auto& k : t) {
                if (!k.node) continue;
                k.after = k.node->CCNode::getPosition();
                if (k.after.x != k.before.x || k.after.y != k.before.y) moved = true;
            }

            double warp = std::clamp<double>(pl->m_gameState.m_timeWarp, 0.01, 20.0);
            double dtEff = static_cast<double>(dt) * warp;

            // Estimacion del tiempo transcurrido desde el ultimo paso de fisica.
            L += dtEff;
            bool canInterp = true;
            if (dtEff >= kStep) {                // por debajo de ~240 FPS no hace falta
                L = std::fmod(L, kStep);
                canInterp = false;
            } else if (moved) {                  // hubo un paso de fisica este frame
                L -= kStep;
                L = std::clamp(L, 0.0, dtEff);
            } else if (L >= kStep) {             // el estimador se adelanto
                L = kStep * 0.999;
            }

            if (moved) {
                for (auto& k : t) {
                    if (!k.node) continue;
                    k.prev = k.before;
                    k.cur = k.after;
                    k.primed = true;
                }
            }

            double alpha = std::clamp(L / kStep, 0.0, 1.0);

            for (auto& k : t) {
                if (!k.node) continue;
                k.cur = k.after;
                if (!canInterp || !k.primed) continue;
                float dx = k.cur.x - k.prev.x, dy = k.cur.y - k.prev.y;
                if (std::sqrt(dx * dx + dy * dy) > kTeleportDist) continue;
                k.shown = CCPoint(
                    k.prev.x + dx * static_cast<float>(alpha),
                    k.prev.y + dy * static_cast<float>(alpha)
                );
                k.node->CCNode::setPosition(k.shown);
                k.shownValid = true;
            }

            if (g_debug && (++frame % 60) == 0) {
                log::info("dt={:.5f} dtEff={:.5f} moved={} L={:.5f} m_extraDelta={:.5f}",
                    dt, dtEff, moved, L, static_cast<double>(pl->m_extraDelta));
            }
        }
    };

    Interp g_interpState;
}

class $modify(FPSDirector, CCDisplayLinkDirector) {
    void setAnimationInterval(double interval) {
        if (!g_reapplying) g_gameInterval = interval;
        if (g_unlock && g_targetFps > 0) interval = 1.0 / g_targetFps;
        CCDisplayLinkDirector::setAnimationInterval(interval);
    }
};

class $modify(FPSPlayLayer, PlayLayer) {
    void update(float dt) {
        if (!g_unlock || !g_interp) {
            g_interpState.restore();
            PlayLayer::update(dt);
            return;
        }
        g_interpState.begin(this);
        PlayLayer::update(dt);
        g_interpState.end(this, dt);
    }

    // Estos puntos leen/guardan la posicion del jugador: que vean la real.
    void resetLevel() {
        g_interpState.restore();
        PlayLayer::resetLevel();
        g_interpState.invalidate();
    }

    CheckpointObject* createCheckpoint() {
        g_interpState.restore();
        return PlayLayer::createCheckpoint();
    }

    void pauseGame(bool b) {
        g_interpState.restore();
        PlayLayer::pauseGame(b);
    }
};

$on_mod(Loaded) {
    auto mod = Mod::get();
    g_unlock    = mod->getSettingValue<bool>("unlock-fps");
    g_targetFps = static_cast<int>(mod->getSettingValue<int64_t>("target-fps"));
    g_interp    = mod->getSettingValue<bool>("interpolate");
    g_debug     = mod->getSettingValue<bool>("debug-log");

    double cur = CCDirector::get()->getAnimationInterval();
    if (cur > 0.0) g_gameInterval = cur;

    listenForSettingChanges<bool>("unlock-fps", [](bool v) { g_unlock = v; reapplyInterval(); });
    listenForSettingChanges<int64_t>("target-fps", [](int64_t v) { g_targetFps = static_cast<int>(v); reapplyInterval(); });
    listenForSettingChanges<bool>("interpolate", [](bool v) { g_interp = v; });
    listenForSettingChanges<bool>("debug-log", [](bool v) { g_debug = v; });

    reapplyInterval();
}
