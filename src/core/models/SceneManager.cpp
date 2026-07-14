#include "core/models/SceneManager.h"

#include "core/Logger.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

namespace railshot {

namespace {

QJsonObject transformToJson(const SourceTransform& t) {
    QJsonObject obj;
    obj["x"] = t.x;
    obj["y"] = t.y;
    obj["width"] = t.width;
    obj["height"] = t.height;
    obj["rotation"] = t.rotation;
    obj["cropLeft"] = t.cropLeft;
    obj["cropTop"] = t.cropTop;
    obj["cropRight"] = t.cropRight;
    obj["cropBottom"] = t.cropBottom;
    return obj;
}

SourceTransform transformFromJson(const QJsonObject& obj) {
    SourceTransform t;
    t.x = static_cast<float>(obj.value("x").toDouble(t.x));
    t.y = static_cast<float>(obj.value("y").toDouble(t.y));
    t.width = static_cast<float>(obj.value("width").toDouble(t.width));
    t.height = static_cast<float>(obj.value("height").toDouble(t.height));
    t.rotation = static_cast<float>(obj.value("rotation").toDouble(t.rotation));
    t.cropLeft = static_cast<float>(obj.value("cropLeft").toDouble(t.cropLeft));
    t.cropTop = static_cast<float>(obj.value("cropTop").toDouble(t.cropTop));
    t.cropRight = static_cast<float>(obj.value("cropRight").toDouble(t.cropRight));
    t.cropBottom = static_cast<float>(obj.value("cropBottom").toDouble(t.cropBottom));
    return t;
}

QString sourceTypeToString(SourceType type) {
    switch (type) {
    case SourceType::VideoDevice: return "VideoDevice";
    case SourceType::AudioDevice: return "AudioDevice";
    case SourceType::Image: return "Image";
    case SourceType::MediaFile: return "MediaFile";
    case SourceType::NDI: return "NDI";
    case SourceType::Browser: return "Browser";
    case SourceType::Scoreboard: return "Scoreboard";
    case SourceType::DisplayCapture: return "DisplayCapture";
    case SourceType::WindowCapture: return "WindowCapture";
    case SourceType::Text: return "Text";
    case SourceType::Color: return "Color";
    case SourceType::DesktopAudio: return "DesktopAudio";
    case SourceType::ApplicationAudio: return "ApplicationAudio";
    case SourceType::GameCapture: return "GameCapture";
    }
    return "VideoDevice";
}

SourceType sourceTypeFromString(const QString& s) {
    if (s == "AudioDevice") return SourceType::AudioDevice;
    if (s == "Image") return SourceType::Image;
    if (s == "MediaFile") return SourceType::MediaFile;
    if (s == "NDI") return SourceType::NDI;
    if (s == "Browser") return SourceType::Browser;
    if (s == "Scoreboard") return SourceType::Scoreboard;
    if (s == "DisplayCapture") return SourceType::DisplayCapture;
    if (s == "WindowCapture") return SourceType::WindowCapture;
    if (s == "Text") return SourceType::Text;
    if (s == "Color") return SourceType::Color;
    if (s == "DesktopAudio") return SourceType::DesktopAudio;
    if (s == "ApplicationAudio") return SourceType::ApplicationAudio;
    if (s == "GameCapture") return SourceType::GameCapture;
    return SourceType::VideoDevice;
}

QString transitionTypeToString(TransitionType type) {
    switch (type) {
    case TransitionType::Cut: return "Cut";
    case TransitionType::Fade: return "Fade";
    case TransitionType::Slide: return "Slide";
    case TransitionType::FadeToBlack: return "FadeToBlack";
    }
    return "Fade";
}

TransitionType transitionTypeFromString(const QString& s) {
    if (s == "Cut") return TransitionType::Cut;
    if (s == "Slide") return TransitionType::Slide;
    if (s == "FadeToBlack") return TransitionType::FadeToBlack;
    return TransitionType::Fade;
}

QString filterTypeToString(FilterType type) {
    switch (type) {
    case FilterType::ColorCorrection:
        return QStringLiteral("ColorCorrection");
    case FilterType::Gain:
        return QStringLiteral("Gain");
    case FilterType::Compressor:
        return QStringLiteral("Compressor");
    case FilterType::NoiseGate:
        return QStringLiteral("NoiseGate");
    case FilterType::NoiseSuppress:
        return QStringLiteral("NoiseSuppress");
    case FilterType::Crop:
        return QStringLiteral("Crop");
    case FilterType::ChromaKey:
        return QStringLiteral("ChromaKey");
    case FilterType::ColorKey:
        return QStringLiteral("ColorKey");
    case FilterType::ImageMask:
        return QStringLiteral("ImageMask");
    case FilterType::ColorGrade:
        return QStringLiteral("ColorGrade");
    case FilterType::Scale:
        return QStringLiteral("Scale");
    case FilterType::Scroll:
        return QStringLiteral("Scroll");
    case FilterType::Sharpness:
        return QStringLiteral("Sharpness");
    case FilterType::RenderDelay:
        return QStringLiteral("RenderDelay");
    case FilterType::Opacity:
    default:
        return QStringLiteral("Opacity");
    }
}

FilterType filterTypeFromString(const QString& s) {
    if (s == "ColorCorrection") return FilterType::ColorCorrection;
    if (s == "Gain") return FilterType::Gain;
    if (s == "Compressor") return FilterType::Compressor;
    if (s == "NoiseGate") return FilterType::NoiseGate;
    if (s == "NoiseSuppress") return FilterType::NoiseSuppress;
    if (s == "Crop") return FilterType::Crop;
    if (s == "ChromaKey") return FilterType::ChromaKey;
    if (s == "ColorKey") return FilterType::ColorKey;
    if (s == "ImageMask") return FilterType::ImageMask;
    if (s == "ColorGrade") return FilterType::ColorGrade;
    if (s == "Scale") return FilterType::Scale;
    if (s == "Scroll") return FilterType::Scroll;
    if (s == "Sharpness") return FilterType::Sharpness;
    if (s == "RenderDelay") return FilterType::RenderDelay;
    return FilterType::Opacity;
}

QJsonObject filterToJson(const SourceFilter& filter) {
    QJsonObject obj;
    obj["id"] = QString::fromStdString(filter.id);
    obj["type"] = filterTypeToString(filter.type);
    obj["enabled"] = filter.enabled;
    obj["paramsJson"] = QString::fromStdString(filter.paramsJson);
    return obj;
}

SourceFilter filterFromJson(const QJsonObject& obj) {
    SourceFilter filter;
    filter.id = obj.value("id").toString().toStdString();
    filter.type = filterTypeFromString(obj.value("type").toString());
    filter.enabled = obj.value("enabled").toBool(true);
    filter.paramsJson = obj.value("paramsJson").toString().toStdString();
    return filter;
}

QJsonObject sourceToJson(const Source& src) {
    QJsonObject obj;
    obj["id"] = QString::fromStdString(src.id);
    obj["name"] = QString::fromStdString(src.name);
    obj["type"] = sourceTypeToString(src.type);
    obj["transform"] = transformToJson(src.transform);
    obj["isVisible"] = src.isVisible;
    obj["locked"] = src.locked;
    obj["zOrder"] = src.zOrder;
    obj["pathOrDeviceId"] = QString::fromStdString(src.pathOrDeviceId);
    obj["volume"] = src.volume;
    obj["muted"] = src.muted;
    obj["syncDelayMs"] = src.syncDelayMs;
    obj["loop"] = src.loop;
    obj["isoRecording"] = src.isoRecording;
    obj["overlaySettings"] = QString::fromStdString(src.overlaySettings);
    QJsonArray filters;
    for (const auto& f : src.filters) {
        filters.append(filterToJson(f));
    }
    obj["filters"] = filters;
    return obj;
}

Source sourceFromJson(const QJsonObject& obj) {
    Source src;
    src.id = obj.value("id").toString().toStdString();
    src.name = obj.value("name").toString().toStdString();
    src.type = sourceTypeFromString(obj.value("type").toString());
    src.transform = transformFromJson(obj.value("transform").toObject());
    src.isVisible = obj.value("isVisible").toBool(true);
    src.locked = obj.value("locked").toBool(false);
    src.zOrder = obj.value("zOrder").toInt(0);
    src.pathOrDeviceId = obj.value("pathOrDeviceId").toString().toStdString();
    src.volume = obj.value("volume").toInt(100);
    src.muted = obj.value("muted").toBool(false);
    src.syncDelayMs = std::clamp(obj.value("syncDelayMs").toInt(0), 0, 2000);
    src.loop = obj.value("loop").toBool(true);
    src.isoRecording = obj.value("isoRecording").toBool(false);
    src.overlaySettings = obj.value("overlaySettings").toString().toStdString();
    const QJsonArray filters = obj.value("filters").toArray();
    for (const auto& value : filters) {
        if (value.isObject()) {
            src.filters.push_back(filterFromJson(value.toObject()));
        }
    }
    return src;
}

QJsonObject sceneToJson(const Scene& scene) {
    QJsonObject obj;
    obj["id"] = QString::fromStdString(scene.id);
    obj["name"] = QString::fromStdString(scene.name);
    obj["transitionType"] = transitionTypeToString(scene.transitionType);
    QJsonArray sources;
    for (const auto& src : scene.sources) {
        sources.append(sourceToJson(src));
    }
    obj["sources"] = sources;
    return obj;
}

Scene sceneFromJson(const QJsonObject& obj) {
    Scene scene;
    scene.id = obj.value("id").toString().toStdString();
    scene.name = obj.value("name").toString().toStdString();
    scene.transitionType = transitionTypeFromString(obj.value("transitionType").toString());
    const QJsonArray sources = obj.value("sources").toArray();
    for (const auto& val : sources) {
        scene.sources.push_back(sourceFromJson(val.toObject()));
    }
    return scene;
}

} // namespace

SceneManager& SceneManager::instance() {
    static SceneManager manager;
    return manager;
}

SceneManager::SceneManager() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dataDir_ = dataDir.toStdString();
    collectionsDir_ = (dataDir + "/scene_collections").toStdString();
    indexPath_ = (dataDir + "/collections.json").toStdString();
    legacyScenesPath_ = (dataDir + "/scenes.json").toStdString();
    initializeCollections();
    lastAutoSave_ = std::chrono::steady_clock::now();
}

