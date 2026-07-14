#include "network/RemoteWebSocketServer.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "core/RemoteCommandBus.h"
#include "core/models/SceneManager.h"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QWebSocket>
#include <QWebSocketServer>

namespace railshot {

namespace {

enum ObsOp : int {
    Hello = 0,
    Identify = 1,
    Identified = 2,
    Reidentify = 3,
    Event = 5,
    Request = 6,
    RequestResponse = 7,
    RequestBatch = 8,
    RequestBatchResponse = 9,
};

QString randomHex(int bytes) {
    QByteArray raw;
    raw.resize(bytes);
    for (int i = 0; i < bytes; ++i) {
        raw[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return QString::fromLatin1(raw.toHex());
}

std::string sceneIdFromName(const QString& name) {
    for (const auto& scene : SceneManager::instance().collection().scenes) {
        if (QString::fromStdString(scene.name) == name) {
            return scene.id;
        }
    }
    return {};
}

} // namespace

RemoteWebSocketServer::RemoteWebSocketServer(QObject* parent)
    : QObject(parent) {
    connect(&RemoteCommandBus::instance(), &RemoteCommandBus::commandExecuted, this,
            [this](const QString& command, bool ok) {
                if (!ok) {
                    return;
                }

                const QJsonObject status =
                    RemoteCommandBus::instance().execute(QStringLiteral("getStatus"));
                const QJsonObject scenes =
                    RemoteCommandBus::instance().execute(QStringLiteral("GetSceneList"));

                if (command == QLatin1String("setActiveScene")
                    || command == QLatin1String("SetCurrentProgramScene")
                    || command == QLatin1String("selectSceneByIndex")) {
                    broadcastEvent(
                        QStringLiteral("CurrentProgramSceneChanged"),
                        QJsonObject{
                            {QStringLiteral("sceneName"),
                             scenes.value(QStringLiteral("currentProgramSceneName"))},
                            {QStringLiteral("sceneUuid"),
                             scenes.value(QStringLiteral("activeSceneId"))},
                        });
                } else if (command == QLatin1String("setPreviewScene")
                           || command == QLatin1String("SetCurrentPreviewScene")) {
                    broadcastEvent(
                        QStringLiteral("CurrentPreviewSceneChanged"),
                        QJsonObject{
                            {QStringLiteral("sceneName"),
                             scenes.value(QStringLiteral("currentPreviewSceneName"))},
                            {QStringLiteral("sceneUuid"),
                             scenes.value(QStringLiteral("previewSceneId"))},
                        });
                } else if (command == QLatin1String("setStudioMode")
                           || command == QLatin1String("SetStudioModeEnabled")) {
                    broadcastEvent(
                        QStringLiteral("StudioModeStateChanged"),
                        QJsonObject{
                            {QStringLiteral("studioModeEnabled"),
                             status.value(QStringLiteral("studioMode"))},
                        });
                } else if (command == QLatin1String("startStream")
                           || command == QLatin1String("stopStream")) {
                    broadcastEvent(
                        QStringLiteral("StreamStateChanged"),
                        QJsonObject{
                            {QStringLiteral("outputActive"),
                             status.value(QStringLiteral("streaming"))},
                            {QStringLiteral("outputState"),
                             status.value(QStringLiteral("streaming")).toBool()
                                 ? QStringLiteral("OBS_WEBSOCKET_OUTPUT_STARTED")
                                 : QStringLiteral("OBS_WEBSOCKET_OUTPUT_STOPPED")},
                        });
                } else if (command == QLatin1String("startRecording")
                           || command == QLatin1String("stopRecording")) {
                    broadcastEvent(
                        QStringLiteral("RecordStateChanged"),
                        QJsonObject{
                            {QStringLiteral("outputActive"),
                             status.value(QStringLiteral("recording"))},
                            {QStringLiteral("outputState"),
                             status.value(QStringLiteral("recording")).toBool()
                                 ? QStringLiteral("OBS_WEBSOCKET_OUTPUT_STARTED")
                                 : QStringLiteral("OBS_WEBSOCKET_OUTPUT_STOPPED")},
                        });
                } else if (command == QLatin1String("studioTransition")
                           || command == QLatin1String("transition")
                           || command == QLatin1String("TriggerStudioModeTransition")) {
                    broadcastEvent(QStringLiteral("SceneTransitionStarted"));
                } else if (command.contains(QLatin1String("Collection"), Qt::CaseInsensitive)) {
                    broadcastEvent(
                        QStringLiteral("CurrentSceneCollectionChanged"),
                        QJsonObject{
                            {QStringLiteral("sceneCollectionName"),
                             status.value(QStringLiteral("collectionName"))},
                        });
                }
            });
}

RemoteWebSocketServer::~RemoteWebSocketServer() {
    stop();
}

bool RemoteWebSocketServer::start(quint16 port) {
    if (server_ && server_->isListening()) {
        return true;
    }
    port_ = port;
    server_ = new QWebSocketServer(QStringLiteral("obs-websocket"), QWebSocketServer::NonSecureMode, this);
    if (!server_->listen(QHostAddress::Any, port_)) {
        Logger::error("RemoteWebSocketServer: failed to listen on " + std::to_string(port_));
        delete server_;
        server_ = nullptr;
        return false;
    }
    connect(server_, &QWebSocketServer::newConnection, this, &RemoteWebSocketServer::onNewConnection);
    Logger::info("RemoteWebSocketServer: listening at " + baseUrl().toStdString()
                 + " (obs-websocket compatible)");
    return true;
}

void RemoteWebSocketServer::stop() {
    if (!server_) {
        return;
    }
    const auto sockets = clients_.keys();
    for (QWebSocket* socket : sockets) {
        if (socket) {
            socket->close();
            socket->deleteLater();
        }
    }
    clients_.clear();
    server_->close();
    server_->deleteLater();
    server_ = nullptr;
}

bool RemoteWebSocketServer::isRunning() const {
    return server_ && server_->isListening();
}

QString RemoteWebSocketServer::baseUrl() const {
    QString host = QHostAddress(QHostAddress::LocalHost).toString();
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& addr : addresses) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            host = addr.toString();
            break;
        }
    }
    return QStringLiteral("ws://%1:%2").arg(host).arg(port_);
}

