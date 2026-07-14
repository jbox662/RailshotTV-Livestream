#include "core/RemoteCommandBus.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "core/models/SceneManager.h"
#include "score/ScoreManager.h"

#include <QJsonArray>
#include <QMetaObject>

namespace railshot {

namespace {

constexpr int kRemoteProtocolVersion = 2;

bool outputBusy(StreamController* controller, QString* error) {
    if (!controller) {
        return false;
    }
    if (controller->isStreaming()) {
        if (error) {
            *error = QStringLiteral("stop streaming first");
        }
        return true;
    }
    if (controller->isRecording()) {
        if (error) {
            *error = QStringLiteral("stop recording first");
        }
        return true;
    }
    if (controller->isVirtualCameraActive()) {
        if (error) {
            *error = QStringLiteral("stop virtual camera first");
        }
        return true;
    }
    return false;
}

} // namespace

RemoteCommandBus& RemoteCommandBus::instance() {
    static RemoteCommandBus bus;
    return bus;
}

void RemoteCommandBus::attach(StreamController* controller,
                              std::function<std::string()> rtmpUrlProvider,
                              std::function<void()> uiRefresh) {
    std::lock_guard lock(mutex_);
    controller_ = controller;
    rtmpUrlProvider_ = std::move(rtmpUrlProvider);
    uiRefresh_ = std::move(uiRefresh);
}

QJsonObject RemoteCommandBus::statusJson() const {
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    if (!controller_) {
        obj[QStringLiteral("streaming")] = false;
        obj[QStringLiteral("recording")] = false;
        obj[QStringLiteral("studioMode")] = false;
        return obj;
    }
    const StreamStats stats = controller_->stats();
    obj[QStringLiteral("streaming")] = stats.isStreaming;
    obj[QStringLiteral("recording")] = stats.isRecording;
    obj[QStringLiteral("connected")] = stats.isConnected;
    obj[QStringLiteral("studioMode")] = controller_->isStudioModeEnabled();
    obj[QStringLiteral("virtualCamera")] = controller_->isVirtualCameraActive();
    obj[QStringLiteral("activeSceneId")] =
        QString::fromStdString(SceneManager::instance().activeSceneId());
    obj[QStringLiteral("previewSceneId")] =
        QString::fromStdString(SceneManager::instance().previewSceneId());
    obj[QStringLiteral("collectionId")] =
        QString::fromStdString(SceneManager::instance().currentCollectionId());
    obj[QStringLiteral("collectionName")] =
        QString::fromStdString(SceneManager::instance().collection().name);
    return obj;
}

QJsonObject RemoteCommandBus::scenesJson() const {
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    const auto& sm = SceneManager::instance();
    obj[QStringLiteral("activeSceneId")] = QString::fromStdString(sm.activeSceneId());
    obj[QStringLiteral("previewSceneId")] = QString::fromStdString(sm.previewSceneId());
    QString programName;
    QString previewName;
    QJsonArray scenes;
    for (const auto& scene : sm.collection().scenes) {
        QJsonObject s;
        s[QStringLiteral("id")] = QString::fromStdString(scene.id);
        s[QStringLiteral("name")] = QString::fromStdString(scene.name);
        scenes.append(s);
        if (scene.id == sm.activeSceneId()) {
            programName = QString::fromStdString(scene.name);
        }
        if (scene.id == sm.previewSceneId()) {
            previewName = QString::fromStdString(scene.name);
        }
    }
    obj[QStringLiteral("scenes")] = scenes;
    obj[QStringLiteral("currentProgramSceneName")] = programName;
    obj[QStringLiteral("currentPreviewSceneName")] = previewName;
    obj[QStringLiteral("collectionId")] = QString::fromStdString(sm.currentCollectionId());
    obj[QStringLiteral("collectionName")] = QString::fromStdString(sm.collection().name);
    obj[QStringLiteral("studioMode")] = sm.isStudioModeEnabled();
    return obj;
}

QJsonObject RemoteCommandBus::collectionsJson() const {
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("activeId")] =
        QString::fromStdString(SceneManager::instance().currentCollectionId());
    QJsonArray items;
    for (const auto& info : SceneManager::instance().listCollections()) {
        QJsonObject c;
        c[QStringLiteral("id")] = QString::fromStdString(info.id);
        c[QStringLiteral("name")] = QString::fromStdString(info.name);
        items.append(c);
    }
    obj[QStringLiteral("collections")] = items;
    return obj;
}