std::string SceneManager::generateId() {
    static std::mt19937 rng{std::random_device{}()};
    static std::uniform_int_distribution<int> dist(0, 15);
    const char* hex = "0123456789abcdef";
    std::string id;
    id.reserve(16);
    for (int i = 0; i < 16; ++i) {
        id.push_back(hex[dist(rng)]);
    }
    return id;
}

void SceneManager::ensureDirectories() const {
    QDir().mkpath(QString::fromStdString(dataDir_));
    QDir().mkpath(QString::fromStdString(collectionsDir_));
}

void SceneManager::resetTransientState() {
    studioProgramScene_.reset();
    transitioning_ = false;
    outgoingSceneId_.clear();
    transitionBlend_ = 1.0f;
}

bool SceneManager::writeCollectionFileUnlocked(const std::string& path) const {
    QJsonObject root;
    root["id"] = QString::fromStdString(collection_.id);
    root["name"] = QString::fromStdString(collection_.name);
    root["activeSceneId"] = QString::fromStdString(activeSceneId_);
    root["previewSceneId"] = QString::fromStdString(previewSceneId_);

    QJsonArray scenes;
    for (const auto& scene : collection_.scenes) {
        scenes.append(sceneToJson(scene));
    }
    root["scenes"] = scenes;

    const QJsonDocument doc(root);
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    file.write(bytes.constData(), bytes.size());
    return true;
}