QString RemoteWebSocketServer::makeSalt() {
    return randomHex(16);
}

QString RemoteWebSocketServer::makeChallenge() {
    return randomHex(32);
}

QString RemoteWebSocketServer::sha256Base64(const QByteArray& input) {
    return QString::fromLatin1(
        QCryptographicHash::hash(input, QCryptographicHash::Sha256).toBase64());
}

bool RemoteWebSocketServer::verifyAuth(const QString& authentication, const ClientState& state) const {
    const QString password = QString::fromStdString(AppSettings::instance().data().websocketPassword);
    if (password.isEmpty()) {
        return true;
    }
    if (authentication.isEmpty() || state.salt.isEmpty() || state.challenge.isEmpty()) {
        return false;
    }
    const QString secret = sha256Base64((password + state.salt).toUtf8());
    const QString expected = sha256Base64((secret + state.challenge).toUtf8());
    return authentication == expected;
}

void RemoteWebSocketServer::sendJson(QWebSocket* socket, const QJsonObject& obj) {
    if (!socket) {
        return;
    }
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void RemoteWebSocketServer::sendHello(QWebSocket* socket) {
    ClientState& state = clients_[socket];
    state.identified = false;

    QJsonObject d;
    d[QStringLiteral("obsWebSocketVersion")] = QStringLiteral("5.5.0-railshot");
    d[QStringLiteral("rpcVersion")] = 1;
    // Compatibility note for RailShot clients.
    d[QStringLiteral("railshot")] = QJsonObject{
        {QStringLiteral("service"), QStringLiteral("RailShot")},
        {QStringLiteral("legacyOps"), true},
    };

    const QString password = QString::fromStdString(AppSettings::instance().data().websocketPassword);
    if (!password.isEmpty()) {
        state.salt = makeSalt();
        state.challenge = makeChallenge();
        d[QStringLiteral("authentication")] = QJsonObject{
            {QStringLiteral("challenge"), state.challenge},
            {QStringLiteral("salt"), state.salt},
        };
    } else {
        state.salt.clear();
        state.challenge.clear();
    }

    sendJson(socket, QJsonObject{{QStringLiteral("op"), Hello}, {QStringLiteral("d"), d}});
}

void RemoteWebSocketServer::onNewConnection() {
    while (server_ && server_->hasPendingConnections()) {
        QWebSocket* socket = server_->nextPendingConnection();
        clients_.insert(socket, ClientState{});
        connect(socket, &QWebSocket::textMessageReceived, this, &RemoteWebSocketServer::onTextMessage);
        connect(socket, &QWebSocket::disconnected, this, &RemoteWebSocketServer::onSocketDisconnected);
        sendHello(socket);
    }
}

QJsonObject RemoteWebSocketServer::handleIdentify(QWebSocket* socket, const QJsonObject& data) {
    ClientState& state = clients_[socket];
    const int rpc = data.value(QStringLiteral("rpcVersion")).toInt(1);
    if (rpc != 1) {
        return QJsonObject{
            {QStringLiteral("op"), Hello},
            {QStringLiteral("d"),
             QJsonObject{{QStringLiteral("error"), QStringLiteral("unsupported rpcVersion")}}},
        };
    }

    const QString auth = data.value(QStringLiteral("authentication")).toString();
    if (!verifyAuth(auth, state)) {
        socket->close(QWebSocketProtocol::CloseCodePolicyViolated, QStringLiteral("Authentication failed"));
        return {};
    }

    state.identified = true;
    return QJsonObject{
        {QStringLiteral("op"), Identified},
        {QStringLiteral("d"), QJsonObject{{QStringLiteral("negotiatedRpcVersion"), 1}}},
    };
}

QJsonObject RemoteWebSocketServer::handleObsRequest(const QJsonObject& data) {
    const QString requestType = data.value(QStringLiteral("requestType")).toString();
    const QString requestId = data.value(QStringLiteral("requestId")).toString();
    const QJsonObject requestData = data.value(QStringLiteral("requestData")).toObject();

    QJsonObject responseData;
    int code = 100;
    QString comment;
    bool ok = true;

    auto fail = [&](int c, const QString& msg) {
        ok = false;
        code = c;
        comment = msg;
    };

    auto mapSceneName = [&]() -> QJsonObject {
        QJsonObject mapped = requestData;
        const QString uuid = requestData.value(QStringLiteral("sceneUuid")).toString();
        if (!uuid.isEmpty()) {
            mapped[QStringLiteral("sceneId")] = uuid;
            return mapped;
        }
        const QString name = requestData.value(QStringLiteral("sceneName")).toString();
        if (!name.isEmpty()) {
            const std::string id = sceneIdFromName(name);
            if (id.empty()) {
                fail(600, QStringLiteral("scene not found"));
                return {};
            }
            mapped[QStringLiteral("sceneId")] = QString::fromStdString(id);
        }
        return mapped;
    };

    if (requestType == QLatin1String("GetVersion")) {
        responseData = QJsonObject{
            {QStringLiteral("obsVersion"), QStringLiteral("RailShot 1.0.0")},
            {QStringLiteral("obsWebSocketVersion"), QStringLiteral("5.5.0-railshot")},
            {QStringLiteral("rpcVersion"), 1},
            {QStringLiteral("availableRequests"),
             QJsonArray{QStringLiteral("GetVersion"), QStringLiteral("GetStats"),
                        QStringLiteral("GetSceneList"), QStringLiteral("GetCurrentProgramScene"),
                        QStringLiteral("GetCurrentPreviewScene"),
                        QStringLiteral("SetCurrentProgramScene"),
                        QStringLiteral("SetCurrentPreviewScene"),
                        QStringLiteral("GetStudioModeEnabled"),
                        QStringLiteral("SetStudioModeEnabled"),
                        QStringLiteral("TriggerStudioModeTransition"),
                        QStringLiteral("StartStream"), QStringLiteral("StopStream"),
                        QStringLiteral("ToggleStream"), QStringLiteral("GetStreamStatus"),
                        QStringLiteral("StartRecord"), QStringLiteral("StopRecord"),
                        QStringLiteral("ToggleRecord"), QStringLiteral("GetRecordStatus"),
                        QStringLiteral("GetSceneCollectionList"),
                        QStringLiteral("SetCurrentSceneCollection")}},
            {QStringLiteral("supportedImageFormats"), QJsonArray{QStringLiteral("png")}},
            {QStringLiteral("platform"), QStringLiteral("Windows")},
            {QStringLiteral("platformDescription"), QStringLiteral("RailShot TV Broadcaster")},
        };
    } else if (requestType == QLatin1String("GetStats")) {
        const QJsonObject status = RemoteCommandBus::instance().execute(QStringLiteral("getStatus"));
        responseData = QJsonObject{
            {QStringLiteral("cpuUsage"), 0.0},
            {QStringLiteral("memoryUsage"), 0.0},
            {QStringLiteral("availableDiskSpace"), 0.0},
            {QStringLiteral("activeFps"), status.value(QStringLiteral("streaming")).toBool() ? 30.0 : 0.0},
            {QStringLiteral("averageFrameRenderTime"), 0.0},
            {QStringLiteral("renderSkippedFrames"), 0},
            {QStringLiteral("renderTotalFrames"), 0},
            {QStringLiteral("outputSkippedFrames"), 0},
            {QStringLiteral("outputTotalFrames"), 0},
            {QStringLiteral("webSocketSessionIncomingMessages"), 0},
            {QStringLiteral("webSocketSessionOutgoingMessages"), 0},
        };
    } else if (requestType == QLatin1String("GetSceneList")) {
        const QJsonObject scenes = RemoteCommandBus::instance().execute(QStringLiteral("GetSceneList"));
        QJsonArray obsScenes;
        for (const QJsonValue& v : scenes.value(QStringLiteral("scenes")).toArray()) {
            const QJsonObject s = v.toObject();
            obsScenes.append(QJsonObject{
                {QStringLiteral("sceneName"), s.value(QStringLiteral("name"))},
                {QStringLiteral("sceneUuid"), s.value(QStringLiteral("id"))},
                {QStringLiteral("sceneIndex"), obsScenes.size()},
            });
        }
        responseData = QJsonObject{
            {QStringLiteral("currentProgramSceneName"),
             scenes.value(QStringLiteral("currentProgramSceneName"))},
            {QStringLiteral("currentPreviewSceneName"),
             scenes.value(QStringLiteral("currentPreviewSceneName"))},
            {QStringLiteral("currentProgramSceneUuid"),
             scenes.value(QStringLiteral("activeSceneId"))},
            {QStringLiteral("currentPreviewSceneUuid"),
             scenes.value(QStringLiteral("previewSceneId"))},
            {QStringLiteral("scenes"), obsScenes},
        };
    } else if (requestType == QLatin1String("GetCurrentProgramScene")
               || requestType == QLatin1String("GetCurrentPreviewScene")) {
        const QJsonObject scenes = RemoteCommandBus::instance().execute(QStringLiteral("GetSceneList"));
        const bool program = requestType.startsWith(QLatin1String("GetCurrentProgram"));
        responseData = QJsonObject{
            {QStringLiteral("sceneName"),
             scenes.value(program ? QStringLiteral("currentProgramSceneName")
                                  : QStringLiteral("currentPreviewSceneName"))},
            {QStringLiteral("sceneUuid"),
             scenes.value(program ? QStringLiteral("activeSceneId")
                                  : QStringLiteral("previewSceneId"))},
        };
    } else if (requestType == QLatin1String("SetCurrentProgramScene")) {
        const QJsonObject mapped = mapSceneName();
        if (!ok) {
            // fall through to status
        } else {
            const QJsonObject r = RemoteCommandBus::instance().execute(
                QStringLiteral("SetCurrentProgramScene"), mapped);
            if (!r.value(QStringLiteral("ok")).toBool()) {
                fail(600, r.value(QStringLiteral("error")).toString());
            }
        }
    } else if (requestType == QLatin1String("SetCurrentPreviewScene")) {
        const QJsonObject mapped = mapSceneName();
        if (ok) {
            const QJsonObject r = RemoteCommandBus::instance().execute(
                QStringLiteral("SetCurrentPreviewScene"), mapped);
            if (!r.value(QStringLiteral("ok")).toBool()) {
                fail(600, r.value(QStringLiteral("error")).toString());
            }
        }
    } else if (requestType == QLatin1String("GetStudioModeEnabled")) {
        const QJsonObject r =
            RemoteCommandBus::instance().execute(QStringLiteral("getStudioMode"));
        responseData = QJsonObject{
            {QStringLiteral("studioModeEnabled"), r.value(QStringLiteral("studioMode")).toBool()},
        };
    } else if (requestType == QLatin1String("SetStudioModeEnabled")) {
        QJsonObject payload;
        payload[QStringLiteral("enabled")] =
            requestData.value(QStringLiteral("studioModeEnabled")).toBool();
        const QJsonObject r =
            RemoteCommandBus::instance().execute(QStringLiteral("SetStudioModeEnabled"), payload);
        if (!r.value(QStringLiteral("ok")).toBool()) {
            fail(600, r.value(QStringLiteral("error")).toString());
        }
    } else if (requestType == QLatin1String("TriggerStudioModeTransition")) {
        const QJsonObject r =
            RemoteCommandBus::instance().execute(QStringLiteral("TriggerStudioModeTransition"));
        if (!r.value(QStringLiteral("ok")).toBool()) {
            fail(600, r.value(QStringLiteral("error")).toString());
        }
    } else if (requestType == QLatin1String("StartStream")) {
        const QJsonObject r = RemoteCommandBus::instance().execute(QStringLiteral("startStream"));
        if (!r.value(QStringLiteral("ok")).toBool()) {
            fail(500, r.value(QStringLiteral("error")).toString());
        }
    } else if (requestType == QLatin1String("StopStream")) {
        const QJsonObject ignored =
            RemoteCommandBus::instance().execute(QStringLiteral("stopStream"));
        Q_UNUSED(ignored);
    } else if (requestType == QLatin1String("ToggleStream")) {
        const QJsonObject status = RemoteCommandBus::instance().execute(QStringLiteral("getStatus"));
        if (status.value(QStringLiteral("streaming")).toBool()) {
            const QJsonObject ignored =
                RemoteCommandBus::instance().execute(QStringLiteral("stopStream"));
            Q_UNUSED(ignored);
            responseData[QStringLiteral("outputActive")] = false;
        } else {
            const QJsonObject r = RemoteCommandBus::instance().execute(QStringLiteral("startStream"));
            if (!r.value(QStringLiteral("ok")).toBool()) {
                fail(500, r.value(QStringLiteral("error")).toString());
            } else {
                responseData[QStringLiteral("outputActive")] = true;
            }
        }
    } else if (requestType == QLatin1String("GetStreamStatus")) {
        const QJsonObject status = RemoteCommandBus::instance().execute(QStringLiteral("getStatus"));
        responseData = QJsonObject{
            {QStringLiteral("outputActive"), status.value(QStringLiteral("streaming")).toBool()},
            {QStringLiteral("outputReconnecting"), false},
            {QStringLiteral("outputTimecode"), QStringLiteral("00:00:00.000")},
            {QStringLiteral("outputDuration"), 0},
            {QStringLiteral("outputCongestion"), 0.0},
            {QStringLiteral("outputBytes"), 0},
            {QStringLiteral("outputSkippedFrames"), 0},
            {QStringLiteral("outputTotalFrames"), 0},
        };
    } else if (requestType == QLatin1String("StartRecord")) {
        const QJsonObject r = RemoteCommandBus::instance().execute(QStringLiteral("startRecording"));
        if (!r.value(QStringLiteral("ok")).toBool()) {
            fail(500, r.value(QStringLiteral("error")).toString());
        }
    } else if (requestType == QLatin1String("StopRecord")) {
        const QJsonObject ignored =
            RemoteCommandBus::instance().execute(QStringLiteral("stopRecording"));
        Q_UNUSED(ignored);
    } else if (requestType == QLatin1String("ToggleRecord")) {
        const QJsonObject status = RemoteCommandBus::instance().execute(QStringLiteral("getStatus"));
        if (status.value(QStringLiteral("recording")).toBool()) {
            const QJsonObject ignored =
                RemoteCommandBus::instance().execute(QStringLiteral("stopRecording"));
            Q_UNUSED(ignored);
            responseData[QStringLiteral("outputActive")] = false;
        } else {
            const QJsonObject r =
                RemoteCommandBus::instance().execute(QStringLiteral("startRecording"));
            if (!r.value(QStringLiteral("ok")).toBool()) {
                fail(500, r.value(QStringLiteral("error")).toString());
            } else {
                responseData[QStringLiteral("outputActive")] = true;
            }
        }
    } else if (requestType == QLatin1String("GetRecordStatus")) {
        const QJsonObject status = RemoteCommandBus::instance().execute(QStringLiteral("getStatus"));
        responseData = QJsonObject{
            {QStringLiteral("outputActive"), status.value(QStringLiteral("recording")).toBool()},
            {QStringLiteral("outputPaused"), false},
            {QStringLiteral("outputTimecode"), QStringLiteral("00:00:00.000")},
            {QStringLiteral("outputDuration"), 0},
            {QStringLiteral("outputBytes"), 0},
        };
    } else if (requestType == QLatin1String("GetSceneCollectionList")) {
        const QJsonObject cols =
            RemoteCommandBus::instance().execute(QStringLiteral("listCollections"));
        QJsonArray names;
        for (const QJsonValue& v : cols.value(QStringLiteral("collections")).toArray()) {
            names.append(v.toObject().value(QStringLiteral("name")));
        }
        responseData = QJsonObject{
            {QStringLiteral("currentSceneCollectionName"),
             cols.value(QStringLiteral("collections")).toArray().isEmpty()
                 ? QString()
                 : QString()},
            {QStringLiteral("sceneCollections"), names},
        };
        // Fill current name.
        for (const QJsonValue& v : cols.value(QStringLiteral("collections")).toArray()) {
            const QJsonObject c = v.toObject();
            if (c.value(QStringLiteral("id")).toString()
                == cols.value(QStringLiteral("activeId")).toString()) {
                responseData[QStringLiteral("currentSceneCollectionName")] =
                    c.value(QStringLiteral("name"));
                break;
            }
        }
    } else if (requestType == QLatin1String("SetCurrentSceneCollection")) {
        const QString name = requestData.value(QStringLiteral("sceneCollectionName")).toString();
        QString id;
        const QJsonObject cols =
            RemoteCommandBus::instance().execute(QStringLiteral("listCollections"));
        for (const QJsonValue& v : cols.value(QStringLiteral("collections")).toArray()) {
            const QJsonObject c = v.toObject();
            if (c.value(QStringLiteral("name")).toString() == name) {
                id = c.value(QStringLiteral("id")).toString();
                break;
            }
        }
        if (id.isEmpty()) {
            fail(600, QStringLiteral("scene collection not found"));
        } else {
            QJsonObject payload;
            payload[QStringLiteral("id")] = id;
            const QJsonObject r = RemoteCommandBus::instance().execute(
                QStringLiteral("switchCollection"), payload);
            if (!r.value(QStringLiteral("ok")).toBool()) {
                fail(600, r.value(QStringLiteral("error")).toString());
            }
        }
    } else {
        fail(204, QStringLiteral("unknown request type: ") + requestType);
    }

    QJsonObject status;
    status[QStringLiteral("result")] = ok;
    status[QStringLiteral("code")] = code;
    if (!comment.isEmpty()) {
        status[QStringLiteral("comment")] = comment;
    }

    QJsonObject d;
    d[QStringLiteral("requestType")] = requestType;
    d[QStringLiteral("requestId")] = requestId;
    d[QStringLiteral("requestStatus")] = status;
    if (ok && !responseData.isEmpty()) {
        d[QStringLiteral("responseData")] = responseData;
    }
    return QJsonObject{{QStringLiteral("op"), RequestResponse}, {QStringLiteral("d"), d}};
}

QJsonObject RemoteWebSocketServer::handleLegacy(const QJsonObject& root) {
    const QString op = root.value(QStringLiteral("op")).toString();
    const QJsonObject data = root.value(QStringLiteral("d")).toObject();
    return RemoteCommandBus::instance().execute(op, data);
}

void RemoteWebSocketServer::onTextMessage(const QString& message) {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        sendJson(socket, QJsonObject{{QStringLiteral("ok"), false},
                                     {QStringLiteral("error"), QStringLiteral("expected JSON object")}});
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonValue opVal = root.value(QStringLiteral("op"));

    // Legacy RailShot string ops.
    if (opVal.isString()) {
        if (!AppSettings::instance().data().websocketPassword.empty()
            && !clients_.value(socket).identified) {
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                          QStringLiteral("Not identified"));
            return;
        }
        sendJson(socket, handleLegacy(root));
        return;
    }

    const int op = opVal.toInt(-1);
    if (op == Identify || op == Reidentify) {
        const QJsonObject reply = handleIdentify(socket, root.value(QStringLiteral("d")).toObject());
        if (!reply.isEmpty()) {
            sendJson(socket, reply);
        }
        return;
    }

    if (!clients_.value(socket).identified
        && !AppSettings::instance().data().websocketPassword.empty()) {
        // Password configured: require Identify first.
        socket->close(QWebSocketProtocol::CloseCodePolicyViolated, QStringLiteral("Not identified"));
        return;
    }
    // No password: treat first request as auto-identified.
    if (!clients_[socket].identified) {
        clients_[socket].identified = true;
    }

    if (op == Request) {
        sendJson(socket, handleObsRequest(root.value(QStringLiteral("d")).toObject()));
        return;
    }
    if (op == RequestBatch) {
        const QJsonObject batch = root.value(QStringLiteral("d")).toObject();
        const QJsonArray requests = batch.value(QStringLiteral("requests")).toArray();
        QJsonArray results;
        for (const QJsonValue& v : requests) {
            const QJsonObject resp = handleObsRequest(v.toObject());
            results.append(resp.value(QStringLiteral("d")));
        }
        sendJson(socket,
                 QJsonObject{{QStringLiteral("op"), RequestBatchResponse},
                             {QStringLiteral("d"),
                              QJsonObject{{QStringLiteral("requestId"),
                                           batch.value(QStringLiteral("requestId"))},
                                          {QStringLiteral("results"), results}}}});
        return;
    }

    sendJson(socket, QJsonObject{{QStringLiteral("ok"), false},
                                 {QStringLiteral("error"), QStringLiteral("unsupported op")}});
}

void RemoteWebSocketServer::broadcastEvent(const QString& eventType, const QJsonObject& eventData) {
    const QJsonObject msg{
        {QStringLiteral("op"), Event},
        {QStringLiteral("d"),
         QJsonObject{{QStringLiteral("eventType"), eventType},
                     {QStringLiteral("eventIntent"), 0},
                     {QStringLiteral("eventData"), eventData}}},
    };
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        if (it.value().identified && it.key()) {
            sendJson(it.key(), msg);
        }
    }
}

void RemoteWebSocketServer::onSocketDisconnected() {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) {
        return;
    }
    clients_.remove(socket);
    socket->deleteLater();
}

} // namespace railshot
