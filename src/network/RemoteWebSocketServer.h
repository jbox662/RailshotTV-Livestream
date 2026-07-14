#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QWebSocketServer;
class QWebSocket;

namespace railshot {

class RemoteWebSocketServer : public QObject {
    Q_OBJECT
public:
    explicit RemoteWebSocketServer(QObject* parent = nullptr);
    ~RemoteWebSocketServer() override;

    bool start(quint16 port = 4455);
    void stop();

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] QString baseUrl() const;
    [[nodiscard]] quint16 port() const { return port_; }

    // Broadcast an obs-websocket Event (op 5) to identified clients.
    void broadcastEvent(const QString& eventType, const QJsonObject& eventData = {});

private slots:
    void onNewConnection();
    void onTextMessage(const QString& message);
    void onSocketDisconnected();

private:
    struct ClientState {
        bool identified = false;
        QString challenge;
        QString salt;
    };

    void sendJson(QWebSocket* socket, const QJsonObject& obj);
    void sendHello(QWebSocket* socket);
    QJsonObject handleIdentify(QWebSocket* socket, const QJsonObject& data);
    QJsonObject handleObsRequest(const QJsonObject& data);
    QJsonObject handleLegacy(const QJsonObject& root);
    [[nodiscard]] bool verifyAuth(const QString& authentication, const ClientState& state) const;
    [[nodiscard]] static QString makeSalt();
    [[nodiscard]] static QString makeChallenge();
    [[nodiscard]] static QString sha256Base64(const QByteArray& input);

    QWebSocketServer* server_ = nullptr;
    quint16 port_ = 4455;
    QHash<QWebSocket*, ClientState> clients_;
};

} // namespace railshot