bool SceneManager::readCollectionFileUnlocked(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    const std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(content));
    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    collection_.id = root.value("id").toString("default").toStdString();
    collection_.name = root.value("name").toString("Default Collection").toStdString();
    collection_.scenes.clear();

    const QJsonArray scenes = root.value("scenes").toArray();
    for (const auto& val : scenes) {
        collection_.scenes.push_back(sceneFromJson(val.toObject()));
    }

    activeSceneId_ = root.value("activeSceneId").toString().toStdString();
    previewSceneId_ = root.value("previewSceneId").toString().toStdString();
    if (activeSceneId_.empty() && !collection_.scenes.empty()) {
        activeSceneId_ = collection_.scenes.front().id;
    }
    if (previewSceneId_.empty() && !collection_.scenes.empty()) {
        previewSceneId_ = activeSceneId_;
    }
    resetTransientState();
    return true;
}

bool SceneManager::loadIndex() {
    index_.clear();
    std::ifstream in(indexPath_);
    if (!in) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(content));
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject root = doc.object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    for (const auto& val : items) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject obj = val.toObject();
        CollectionInfo info;
        info.id = obj.value(QStringLiteral("id")).toString().toStdString();
        info.name = obj.value(QStringLiteral("name")).toString().toStdString();
        if (!info.id.empty()) {
            index_.push_back(info);
        }
    }
    const std::string activeId = root.value(QStringLiteral("activeId")).toString().toStdString();
    if (!activeId.empty()) {
        for (const auto& info : index_) {
            if (info.id == activeId) {
                collection_.id = activeId;
                break;
            }
        }
    }
    return !index_.empty();
}

