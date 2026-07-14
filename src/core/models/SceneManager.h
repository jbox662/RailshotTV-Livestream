#pragma once

#include "core/models/SourceTypes.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace railshot {

class SceneManager {
public:
    static constexpr int kDefaultFadeDurationMs = 300;

    static SceneManager& instance();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    void loadDefaults();
    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path) const;

    void tickAutoSave();
    bool saveNow();

    [[nodiscard]] SceneCollection& collection();
    [[nodiscard]] const SceneCollection& collection() const;

    [[nodiscard]] std::vector<CollectionInfo> listCollections() const;
    [[nodiscard]] std::string currentCollectionId() const;
    [[nodiscard]] std::string collectionFilePath(const std::string& id) const;

    bool createCollection(const std::string& name);
    bool duplicateCollection(const std::string& name);
    bool renameCollection(const std::string& id, const std::string& name);
    bool deleteCollection(const std::string& id);
    bool switchCollection(const std::string& id);

    [[nodiscard]] std::string activeSceneId() const;
    [[nodiscard]] std::string previewSceneId() const;
    [[nodiscard]] Scene* activeScene();
    [[nodiscard]] const Scene* activeScene() const;
    [[nodiscard]] Scene* previewScene();
    [[nodiscard]] const Scene* previewScene() const;
    [[nodiscard]] Scene* sceneById(const std::string& id);
    [[nodiscard]] const Scene* sceneById(const std::string& id) const;
    [[nodiscard]] std::optional<Scene> sceneSnapshot(const std::string& id) const;
    [[nodiscard]] std::optional<Scene> activeSceneSnapshot() const;
    [[nodiscard]] std::optional<Scene> previewSceneSnapshot() const;
    [[nodiscard]] std::optional<Scene> programSceneSnapshotForRender() const;
    [[nodiscard]] bool isStudioModeEnabled() const;

    void setStudioModeEnabled(bool enabled);

    [[nodiscard]] Source* sourceById(const std::string& sourceId);
    [[nodiscard]] const Source* sourceById(const std::string& sourceId) const;
    [[nodiscard]] Source* sourceInScene(const std::string& sceneId, const std::string& sourceId);

    void setPreviewScene(const std::string& sceneId);
    void swapPreviewAndProgram();

    Scene& addScene(const std::string& name);
    bool removeScene(const std::string& sceneId);
    Source& addSource(const std::string& sceneId, const Source& sourceTemplate);
    bool removeSource(const std::string& sceneId, const std::string& sourceId);

    void setActiveScene(const std::string& sceneId);
    void updateTransition();

    [[nodiscard]] bool isTransitioning() const;
    [[nodiscard]] float transitionBlend() const;
    [[nodiscard]] TransitionType activeTransitionType() const;
    [[nodiscard]] std::string outgoingSceneId() const;
    [[nodiscard]] int fadeDurationMs() const { return fadeDurationMs_; }
    void setFadeDurationMs(int ms);

    void setOnScenesChanged(std::function<void()> callback);

    static std::string generateId();

private:
    SceneManager();

    void notifyChanged();
    void beginTransition(const std::string& newSceneId, TransitionType type);
    void syncProgramBufferFromSceneId(const std::string& sceneId);

    void initializeCollections();
    bool loadIndex();
    bool saveIndex() const;
    void ensureDirectories() const;
    bool writeCollectionFileUnlocked(const std::string& path) const;
    bool readCollectionFileUnlocked(const std::string& path);
    void resetTransientState();
    std::string makeDefaultCollectionUnlocked(const std::string& name);

    mutable std::mutex mutex_;
    SceneCollection collection_;
    std::vector<CollectionInfo> index_;
    std::string activeSceneId_;
    std::string previewSceneId_;
    std::string outgoingSceneId_;
    std::optional<Scene> studioProgramScene_;
    bool studioModeEnabled_ = false;
    bool transitioning_ = false;
    float transitionBlend_ = 1.0f;
    TransitionType activeTransitionType_ = TransitionType::Fade;
    int fadeDurationMs_ = kDefaultFadeDurationMs;
    std::chrono::steady_clock::time_point transitionStart_;

    std::string dataDir_;
    std::string collectionsDir_;
    std::string indexPath_;
    std::string legacyScenesPath_;
    std::string savePath_;
    std::chrono::steady_clock::time_point lastAutoSave_;
    std::function<void()> onScenesChanged_;
};

} // namespace railshot
