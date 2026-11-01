// FPS Unlocker - Geode, Geometry Dash 2.2081 (Windows)
//
// 1) Quita el limite de FPS: hookea CCDisplayLinkDirector::setAnimationInterval
//    y sustituye el intervalo por 1 / FPS objetivo.
// 2) Suaviza el render por encima de 240 FPS SIN tocar la fisica:
//    la fisica de GD corre en pasos fijos de 1/240 s. Con mas de 240 FPS hay
//    frames sin ningun paso de fisica. En esos frames solo movemos la posicion
//    VISUAL del jugador y de la camara (interpolando entre los dos ultimos
//    pasos reales) y la devolvemos a su valor real justo antes del siguiente
//    GJBaseGameLayer::update, asi la logica (y mods como Click Between Frames)
//    siempre ve las posiciones reales.

#include <Geode/Geode.hpp>
#include <Geode/modify/CCDisplayLinkDirector.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

// ---------------------------------------------------------------- ajustes

static bool unlockEnabled() {
	return Mod::get()->getSettingValue<bool>("unlock-fps");
}

static double targetInterval() {
	int64_t fps = Mod::get()->getSettingValue<int64_t>("target-fps");
	if (fps < 1) fps = 1;
	return 1.0 / static_cast<double>(fps);
}

// ultimo intervalo que pidio el propio juego (su limite original)
static double s_gameInterval = 1.0 / 60.0;

static void reapplyInterval() {
	if (auto dir = CCDirector::sharedDirector()) {
		// el hook decide si se sustituye o se deja pasar el valor del juego
		dir->setAnimationInterval(s_gameInterval);
	}
}

// ------------------------------------------------- 1) desbloqueo de FPS

class $modify(FPSDirector, CCDisplayLinkDirector) {
	void setAnimationInterval(double interval) {
		s_gameInterval = interval;
		if (unlockEnabled()) {
			interval = targetInterval();
		}
		CCDisplayLinkDirector::setAnimationInterval(interval);
	}
};

// ------------------------------------------- 2) suavizado visual > 240 FPS

namespace interp {
	constexpr float STEP = 1.f / 240.f;      // paso fijo de fisica de GD
	constexpr float TELEPORT_DIST = 80.f;    // saltos mayores (portales, etc.) no se suavizan

	struct Tracked {
		CCPoint prev;   // posicion real tras el penultimo paso
		CCPoint cur;    // posicion real mas reciente
		CCPoint truth;  // posicion real guardada para restaurar
	};

	struct State {
		GJBaseGameLayer* owner = nullptr;
		CCNode* layer = nullptr;
		PlayerObject* p1 = nullptr;
		PlayerObject* p2 = nullptr;
		Tracked cam, a, b;
		int lastStep = 0;
		bool valid = false;
		bool applied = false;
		bool warned = false;
	};
	static State s;

	static bool smoothingOn() {
		return unlockEnabled() && Mod::get()->getSettingValue<bool>("smoothing");
	}

	static bool extrapolate() {
		return Mod::get()->getSettingValue<std::string>("smoothing-mode") == "Extrapolar";
	}

	static void setPos(CCNode* n, CCPoint const& p) {
		// CCNode::setPosition directo: no pasa por los overrides de GD
		n->CCNode::setPosition(p);
	}

	static void setPlayerPos(PlayerObject* p, CCPoint const& pt) {
		// CCSprite::setPosition marca el sprite como "dirty" pero no ejecuta
		// el override de PlayerObject/GameObject
		p->CCSprite::setPosition(pt);
	}

	// Devuelve a su valor real lo que movimos visualmente en el frame anterior.
	static void restore(GJBaseGameLayer* g) {
		if (!s.applied) return;
		s.applied = false;
		if (s.owner != g || s.layer != g->m_objectLayer || s.p1 != g->m_player1 || s.p2 != g->m_player2) {
			// los nodos cambiaron (otro nivel): no tocar punteros posiblemente viejos
			s.valid = false;
			return;
		}
		setPos(s.layer, s.cam.truth);
		setPlayerPos(s.p1, s.a.truth);
		setPlayerPos(s.p2, s.b.truth);
	}

	static void reset() {
		s.valid = false;
		s.applied = false;
		s.owner = nullptr;
		s.layer = nullptr;
		s.p1 = nullptr;
		s.p2 = nullptr;
	}

	static void feed(Tracked& t, CCPoint const& now, bool newStep, bool contiguous) {
		t.truth = now;
		if (newStep) {
			if (contiguous && ccpDistance(t.cur, now) <= TELEPORT_DIST) {
				t.prev = t.cur;
			} else {
				t.prev = now; // sin suavizado en este paso
			}
		}
		t.cur = now;
	}

	static CCPoint display(Tracked const& t, float alpha, bool extra) {
		if (extra) {
			return ccp(t.cur.x + (t.cur.x - t.prev.x) * alpha, t.cur.y + (t.cur.y - t.prev.y) * alpha);
		}
		return ccp(t.prev.x + (t.cur.x - t.prev.x) * alpha, t.prev.y + (t.cur.y - t.prev.y) * alpha);
	}