bool SceneManager::saveIndex() const {
    QJsonObject root;
    root[QStringLiteral("activeId")] = QString::fromStdString(collection_.id);
    QJsonArray items;
    for (const auto& info : index_) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = QString::fromStdString(info.id);
        obj[QStringLiteral("name")] = QString::fromStdString(info.name);
        items.append(obj);
    }
    root[QStringLiteral("items")] = items;
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    std::ofstream out(indexPath_, std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(bytes.constData(), bytes.size());
    return true;
}

std::string SceneManager::makeDefaultCollectionUnlocked(const std::string& name) {
    collection_.id = generateId();
    collection_.name = name.empty() ? "Default Collection" : name;
    collection_.scenes.clear();
    Scene scene;
    scene.id = generateId();
    scene.name = "Scene";
    scene.transitionType = TransitionType::Fade;
    collection_.scenes.push_back(scene);
    activeSceneId_ = scene.id;
    previewSceneId_ = scene.id;
    resetTransientState();
    return collection_.id;
}

void SceneManager::initializeCollections() {
    ensureDirectories();
    std::lock_guard lock(mutex_);

    if (loadIndex()) {
        std::string activeId = collection_.id;
        bool found = false;
        for (const auto& info : index_) {
            if (info.id == activeId) {
                found = true;
                break;
            }
        }
        if (!found) {
            activeId = index_.front().id;
        }
        savePath_ = collectionFilePath(activeId);
        if (!readCollectionFileUnlocked(savePath_)) {
            makeDefaultCollectionUnlocked(index_.front().name);
            collection_.id = activeId;
            writeCollectionFileUnlocked(savePath_);
        }
        Logger::info("SceneManager: loaded collection " + collection_.name);
        return;
    }

    if (std::filesystem::exists(legacyScenesPath_) && readCollectionFileUnlocked(legacyScenesPath_)) {
        if (collection_.id.empty() || collection_.id == "default") {
            collection_.id = generateId();
        }
        if (collection_.name.empty()) {
            collection_.name = "Default Collection";
        }
        Logger::info("SceneManager: migrating legacy scenes.json");
    } else {
        makeDefaultCollectionUnlocked("Default Collection");
    }

    savePath_ = collectionFilePath(collection_.id);
    writeCollectionFileUnlocked(savePath_);
    index_.clear();
    index_.push_back(CollectionInfo{collection_.id, collection_.name});
    saveIndex();
}

