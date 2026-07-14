#pragma once

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

private slots:
    void onNewConnection();
    void onTextMessage(const QString& message);
    void onSocketDisconnected();

private:
    QWebSocketServer* server_ = nullptr;
    quint16 port_ = 4455;
};

} // namespace railshot