	// Se llama justo despues del update original.
	static void afterUpdate(GJBaseGameLayer* g) {
		auto layer = g->m_objectLayer;
		auto p1 = g->m_player1;
		auto p2 = g->m_player2;
		if (!layer || !p1 || !p2) return;

		bool changedNodes = s.owner != g || s.layer != layer || s.p1 != p1 || s.p2 != p2;
		if (changedNodes) {
			s.owner = g;
			s.layer = layer;
			s.p1 = p1;
			s.p2 = p2;
			s.valid = false;
		}

		int step = g->m_currentStep;
		bool newStep = !s.valid || step != s.lastStep;
		// con <240 FPS hay varios pasos por frame: ahi no hay nada que suavizar
		bool contiguous = s.valid && (step - s.lastStep) == 1;
		if (!newStep) contiguous = true;

		CCPoint camNow = layer->getPosition();
		CCPoint aNow = p1->getPosition();
		CCPoint bNow = p2->getPosition();

		if (!s.valid) {
			s.cam = {camNow, camNow, camNow};
			s.a = {aNow, aNow, aNow};
			s.b = {bNow, bNow, bNow};
			s.valid = true;
			s.lastStep = step;
			return;
		}

		feed(s.cam, camNow, newStep, contiguous);
		feed(s.a, aNow, newStep, contiguous);
		feed(s.b, bNow, newStep, contiguous);
		s.lastStep = step;

		if (!smoothingOn()) return;
		if (p1->m_isDead || p2->m_isDead) return;

		// fraccion del siguiente paso que ya transcurrio
		float stepDelta = STEP * std::min(g->m_gameState.m_timeWarp, 1.f);
		if (stepDelta <= 0.f) return;
		double extra = g->m_extraDelta;
		if (extra < 0.0 || extra > stepDelta * 1.5) {
			// m_extraDelta no tiene el significado esperado: no suavizar
			if (!s.warned) {
				s.warned = true;
				log::warn("FPS Unlocker: m_extraDelta fuera de rango ({}), suavizado omitido", extra);
			}
			return;
		}
		float alpha = static_cast<float>(std::clamp(extra / stepDelta, 0.0, 1.0));

		bool ext = extrapolate();
		setPos(layer, display(s.cam, alpha, ext));
		setPlayerPos(p1, display(s.a, alpha, ext));
		setPlayerPos(p2, display(s.b, alpha, ext));
		s.applied = true;
	}
}

class $modify(FPSBaseGameLayer, GJBaseGameLayer) {
	void update(float dt) {
		auto pl = PlayLayer::get();
		// solo dentro de un nivel jugado, no en el editor
		if (!pl || static_cast<GJBaseGameLayer*>(pl) != this) {
			GJBaseGameLayer::update(dt);
			return;
		}
		interp::restore(this);
		GJBaseGameLayer::update(dt);
		interp::afterUpdate(this);
	}
};

class $modify(FPSPlayLayer, PlayLayer) {
	void resetLevel() {
		// resetLevel coloca al jugador: tiene que ver las posiciones reales
		interp::restore(this);
		PlayLayer::resetLevel();
		interp::reset();
	}

	void onQuit() {
		interp::restore(this);
		interp::reset();
		PlayLayer::onQuit();
	}

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		interp::reset();
		return PlayLayer::init(level, useReplay, dontCreateObjects);
	}
};

// ------------------------------------------------ boton en menu de pausa

class $modify(FPSPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto menu = CCMenu::create();
		menu->setID("fps-unlocker-menu"_spr);
		menu->setPosition({0.f, 0.f});

		auto toggler = CCMenuItemToggler::createWithStandardSprites(
			this, menu_selector(FPSPauseLayer::onToggleFPSUnlock), 0.6f
		);
		toggler->toggle(unlockEnabled());
		toggler->setPosition({26.f, 26.f});
		menu->addChild(toggler);

		auto label = CCLabelBMFont::create("FPS Unlocker", "bigFont.fnt");
		label->setID("fps-unlocker-label"_spr);
		label->setScale(0.4f);
		label->setAnchorPoint({0.f, 0.5f});
		label->setPosition({46.f, 26.f});

		this->addChild(menu, 10);
		this->addChild(label, 10);
	}

	void onToggleFPSUnlock(CCObject* sender) {
		// el callback se ejecuta antes de que el toggler cambie de estado
		auto toggler = static_cast<CCMenuItemToggler*>(sender);
		bool newValue = !toggler->isToggled();
		Mod::get()->setSettingValue<bool>("unlock-fps", newValue);
	}
};

// ------------------------------------------------------------------ carga

$on_mod(Loaded) {
	if (auto dir = CCDirector::sharedDirector()) {
		s_gameInterval = dir->getAnimationInterval();
	}
	reapplyInterval();

	listenForSettingChanges<bool>("unlock-fps", [](bool) {
		if (!unlockEnabled()) {
			// devolver a su sitio lo que el suavizado haya movido visualmente
			if (auto pl = PlayLayer::get()) interp::restore(pl);
			interp::reset();
		}
		reapplyInterval();
	});
	listenForSettingChanges<int64_t>("target-fps", [](int64_t) { reapplyInterval(); });
}