std::string SceneManager::collectionFilePath(const std::string& id) const {
    return collectionsDir_ + "/" + id + ".json";
}

std::vector<CollectionInfo> SceneManager::listCollections() const {
    std::lock_guard lock(mutex_);
    return index_;
}

std::string SceneManager::currentCollectionId() const {
    std::lock_guard lock(mutex_);
    return collection_.id;
}

bool SceneManager::saveNow() {
    std::lock_guard lock(mutex_);
    ensureDirectories();
    if (!writeCollectionFileUnlocked(savePath_)) {
        return false;
    }
    lastAutoSave_ = std::chrono::steady_clock::now();
    return saveIndex();
}

bool SceneManager::createCollection(const std::string& name) {
    {
        std::lock_guard lock(mutex_);
        ensureDirectories();
        writeCollectionFileUnlocked(savePath_);
        makeDefaultCollectionUnlocked(name.empty() ? "New Collection" : name);
        savePath_ = collectionFilePath(collection_.id);
        writeCollectionFileUnlocked(savePath_);
        index_.push_back(CollectionInfo{collection_.id, collection_.name});
        saveIndex();
    }
    notifyChanged();
    return true;
}

bool SceneManager::duplicateCollection(const std::string& name) {
    {
        std::lock_guard lock(mutex_);
        ensureDirectories();
        writeCollectionFileUnlocked(savePath_);
        const std::string newId = generateId();
        const std::string newName = name.empty() ? (collection_.name + " Copy") : name;
        SceneCollection copy = collection_;
        copy.id = newId;
        copy.name = newName;
        collection_ = std::move(copy);
        savePath_ = collectionFilePath(newId);
        writeCollectionFileUnlocked(savePath_);
        index_.push_back(CollectionInfo{newId, newName});
        saveIndex();
        resetTransientState();
    }
    notifyChanged();
    return true;
}

