// Adds the Better Startpos button (the mod's logo) to the pause menu. It opens
// the mod's hub (Settings and Links).
#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "ui/Menus.hpp"

using namespace geode::prelude;

class $modify(BSPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        // The mod's round logo (resources/pause-button.png, packed in via
        // mod.json), the same height as GD's other round pause-menu buttons:
        // copied from a button already in that column when there is one,
        // else 26 units (what those buttons measure in GD 2.209).
        auto column = this->getChildByID("right-button-menu");
        float height = 26.f;
        if (column) {
            for (auto child : CCArrayExt<CCNode*>(column->getChildren())) {
                height = child->getScaledContentSize().height;
                break;
            }
        }
        auto sprite = CCSprite::create("pause-button.png"_spr);
        sprite->setScale(height / sprite->getContentHeight());
        auto button = CCMenuItemExt::createSpriteExtra(sprite, [](auto) {
            HubPopup::create()->show();
        });
        button->setID("links-button"_spr);

        // The Node IDs mod (installed with Geode) names this menu. If it's
        // missing for some reason, fall back to our own menu on the right side.
        if (column) {
            column->addChild(button);
            column->updateLayout();
        }
        else {
            auto winSize = CCDirector::get()->getWinSize();
            auto fallbackMenu = CCMenu::create();
            fallbackMenu->setID("fallback-menu"_spr);
            fallbackMenu->setPosition({ winSize.width - 30.f, winSize.height / 2 });
            fallbackMenu->addChild(button);
            this->addChild(fallbackMenu);
        }
    }
};
