// FPS Unlocker - Geode, Geometry Dash 2.2081 (Windows)
//
// Quita el limite de FPS del juego hookeando CCDisplayLinkDirector::setAnimationInterval
// y sustituyendo el intervalo que pide el juego por 1 / FPS objetivo.
//
// No toca ningun dato de la partida (posiciones, dt, TimeWarp, pasos de fisica).
// La fisica de GD ya corre en un acumulador de pasos fijos de 1/240s dentro de
// GJBaseGameLayer::update, independiente de cuantos FPS tenga el render, asi que
// no hace falta (ni conviene) tocar nada mas para lograr mas FPS sin alterarla.

#include <Geode/Geode.hpp>
#include <Geode/modify/CCDisplayLinkDirector.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <algorithm>

using namespace geode::prelude;

// ---------------------------------------------------------------- ajustes

static bool unlockEnabled() {
	return Mod::get()->getSettingValue<bool>("unlock-fps");
}

static int64_t targetFps() {
	return Mod::get()->getSettingValue<int64_t>("target-fps");
}

static double targetInterval() {
	int64_t fps = targetFps();
	if (fps < 1) fps = 1;
	return 1.0 / static_cast<double>(fps);
}

// ultimo intervalo que pidio el propio juego (su limite original)
static double s_gameInterval = 1.0 / 60.0;

static void reapplyInterval() {
	if (auto dir = CCDirector::sharedDirector()) {
		dir->setAnimationInterval(s_gameInterval);
	}
}

// ------------------------------------------------- hook de desbloqueo de FPS

class $modify(FPSDirector, CCDisplayLinkDirector) {
	void setAnimationInterval(double interval) {
		s_gameInterval = interval;
		if (unlockEnabled()) {
			interval = targetInterval();
		}
		CCDisplayLinkDirector::setAnimationInterval(interval);
	}
};

// --------------------------------------------------- panel del menu de pausa

class FPSUnlockerPopup : public Popup {
protected:
	CCMenuItemToggler* m_unlockToggle = nullptr;
	CCLabelBMFont* m_fpsLabel = nullptr;

	bool init() {
		if (!Popup::init(220.f, 140.f)) return false;

		this->setTitle("FPS Unlocker");

		auto unlockLabel = CCLabelBMFont::create("Activar FPS Unlocker", "bigFont.fnt");
		unlockLabel->setScale(.4f);
		m_mainLayer->addChildAtPosition(unlockLabel, Anchor::Center, ccp(-10, 20), ccp(0.f, .5f));

		m_unlockToggle = CCMenuItemToggler::createWithStandardSprites(
			this, menu_selector(FPSUnlockerPopup::onToggleUnlock), .65f
		);
		m_buttonMenu->addChildAtPosition(m_unlockToggle, Anchor::Center, ccp(70, 20));
		m_unlockToggle->toggle(unlockEnabled());

		auto fpsCaption = CCLabelBMFont::create("FPS objetivo", "bigFont.fnt");
		fpsCaption->setScale(.4f);
		m_mainLayer->addChildAtPosition(fpsCaption, Anchor::Center, ccp(0, -15));

		auto minusSpr = ButtonSprite::create("-", "bigFont.fnt", "GJ_button_04.png", .8f);
		minusSpr->setScale(.75f);
		auto minusBtn = CCMenuItemSpriteExtra::create(
			minusSpr, this, menu_selector(FPSUnlockerPopup::onFpsStep)
		);
		minusBtn->setTag(-60);
		m_buttonMenu->addChildAtPosition(minusBtn, Anchor::Center, ccp(-55, -40));

		m_fpsLabel = CCLabelBMFont::create("", "bigFont.fnt");
		m_fpsLabel->setScale(.55f);
		m_mainLayer->addChildAtPosition(m_fpsLabel, Anchor::Center, ccp(0, -40));

		auto plusSpr = ButtonSprite::create("+", "bigFont.fnt", "GJ_button_04.png", .8f);
		plusSpr->setScale(.75f);
		auto plusBtn = CCMenuItemSpriteExtra::create(
			plusSpr, this, menu_selector(FPSUnlockerPopup::onFpsStep)
		);
		plusBtn->setTag(60);
		m_buttonMenu->addChildAtPosition(plusBtn, Anchor::Center, ccp(55, -40));

		this->updateFpsLabel();

		return true;
	}

	void updateFpsLabel() {
		m_fpsLabel->setString(std::to_string(targetFps()).c_str());
	}

	void onToggleUnlock(CCObject*) {
		bool newValue = !unlockEnabled();
		Mod::get()->setSettingValue<bool>("unlock-fps", newValue);
		m_unlockToggle->toggle(newValue);
	}

	void onFpsStep(CCObject* sender) {
		int delta = static_cast<CCMenuItemSpriteExtra*>(sender)->getTag();
		int64_t fps = std::clamp<int64_t>(targetFps() + delta, 60, 10000);
		Mod::get()->setSettingValue<int64_t>("target-fps", fps);
		this->updateFpsLabel();
	}

public:
	static FPSUnlockerPopup* create() {
		auto ret = new FPSUnlockerPopup();
		if (ret->init()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

class $modify(FPSPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
		spr->setScale(.8f);
		auto btn = CCMenuItemSpriteExtra::create(
			spr, this, menu_selector(FPSPauseLayer::onFPSUnlockerSettings)
		);
		btn->setID("fps-unlocker-btn"_spr);

		auto menu = CCMenu::create();
		menu->setID("fps-unlocker-menu"_spr);
		menu->addChild(btn);
		menu->setPosition({32.f, 32.f});
		this->addChild(menu, 10);
	}

	void onFPSUnlockerSettings(CCObject*) {
		FPSUnlockerPopup::create()->show();
	}
};

// ------------------------------------------------------------------ carga

$on_mod(Loaded) {
	if (auto dir = CCDirector::sharedDirector()) {
		s_gameInterval = dir->getAnimationInterval();
	}
	reapplyInterval();

	listenForSettingChanges<bool>("unlock-fps", [](bool) { reapplyInterval(); });
	listenForSettingChanges<int64_t>("target-fps", [](int64_t) { reapplyInterval(); });
}