QJsonObject RemoteCommandBus::scoreJson() const {
    const MatchState state = ScoreManager::instance().state();
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("player1Name")] = QString::fromStdString(state.player1Name);
    obj[QStringLiteral("player2Name")] = QString::fromStdString(state.player2Name);
    obj[QStringLiteral("player1Score")] = state.player1Score;
    obj[QStringLiteral("player2Score")] = state.player2Score;
    obj[QStringLiteral("raceTo")] = state.raceTo;
    obj[QStringLiteral("activePlayer")] = state.activePlayer;
    return obj;
}

QJsonObject RemoteCommandBus::execute(const QString& op, const QJsonObject& data) {
    QJsonObject result;
    result[QStringLiteral("ok")] = false;
    result[QStringLiteral("op")] = op;

    auto refresh = [this]() {
        std::function<void()> cb;
        {
            std::lock_guard lock(mutex_);
            cb = uiRefresh_;
        }
        if (cb) {
            cb();
        }
    };

    StreamController* controller = nullptr;
    std::function<std::string()> rtmpProvider;
    {
        std::lock_guard lock(mutex_);
        controller = controller_;
        rtmpProvider = rtmpUrlProvider_;
    }

    if (op == QLatin1String("getVersion") || op == QLatin1String("version")) {
        result[QStringLiteral("ok")] = true;
        result[QStringLiteral("service")] = QStringLiteral("RailShot");
        result[QStringLiteral("appVersion")] = QStringLiteral("1.0.0");
        result[QStringLiteral("protocol")] = kRemoteProtocolVersion;
        return result;
    }
    if (op == QLatin1String("getStatus") || op == QLatin1String("status")) {
        return statusJson();
    }
    if (op == QLatin1String("listScenes") || op == QLatin1String("scenes")
        || op == QLatin1String("GetSceneList")) {
        return scenesJson();
    }
    if (op == QLatin1String("getScore") || op == QLatin1String("score")) {
        return scoreJson();
    }
    if (op == QLatin1String("listCollections") || op == QLatin1String("getCollections")) {
        return collectionsJson();
    }
    if (op == QLatin1String("getCurrentCollection")) {
        result = collectionsJson();
        result[QStringLiteral("id")] =
            QString::fromStdString(SceneManager::instance().currentCollectionId());
        result[QStringLiteral("name")] =
            QString::fromStdString(SceneManager::instance().collection().name);
        return result;
    }
    if (op == QLatin1String("getStudioMode")) {
        result[QStringLiteral("ok")] = true;
        result[QStringLiteral("studioMode")] =
            controller ? controller->isStudioModeEnabled()
                       : SceneManager::instance().isStudioModeEnabled();
        return result;
    }
    if (op == QLatin1String("adjustScore")) {
        ScoreManager::instance().adjustScore(data.value(QStringLiteral("player")).toInt(),
                                             data.value(QStringLiteral("delta")).toInt());
        result = scoreJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("setActiveScene") || op == QLatin1String("SetCurrentProgramScene")) {
        const std::string id = data.value(QStringLiteral("sceneId")).toString().toStdString();
        if (id.empty() || !SceneManager::instance().sceneById(id)) {
            result[QStringLiteral("error")] = QStringLiteral("unknown scene");
            emit commandExecuted(op, false);
            return result;
        }
        if (controller) {
            controller->setActiveScene(id);
        } else {
            SceneManager::instance().setActiveScene(id);
        }
        result = scenesJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("setPreviewScene") || op == QLatin1String("SetCurrentPreviewScene")) {
        const std::string id = data.value(QStringLiteral("sceneId")).toString().toStdString();
        if (id.empty() || !SceneManager::instance().sceneById(id)) {
            result[QStringLiteral("error")] = QStringLiteral("unknown scene");
            emit commandExecuted(op, false);
            return result;
        }
        if (controller) {
            controller->setPreviewScene(id);
        } else {
            SceneManager::instance().setPreviewScene(id);
        }
        result = scenesJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("setStudioMode") || op == QLatin1String("SetStudioModeEnabled")) {
        const bool enabled = data.value(QStringLiteral("enabled")).toBool(
            data.value(QStringLiteral("studioMode")).toBool(true));
        if (controller) {
            controller->setStudioModeEnabled(enabled);
        } else {
            SceneManager::instance().setStudioModeEnabled(enabled);
        }
        result = statusJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("studioTransition") || op == QLatin1String("transition")
        || op == QLatin1String("TriggerStudioModeTransition")) {
        if (controller) {
            controller->transitionPreviewToProgram();
        } else {
            SceneManager::instance().swapPreviewAndProgram();
        }
        result = statusJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("createCollection")) {
        QString busy;
        if (outputBusy(controller, &busy)) {
            result[QStringLiteral("error")] = busy;
            emit commandExecuted(op, false);
            return result;
        }
        const std::string name = data.value(QStringLiteral("name")).toString().toStdString();
        if (!SceneManager::instance().createCollection(name)) {
            result[QStringLiteral("error")] = QStringLiteral("failed to create collection");
            emit commandExecuted(op, false);
            return result;
        }
        AppSettings::instance().setActiveCollectionId(
            SceneManager::instance().currentCollectionId());
        if (controller) {
            controller->onSceneCollectionChanged();
        }
        result = collectionsJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("renameCollection")) {
        const std::string id = data.value(QStringLiteral("id")).toString().toStdString();
        const std::string name = data.value(QStringLiteral("name")).toString().toStdString();
        if (!SceneManager::instance().renameCollection(id.empty()
                                                           ? SceneManager::instance().currentCollectionId()
                                                           : id,
                                                       name)) {
            result[QStringLiteral("error")] = QStringLiteral("failed to rename collection");
            emit commandExecuted(op, false);
            return result;
        }
        result = collectionsJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("deleteCollection")) {
        QString busy;
        if (outputBusy(controller, &busy)) {
            result[QStringLiteral("error")] = busy;
            emit commandExecuted(op, false);
            return result;
        }
        const std::string id = data.value(QStringLiteral("id")).toString().toStdString();
        const std::string target =
            id.empty() ? SceneManager::instance().currentCollectionId() : id;
        if (!SceneManager::instance().deleteCollection(target)) {
            result[QStringLiteral("error")] = QStringLiteral("failed to delete collection");
            emit commandExecuted(op, false);
            return result;
        }
        AppSettings::instance().setActiveCollectionId(
            SceneManager::instance().currentCollectionId());
        if (controller) {
            controller->onSceneCollectionChanged();
        }
        result = collectionsJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("switchCollection") || op == QLatin1String("setCurrentCollection")) {
        QString busy;
        if (outputBusy(controller, &busy)) {
            result[QStringLiteral("error")] = busy;
            emit commandExecuted(op, false);
            return result;
        }
        const std::string id = data.value(QStringLiteral("id")).toString().toStdString();
        if (!SceneManager::instance().switchCollection(id)) {
            result[QStringLiteral("error")] = QStringLiteral("failed to switch collection");
            emit commandExecuted(op, false);
            return result;
        }
        AppSettings::instance().setActiveCollectionId(
            SceneManager::instance().currentCollectionId());
        if (controller) {
            controller->onSceneCollectionChanged();
        }
        result = collectionsJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("startStream")) {
        if (!controller) {
            result[QStringLiteral("error")] = QStringLiteral("no controller");
            emit commandExecuted(op, false);
            return result;
        }
        if (controller->isStreaming()) {
            result = statusJson();
            return result;
        }
        std::string url = data.value(QStringLiteral("rtmpUrl")).toString().toStdString();
        if (url.empty() && rtmpProvider) {
            url = rtmpProvider();
        }
        if (url.empty()) {
            result[QStringLiteral("error")] = QStringLiteral("missing RTMP URL");
            emit commandExecuted(op, false);
            return result;
        }
        if (!controller->setRtmpUrl(url) || !controller->startStream()) {
            result[QStringLiteral("error")] = QStringLiteral("failed to start stream");
            emit commandExecuted(op, false);
            return result;
        }
        result = statusJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("stopStream")) {
        if (controller && controller->isStreaming()) {
            controller->stopStream();
            controller->ensurePreviewEngine();
        }
        result = statusJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("startRecording")) {
        if (!controller || !controller->startRecording()) {
            result[QStringLiteral("error")] = QStringLiteral("failed to start recording");
            emit commandExecuted(op, false);
            return result;
        }
        result = statusJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("stopRecording")) {
        if (controller) {
            controller->stopRecording();
        }
        result = statusJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }
    if (op == QLatin1String("selectSceneByIndex")) {
        const int index = data.value(QStringLiteral("index")).toInt(-1);
        const auto& scenes = SceneManager::instance().collection().scenes;
        if (index < 0 || index >= static_cast<int>(scenes.size())) {
            result[QStringLiteral("error")] = QStringLiteral("invalid scene index");
            emit commandExecuted(op, false);
            return result;
        }
        const std::string id = scenes[static_cast<size_t>(index)].id;
        if (controller) {
            controller->setActiveScene(id);
        } else {
            SceneManager::instance().setActiveScene(id);
        }
        result = scenesJson();
        emit commandExecuted(op, true);
        refresh();
        return result;
    }

    result[QStringLiteral("error")] = QStringLiteral("unknown op");
    Logger::warn("RemoteCommandBus: unknown op " + op.toStdString());
    emit commandExecuted(op, false);
    return result;
}

} // namespace railshot