bool SceneManager::renameCollection(const std::string& id, const std::string& name) {
    if (id.empty() || name.empty()) {
        return false;
    }
    {
        std::lock_guard lock(mutex_);
        bool found = false;
        for (auto& info : index_) {
            if (info.id == id) {
                info.name = name;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
        if (collection_.id == id) {
            collection_.name = name;
            writeCollectionFileUnlocked(savePath_);
        } else {
            const std::string path = collectionFilePath(id);
            std::ifstream in(path);
            if (in) {
                std::string content((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
                QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(content));
                if (doc.isObject()) {
                    QJsonObject root = doc.object();
                    root[QStringLiteral("name")] = QString::fromStdString(name);
                    std::ofstream out(path, std::ios::trunc);
                    if (out) {
                        const QByteArray bytes =
                            QJsonDocument(root).toJson(QJsonDocument::Indented);
                        out.write(bytes.constData(), bytes.size());
                    }
                }
            }
        }
        saveIndex();
    }
    notifyChanged();
    return true;
}

bool SceneManager::deleteCollection(const std::string& id) {
    {
        std::lock_guard lock(mutex_);
        if (index_.size() <= 1) {
            return false;
        }
        const auto it = std::find_if(index_.begin(), index_.end(),
                                     [&](const CollectionInfo& c) { return c.id == id; });
        if (it == index_.end()) {
            return false;
        }
        const bool deletingActive = (collection_.id == id);
        if (deletingActive) {
            writeCollectionFileUnlocked(savePath_);
        }
        index_.erase(it);
        std::error_code ec;
        std::filesystem::remove(collectionFilePath(id), ec);

        if (deletingActive) {
            const std::string nextId = index_.front().id;
            savePath_ = collectionFilePath(nextId);
            if (!readCollectionFileUnlocked(savePath_)) {
                makeDefaultCollectionUnlocked(index_.front().name);
                collection_.id = nextId;
                writeCollectionFileUnlocked(savePath_);
            }
            for (auto& info : index_) {
                if (info.id == collection_.id) {
                    info.name = collection_.name;
                    break;
                }
            }
        }
        saveIndex();
    }
    notifyChanged();
    return true;
}

bool SceneManager::switchCollection(const std::string& id) {
    {
        std::lock_guard lock(mutex_);
        if (id.empty() || id == collection_.id) {
            return id == collection_.id;
        }
        bool found = false;
        for (const auto& info : index_) {
            if (info.id == id) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }

        ensureDirectories();
        writeCollectionFileUnlocked(savePath_);
        savePath_ = collectionFilePath(id);
        if (!readCollectionFileUnlocked(savePath_)) {
            return false;
        }
        for (auto& info : index_) {
            if (info.id == collection_.id) {
                info.name = collection_.name;
                break;
            }
        }
        saveIndex();
    }
    notifyChanged();
    return true;
}

void SceneManager::loadDefaults() {
    std::lock_guard lock(mutex_);
    makeDefaultCollectionUnlocked(collection_.name.empty() ? "Default Collection"
                                                           : collection_.name);
    notifyChanged();
}

bool SceneManager::loadFromFile(const std::string& path) {
    std::lock_guard lock(mutex_);
    if (!readCollectionFileUnlocked(path)) {
        return false;
    }
    Logger::info("SceneManager: loaded " + std::to_string(collection_.scenes.size()) + " scenes");
    return true;
}

bool SceneManager::saveToFile(const std::string& path) const {
    std::lock_guard lock(mutex_);
    return writeCollectionFileUnlocked(path);
}

void SceneManager::tickAutoSave() {
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastAutoSave_).count() >= 60) {
        saveNow();
    }
}

SceneCollection& SceneManager::collection() {
    return collection_;
}

const SceneCollection& SceneManager::collection() const {
    return collection_;
}

std::string SceneManager::activeSceneId() const {
    std::lock_guard lock(mutex_);
    return activeSceneId_;
}

std::string SceneManager::previewSceneId() const {
    std::lock_guard lock(mutex_);
    return previewSceneId_;
}

Scene* SceneManager::previewScene() {
    std::lock_guard lock(mutex_);
    return sceneById(previewSceneId_);
}

const Scene* SceneManager::previewScene() const {
    std::lock_guard lock(mutex_);
    for (const auto& scene : collection_.scenes) {
        if (scene.id == previewSceneId_) {
            return &scene;
        }
    }
    return nullptr;
}

std::optional<Scene> SceneManager::previewSceneSnapshot() const {
    std::lock_guard lock(mutex_);
    for (const auto& scene : collection_.scenes) {
        if (scene.id == previewSceneId_) {
            return scene;
        }
    }
    return std::nullopt;
}

bool SceneManager::isStudioModeEnabled() const {
    std::lock_guard lock(mutex_);
    return studioModeEnabled_;
}

void SceneManager::setStudioModeEnabled(bool enabled) {
    std::lock_guard lock(mutex_);
    studioModeEnabled_ = enabled;
    if (enabled) {
        previewSceneId_ = activeSceneId_;
        syncProgramBufferFromSceneId(activeSceneId_);
    } else {
        studioProgramScene_.reset();
    }
}

void SceneManager::syncProgramBufferFromSceneId(const std::string& sceneId) {
    for (const auto& scene : collection_.scenes) {
        if (scene.id == sceneId) {
            studioProgramScene_ = scene;
            return;
        }
    }
    studioProgramScene_.reset();
}

std::optional<Scene> SceneManager::programSceneSnapshotForRender() const {
    std::lock_guard lock(mutex_);
    if (studioModeEnabled_ && studioProgramScene_.has_value()) {
        return studioProgramScene_;
    }
    for (const auto& scene : collection_.scenes) {
        if (scene.id == activeSceneId_) {
            return scene;
        }
    }
    return std::nullopt;
}

void SceneManager::setPreviewScene(const std::string& sceneId) {
    std::lock_guard lock(mutex_);
    if (!sceneById(sceneId) || sceneId == previewSceneId_) {
        return;
    }
    previewSceneId_ = sceneId;
    notifyChanged();
}

void SceneManager::swapPreviewAndProgram() {
    std::lock_guard lock(mutex_);
    if (previewSceneId_.empty()) {
        return;
    }

    if (previewSceneId_ == activeSceneId_) {
        syncProgramBufferFromSceneId(previewSceneId_);
        transitioning_ = false;
        outgoingSceneId_.clear();
        transitionBlend_ = 1.0f;
        notifyChanged();
        return;
    }

    const std::string oldProgram = activeSceneId_;
    const std::string newProgram = previewSceneId_;
    previewSceneId_ = oldProgram;

    const Scene* incoming = sceneById(newProgram);
    if (!incoming) {
        return;
    }

    if (incoming->transitionType == TransitionType::Cut) {
        activeSceneId_ = newProgram;
        transitioning_ = false;
        outgoingSceneId_.clear();
        transitionBlend_ = 1.0f;
        if (studioModeEnabled_) {
            syncProgramBufferFromSceneId(activeSceneId_);
        }
    } else {
        beginTransition(newProgram, incoming->transitionType);
    }
    notifyChanged();
}

Scene* SceneManager::activeScene() {
    std::lock_guard lock(mutex_);
    return sceneById(activeSceneId_);
}

const Scene* SceneManager::activeScene() const {
    std::lock_guard lock(mutex_);
    for (const auto& scene : collection_.scenes) {
        if (scene.id == activeSceneId_) {
            return &scene;
        }
    }
    return nullptr;
}

Scene* SceneManager::sceneById(const std::string& id) {
    for (auto& scene : collection_.scenes) {
        if (scene.id == id) {
            return &scene;
        }
    }
    return nullptr;
}

std::optional<Scene> SceneManager::sceneSnapshot(const std::string& id) const {
    std::lock_guard lock(mutex_);
    for (const auto& scene : collection_.scenes) {
        if (scene.id == id) {
            return scene;
        }
    }
    return std::nullopt;
}

std::optional<Scene> SceneManager::activeSceneSnapshot() const {
    std::lock_guard lock(mutex_);
    for (const auto& scene : collection_.scenes) {
        if (scene.id == activeSceneId_) {
            return scene;
        }
    }
    return std::nullopt;
}

const Scene* SceneManager::sceneById(const std::string& id) const {
    std::lock_guard lock(mutex_);
    for (const auto& scene : collection_.scenes) {
        if (scene.id == id) {
            return &scene;
        }
    }
    return nullptr;
}

Source* SceneManager::sourceById(const std::string& sourceId) {
    for (auto& scene : collection_.scenes) {
        for (auto& src : scene.sources) {
            if (src.id == sourceId) {
                return &src;
            }
        }
    }
    return nullptr;
}

const Source* SceneManager::sourceById(const std::string& sourceId) const {
    for (const auto& scene : collection_.scenes) {
        for (const auto& src : scene.sources) {
            if (src.id == sourceId) {
                return &src;
            }
        }
    }
    return nullptr;
}

Source* SceneManager::sourceInScene(const std::string& sceneId, const std::string& sourceId) {
    Scene* scene = sceneById(sceneId);
    if (!scene) {
        return nullptr;
    }
    for (auto& src : scene->sources) {
        if (src.id == sourceId) {
            return &src;
        }
    }
    return nullptr;
}

Scene& SceneManager::addScene(const std::string& name) {
    std::lock_guard lock(mutex_);
    Scene scene;
    scene.id = generateId();
    scene.name = name;
    collection_.scenes.push_back(scene);
    notifyChanged();
    return collection_.scenes.back();
}

bool SceneManager::removeScene(const std::string& sceneId) {
    std::lock_guard lock(mutex_);
    const auto it = std::remove_if(collection_.scenes.begin(), collection_.scenes.end(),
                                   [&](const Scene& s) { return s.id == sceneId; });
    if (it == collection_.scenes.end()) {
        return false;
    }
    collection_.scenes.erase(it, collection_.scenes.end());
    if (activeSceneId_ == sceneId && !collection_.scenes.empty()) {
        activeSceneId_ = collection_.scenes.front().id;
    }
    if (previewSceneId_ == sceneId && !collection_.scenes.empty()) {
        previewSceneId_ = collection_.scenes.front().id;
    }
    notifyChanged();
    return true;
}

Source& SceneManager::addSource(const std::string& sceneId, const Source& sourceTemplate) {
    std::lock_guard lock(mutex_);
    Scene* scene = sceneById(sceneId);
    if (!scene) {
        static Source empty;
        return empty;
    }
    Source src = sourceTemplate;
    if (src.id.empty()) {
        src.id = generateId();
    }
    scene->sources.push_back(src);
    notifyChanged();
    return scene->sources.back();
}

bool SceneManager::removeSource(const std::string& sceneId, const std::string& sourceId) {
    std::lock_guard lock(mutex_);
    Scene* scene = sceneById(sceneId);
    if (!scene) {
        return false;
    }
    const auto it = std::remove_if(scene->sources.begin(), scene->sources.end(),
                                   [&](const Source& s) { return s.id == sourceId; });
    if (it == scene->sources.end()) {
        return false;
    }
    scene->sources.erase(it, scene->sources.end());
    notifyChanged();
    return true;
}

void SceneManager::setActiveScene(const std::string& sceneId) {
    std::lock_guard lock(mutex_);
    if (sceneId == activeSceneId_ || !sceneById(sceneId)) {
        return;
    }

    const Scene* incoming = sceneById(sceneId);
    if (incoming->transitionType == TransitionType::Cut) {
        activeSceneId_ = sceneId;
        transitioning_ = false;
        outgoingSceneId_.clear();
        transitionBlend_ = 1.0f;
    } else {
        beginTransition(sceneId, incoming->transitionType);
    }
    notifyChanged();
}

void SceneManager::beginTransition(const std::string& newSceneId, TransitionType type) {
    outgoingSceneId_ = activeSceneId_;
    activeSceneId_ = newSceneId;
    transitioning_ = true;
    transitionBlend_ = 0.0f;
    transitionStart_ = std::chrono::steady_clock::now();
    activeTransitionType_ = type;
}

void SceneManager::updateTransition() {
    std::lock_guard lock(mutex_);
    if (!transitioning_) {
        return;
    }

    const auto elapsed = std::chrono::steady_clock::now() - transitionStart_;
    const float ms = static_cast<float>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    transitionBlend_ = std::clamp(ms / static_cast<float>(fadeDurationMs_), 0.0f, 1.0f);

    if (transitionBlend_ >= 1.0f) {
        transitioning_ = false;
        outgoingSceneId_.clear();
        if (studioModeEnabled_) {
            syncProgramBufferFromSceneId(activeSceneId_);
        }
    }
}

bool SceneManager::isTransitioning() const {
    std::lock_guard lock(mutex_);
    return transitioning_;
}

float SceneManager::transitionBlend() const {
    std::lock_guard lock(mutex_);
    return transitionBlend_;
}

TransitionType SceneManager::activeTransitionType() const {
    std::lock_guard lock(mutex_);
    return activeTransitionType_;
}

void SceneManager::setFadeDurationMs(int ms) {
    std::lock_guard lock(mutex_);
    fadeDurationMs_ = std::clamp(ms, 0, 10000);
}

std::string SceneManager::outgoingSceneId() const {
    std::lock_guard lock(mutex_);
    return outgoingSceneId_;
}

void SceneManager::setOnScenesChanged(std::function<void()> callback) {
    std::lock_guard lock(mutex_);
    onScenesChanged_ = std::move(callback);
}

void SceneManager::notifyChanged() {
    if (onScenesChanged_) {
        onScenesChanged_();
    }
}

} // namespace railshot
