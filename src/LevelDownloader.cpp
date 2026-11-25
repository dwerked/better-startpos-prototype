#include "LevelDownloader.hpp"

using namespace geode::prelude;

// Only one download at a time: GD has a single "level download" listener slot
// (GameLevelManager::m_levelDownloadDelegate), the same one the level page uses.
class LevelDataDownload : public CCObject, public LevelDownloadDelegate {
public:
    int m_levelID = 0;
    Ref<GJGameLevel> m_level; // GD's saved copy, which needs the data

    static constexpr float GIVE_UP_AFTER = 20.f; // seconds

    void levelDownloadFinished(GJGameLevel* downloaded) override {
        if (!downloaded || downloaded->m_levelID.value() != m_levelID) return;
        // GD may hand back a separate object; make sure the saved copy we play
        // from has the data too.
        if (std::string(m_level->m_levelString).empty()) {
            m_level->m_levelString = downloaded->m_levelString;
        }
        log::info("Downloaded data for linked level {}", m_levelID);
        this->finish();
    }

    void levelDownloadFailed(int response) override {
        log::warn("Couldn't download linked level {} (response {})", m_levelID, response);
        this->finish();
    }

    void timedOut(float) {
        log::warn("Download of linked level {} timed out", m_levelID);
        this->finish();
    }

    void finish();
};

static Ref<LevelDataDownload> s_download;

void LevelDataDownload::finish() {
    auto glm = GameLevelManager::get();
    // Only clear the listener slot if it's still ours.
    if (glm->m_levelDownloadDelegate == this) glm->m_levelDownloadDelegate = nullptr;
    CCDirector::get()->getScheduler()->unscheduleSelector(schedule_selector(LevelDataDownload::timedOut), this);
    s_download = nullptr;
}

bool hasLevelData(GJGameLevel* level) {
    return level && !std::string(level->m_levelString).empty();
}

bool isDownloadingLevel(GJGameLevel* level) {
    return level && s_download && s_download->m_levelID == level->m_levelID.value();
}

void prefetchLevelData(GJGameLevel* level) {
    if (!level || hasLevelData(level)) return;
    if (level->m_levelType == GJLevelType::Editor) return; // local levels always have data
    if (s_download) return; // one at a time

    auto glm = GameLevelManager::get();
    // Someone else (e.g. a level page) is waiting on a download; don't steal it.
    if (glm->m_levelDownloadDelegate) return;

    s_download = new LevelDataDownload();
    s_download->release(); // Ref took its own reference; drop the one from `new`
    s_download->m_levelID = level->m_levelID.value();
    s_download->m_level = level;

    glm->m_levelDownloadDelegate = s_download.data();
    CCDirector::get()->getScheduler()->scheduleSelector(
        schedule_selector(LevelDataDownload::timedOut), s_download.data(),
        LevelDataDownload::GIVE_UP_AFTER, 0, LevelDataDownload::GIVE_UP_AFTER, false
    );
    log::info("Downloading data for linked level {}", s_download->m_levelID);
    glm->downloadLevel(s_download->m_levelID, false, 0);
}
