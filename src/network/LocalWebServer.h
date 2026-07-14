#pragma once

#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QJsonObject>

namespace railshot {

class LocalWebServer : public QObject {
    Q_OBJECT

public:
    explicit LocalWebServer(QObject* parent = nullptr);
    ~LocalWebServer() override;

    bool start(quint16 port = 8080);
    void stop();

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] QString baseUrl() const;
    [[nodiscard]] quint16 port() const { return port_; }

private slots:
    void onNewConnection();

private:
    void handleClient(QTcpSocket* socket);
    QByteArray remoteControlHtml() const;
    static QByteArray jsonResponse(const QJsonObject& obj, int status = 200);
    static QByteArray textResponse(const QByteArray& body, const char* contentType, int status);

    QTcpServer server_;
    quint16 port_ = 8080;
};

} // namespace railshot
