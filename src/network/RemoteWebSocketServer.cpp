#include "network/RemoteWebSocketServer.h"

#include "core/Logger.h"
#include "core/RemoteCommandBus.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QWebSocket>
#include <QWebSocketServer>

namespace railshot {

RemoteWebSocketServer::RemoteWebSocketServer(QObject* parent)
    : QObject(parent) {}

RemoteWebSocketServer::~RemoteWebSocketServer() {
    stop();
}

bool RemoteWebSocketServer::start(quint16 port) {
    if (server_ && server_->isListening()) {
        return true;
    }
    port_ = port;
    server_ = new QWebSocketServer(QStringLiteral("RailShot Remote"), QWebSocketServer::NonSecureMode, this);
    if (!server_->listen(QHostAddress::Any, port_)) {
        Logger::error("RemoteWebSocketServer: failed to listen on " + std::to_string(port_));
        delete server_;
        server_ = nullptr;
        return false;
    }
    connect(server_, &QWebSocketServer::newConnection, this, &RemoteWebSocketServer::onNewConnection);
    Logger::info("RemoteWebSocketServer: listening at " + baseUrl().toStdString());
    return true;
}

void RemoteWebSocketServer::stop() {
    if (!server_) {
        return;
    }
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

void RemoteWebSocketServer::onNewConnection() {
    while (server_ && server_->hasPendingConnections()) {
        QWebSocket* socket = server_->nextPendingConnection();
        connect(socket, &QWebSocket::textMessageReceived, this, &RemoteWebSocketServer::onTextMessage);
        connect(socket, &QWebSocket::disconnected, this, &RemoteWebSocketServer::onSocketDisconnected);
        QJsonObject hello;
        hello[QStringLiteral("ok")] = true;
        hello[QStringLiteral("op")] = QStringLiteral("hello");
        hello[QStringLiteral("d")] = QJsonObject{
            {QStringLiteral("service"), QStringLiteral("RailShot")},
            {QStringLiteral("version"), 2},
            {QStringLiteral("protocol"), 2},
        };
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(hello).toJson(QJsonDocument::Compact)));
    }
}

void RemoteWebSocketServer::onTextMessage(const QString& message) {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        QJsonObject err;
        err[QStringLiteral("ok")] = false;
        err[QStringLiteral("error")] = QStringLiteral("expected JSON object");
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact)));
        return;
    }
    const QJsonObject root = doc.object();
    const QString op = root.value(QStringLiteral("op")).toString();
    const QJsonObject data = root.value(QStringLiteral("d")).toObject();
    const QJsonObject result = RemoteCommandBus::instance().execute(op, data);
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));
}

void RemoteWebSocketServer::onSocketDisconnected() {
    auto* socket = qobject_cast<QWebSocket*>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

} // namespace railshot
