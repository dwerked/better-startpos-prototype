#include "SmartStartpos.hpp"

using namespace geode::prelude;

// Speed portals don't have an object type of their own in GD, so they're
// recognized by object ID.
static std::optional<Speed> speedForPortal(GameObject* object) {
    switch (object->m_objectID) {
        case 200: return Speed::Slow;     // 0.5x
        case 201: return Speed::Normal;   // 1x
        case 202: return Speed::Fast;     // 2x
        case 203: return Speed::Faster;   // 3x
        case 1334: return Speed::Fastest; // 4x
        default: return std::nullopt;
    }
}

// Gamemode numbers, as GD stores them in a startpos's settings.
static std::optional<int> gamemodeForPortal(GameObject* object) {
    switch (object->m_objectType) {
        case GameObjectType::CubePortal: return 0;
        case GameObjectType::ShipPortal: return 1;
        case GameObjectType::BallPortal: return 2;
        case GameObjectType::UfoPortal: return 3;
        case GameObjectType::WavePortal: return 4;
        case GameObjectType::RobotPortal: return 5;
        case GameObjectType::SpiderPortal: return 6;
        case GameObjectType::SwingPortal: return 7;
        default: return std::nullopt;
    }
}

// Orbs and pads that can flip gravity. Whether they do depends on the player
// (clicking an orb, landing on a pad), so they make gravity uncertain.
static bool isGravityOrbOrPad(GameObject* object) {
    switch (object->m_objectType) {
        case GameObjectType::GravityRing:     // blue orb
        case GameObjectType::GreenRing:       // green orb
        case GameObjectType::GravityDashRing: // dash orb that flips gravity
        case GameObjectType::SpiderOrb:
        case GameObjectType::GravityPad:      // blue pad
        case GameObjectType::SpiderPad:
            return true;
        default:
            return false;
    }
}

// Blue and yellow gravity portals set gravity no matter what came before,
// so they end any uncertainty from orbs and pads before them.
static bool isFixedGravityPortal(GameObject* object) {
    return object->m_objectType == GameObjectType::NormalGravityPortal
        || object->m_objectType == GameObjectType::InverseGravityPortal;
}

bool isSmartPortal(GameObject* object) {
    if (speedForPortal(object) || gamemodeForPortal(object) || isGravityOrbOrPad(object)) return true;
    switch (object->m_objectType) {
        case GameObjectType::RegularSizePortal:
        case GameObjectType::MiniSizePortal:
        case GameObjectType::NormalGravityPortal:
        case GameObjectType::InverseGravityPortal:
        case GameObjectType::GravityTogglePortal:
        case GameObjectType::DualPortal:
        case GameObjectType::SoloPortal:
        case GameObjectType::NormalMirrorPortal:
        case GameObjectType::InverseMirrorPortal:
            return true;
        default:
            return false;
    }
}

PlayerState PlayerState::from(LevelSettingsObject* settings) {
    PlayerState state;
    state.gamemode = settings->m_startMode;
    state.speed = settings->m_startSpeed;
    state.mini = settings->m_startMini;
    state.upsideDown = settings->m_isFlipped;
    state.dual = settings->m_startDual;
    state.mirrored = settings->m_mirrorMode;
    return state;
}

bool PlayerState::writeTo(LevelSettingsObject* settings) const {
    bool changed = !(*this == from(settings));
    settings->m_startMode = gamemode;
    settings->m_startSpeed = speed;
    settings->m_startMini = mini;
    settings->m_isFlipped = upsideDown;
    settings->m_startDual = dual;
    settings->m_mirrorMode = mirrored;
    return changed;
}

void PlayerState::pass(GameObject* portal) {
    if (auto newSpeed = speedForPortal(portal)) speed = *newSpeed;
    if (auto newMode = gamemodeForPortal(portal)) gamemode = *newMode;
    switch (portal->m_objectType) {
        case GameObjectType::RegularSizePortal: mini = false; break;
        case GameObjectType::MiniSizePortal: mini = true; break;
        case GameObjectType::NormalGravityPortal: upsideDown = false; break;
        case GameObjectType::InverseGravityPortal: upsideDown = true; break;
        case GameObjectType::GravityTogglePortal: upsideDown = !upsideDown; break; // green: flips
        case GameObjectType::DualPortal: dual = true; break;
        case GameObjectType::SoloPortal: dual = false; break;
        case GameObjectType::NormalMirrorPortal: mirrored = false; break;
        case GameObjectType::InverseMirrorPortal: mirrored = true; break;
        default: break; // orbs and pads: we can't know, so they change nothing here
    }
}

SmartResult applySmartStartpos(LevelSettingsObject* levelStart,
                               std::vector<GameObject*> portals,
                               std::vector<StartPosObject*> const& startposes) {
    SmartResult result;
    result.gravityUnsure.assign(startposes.size(), false);
    if (!levelStart) return result;

    std::stable_sort(portals.begin(), portals.end(), [](auto* a, auto* b) {
        return a->getPositionX() < b->getPositionX();
    });

    // Walk the portals and the (already sorted) startposes left to right together.
    PlayerState state = PlayerState::from(levelStart);
    bool gravityUnsure = false;
    size_t nextPortal = 0;
    for (size_t i = 0; i < startposes.size(); i++) {
        auto startpos = startposes[i];
        // Every portal left of this startpos has been passed by the time you reach it.
        while (nextPortal < portals.size() && portals[nextPortal]->getPositionX() < startpos->getPositionX()) {
            auto portal = portals[nextPortal++];
            state.pass(portal);
            if (isGravityOrbOrPad(portal)) gravityUnsure = true;
            if (isFixedGravityPortal(portal)) gravityUnsure = false;
        }
        result.gravityUnsure[i] = gravityUnsure;
        if (startpos->m_startSettings && state.writeTo(startpos->m_startSettings)) result.changed++;
    }
    return result;
}
