#pragma once

#include "core/StreamController.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>
#include <mutex>

namespace railshot {

/// Shared control surface for hotkeys, HTTP remote, and WebSocket remote.
class RemoteCommandBus : public QObject {
    Q_OBJECT
public:
    static RemoteCommandBus& instance();

    void attach(StreamController* controller,
                std::function<std::string()> rtmpUrlProvider,
                std::function<void()> uiRefresh);

    /// Execute op with optional JSON payload. Always returns a JSON object with "ok" bool.
    [[nodiscard]] QJsonObject execute(const QString& op, const QJsonObject& data = {});

signals:
    void commandExecuted(const QString& op, bool ok);

private:
    RemoteCommandBus() = default;

    QJsonObject statusJson() const;
    QJsonObject scenesJson() const;
    QJsonObject collectionsJson() const;
    QJsonObject scoreJson() const;

    mutable std::mutex mutex_;
    StreamController* controller_ = nullptr;
    std::function<std::string()> rtmpUrlProvider_;
    std::function<void()> uiRefresh_;
};

} // namespace railshot
