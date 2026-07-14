#include "network/LocalWebServer.h"

#include "core/Logger.h"
#include "core/RemoteCommandBus.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QTcpSocket>

namespace railshot {

LocalWebServer::LocalWebServer(QObject* parent)
    : QObject(parent) {}

LocalWebServer::~LocalWebServer() {
    stop();
}

bool LocalWebServer::start(quint16 port) {
    if (server_.isListening()) {
        return true;
    }
    port_ = port;
    if (!server_.listen(QHostAddress::Any, port_)) {
        Logger::error("LocalWebServer: failed to listen on port " + std::to_string(port_));
        return false;
    }
    connect(&server_, &QTcpServer::newConnection, this, &LocalWebServer::onNewConnection);
    Logger::info("LocalWebServer: remote control at " + baseUrl().toStdString());
    return true;
}

void LocalWebServer::stop() {
    if (server_.isListening()) {
        server_.close();
    }
}

bool LocalWebServer::isRunning() const {
    return server_.isListening();
}

QString LocalWebServer::baseUrl() const {
    QString host = QHostAddress(QHostAddress::LocalHost).toString();
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& addr : addresses) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            host = addr.toString();
            break;
        }
    }
    return QStringLiteral("http://%1:%2").arg(host).arg(port_);
}

void LocalWebServer::onNewConnection() {
    while (QTcpSocket* socket = server_.nextPendingConnection()) {
        handleClient(socket);
    }
}

QByteArray LocalWebServer::jsonResponse(const QJsonObject& obj, int status) {
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    const QByteArray statusText = status == 200 ? "OK" : "Error";
    return "HTTP/1.1 " + QByteArray::number(status) + " " + statusText + "\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
           "Connection: close\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
           "Access-Control-Allow-Headers: Content-Type\r\n"
           "\r\n" + body;
}

QByteArray LocalWebServer::textResponse(const QByteArray& body, const char* contentType, int status) {
    const QByteArray statusText = status == 200 ? "OK" : "Error";
    return "HTTP/1.1 " + QByteArray::number(status) + " " + statusText + "\r\n"
           "Content-Type: " + contentType + "\r\n"
           "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
           "Connection: close\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "\r\n" + body;
}

QByteArray LocalWebServer::remoteControlHtml() const {
    return R"html(<!DOCTYPE html>
<html><head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>RailShot Remote</title>
<style>
  body { font-family: Bahnschrift, Segoe UI, sans-serif; background:#0b0d0c; color:#e8ebe6; margin:0; padding:16px; }
  h1 { font-size:1.15rem; color:#c5d0c8; letter-spacing:0.04em; }
  h2 { font-size:0.8rem; text-transform:uppercase; letter-spacing:0.12em; color:#8a968c; margin:18px 0 8px; }
  .score { display:flex; gap:12px; margin:12px 0; }
  .player { flex:1; background:#121714; border:1px solid #1c2420; border-radius:10px; padding:14px; text-align:center; }
  .player.active { border-color:#2f9e62; }
  .name { font-size:0.95rem; color:#8a968c; }
  .pts { font-size:2.4rem; font-weight:700; margin:8px 0; }
  button { width:100%; padding:14px; margin:5px 0; font-size:1.05rem; border:none; border-radius:8px;
    background:#2f9e62; color:#0b0d0c; font-weight:700; }
  button.secondary { background:#1a211c; color:#e8ebe6; border:1px solid #2a342e; }
  button.minus { background:#1a211c; color:#e8ebe6; border:1px solid #2a342e; }
  select { width:100%; padding:12px; margin:5px 0 10px; border-radius:8px; background:#121714; color:#e8ebe6;
    border:1px solid #2a342e; font-size:1rem; }
  #scenes, #previewScenes { display:flex; flex-direction:column; gap:6px; }
  #status { color:#8a968c; font-size:0.85rem; margin-top:14px; }
  .hidden { display:none; }
</style></head><body>
<h1>RailShot Remote</h1>
<h2>Collection</h2>
<select id="collections" onchange="switchCollection(this.value)"></select>
<h2>Score</h2>
<div class="score">
  <div class="player" id="p1box"><div class="name" id="p1name">Player 1</div><div class="pts" id="p1">0</div>
    <button onclick="adjust(1,1)">+1</button><button class="minus" onclick="adjust(1,-1)">-1</button></div>
  <div class="player" id="p2box"><div class="name" id="p2name">Player 2</div><div class="pts" id="p2">0</div>
    <button onclick="adjust(2,1)">+1</button><button class="minus" onclick="adjust(2,-1)">-1</button></div>
</div>
<h2>Program Scenes</h2>
<div id="scenes"></div>
<div id="previewBlock" class="hidden">
  <h2>Preview Scenes</h2>
  <div id="previewScenes"></div>
</div>
<h2>Controls</h2>
<button class="secondary" id="studioBtn" onclick="toggleStudio()">Studio Mode</button>
<button class="secondary" onclick="post('/api/transition')">Transition</button>
<button onclick="post('/api/stream/start')">Start Stream</button>
<button class="secondary" onclick="post('/api/stream/stop')">Stop Stream</button>
<button class="secondary" onclick="post('/api/record/start')">Start Record</button>
<button class="secondary" onclick="post('/api/record/stop')">Stop Record</button>
<div id="status">Connecting…</div>
<script>
async function refresh(){
  const [score, scenes, status, collections] = await Promise.all([
    fetch('/api/score').then(r=>r.json()),
    fetch('/api/scenes').then(r=>r.json()),
    fetch('/api/status').then(r=>r.json()),
    fetch('/api/collections').then(r=>r.json())
  ]);
  document.getElementById('p1name').textContent=score.player1Name;
  document.getElementById('p2name').textContent=score.player2Name;
  document.getElementById('p1').textContent=score.player1Score;
  document.getElementById('p2').textContent=score.player2Score;
  document.getElementById('p1box').classList.toggle('active', score.activePlayer===1);
  document.getElementById('p2box').classList.toggle('active', score.activePlayer===2);

  const colSel=document.getElementById('collections');
  const prev=colSel.value;
  colSel.innerHTML='';
  (collections.collections||[]).forEach(c=>{
    const o=document.createElement('option');
    o.value=c.id; o.textContent=c.name;
    if(c.id===collections.activeId) o.selected=true;
    colSel.appendChild(o);
  });
  if(prev && [...colSel.options].some(o=>o.value===prev)) colSel.value=prev;

  const host=document.getElementById('scenes'); host.innerHTML='';
  (scenes.scenes||[]).forEach(s=>{
    const b=document.createElement('button');
    b.className = s.id===scenes.activeSceneId ? '' : 'secondary';
    b.textContent = s.name + (s.id===scenes.activeSceneId?' (Program)':'');
    b.onclick=()=>post('/api/scenes/active',{sceneId:s.id});
    host.appendChild(b);
  });

  const studioOn=!!status.studioMode;
  document.getElementById('previewBlock').classList.toggle('hidden', !studioOn);
  document.getElementById('studioBtn').textContent = studioOn ? 'Exit Studio Mode' : 'Studio Mode';
  const ph=document.getElementById('previewScenes'); ph.innerHTML='';
  if(studioOn){
    (scenes.scenes||[]).forEach(s=>{
      const b=document.createElement('button');
      b.className = s.id===scenes.previewSceneId ? '' : 'secondary';
      b.textContent = s.name + (s.id===scenes.previewSceneId?' (Preview)':'');
      b.onclick=()=>post('/api/scenes/preview',{sceneId:s.id});
      ph.appendChild(b);
    });
  }

  document.getElementById('status').textContent =
    (status.streaming?'LIVE':'Idle') + (status.recording?' · REC':'') +
    (status.studioMode?' · Studio':'') +
    (status.collectionName?(' · '+status.collectionName):'');
}
async function adjust(player,delta){
  await fetch('/api/score/adjust',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({player,delta})});
  refresh();
}
async function post(path, body){
  await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(body||{})});
  refresh();
}
async function toggleStudio(){
  const status=await fetch('/api/status').then(r=>r.json());
  await post('/api/studio',{enabled:!status.studioMode});
}
async function switchCollection(id){
  if(!id) return;
  await post('/api/collections/switch',{id});
}
refresh(); setInterval(refresh,2000);
</script></body></html>)html";
}

void LocalWebServer::handleClient(QTcpSocket* socket) {
    if (!socket->waitForReadyRead(3000)) {
        socket->deleteLater();
        return;
    }

    const QByteArray request = socket->readAll();
    const QList<QByteArray> lines = request.split('\n');
    if (lines.isEmpty()) {
        socket->deleteLater();
        return;
    }

    const QList<QByteArray> parts = lines[0].trimmed().split(' ');
    const QString method = parts.size() > 0 ? QString::fromLatin1(parts[0]) : QString();
    const QString path = parts.size() > 1 ? QString::fromLatin1(parts[1]) : QString();

    auto& bus = RemoteCommandBus::instance();
    QByteArray response;

    auto parseBody = [&]() {
        QJsonObject payload;
        const int bodyIndex = request.indexOf("\r\n\r\n");
        if (bodyIndex >= 0) {
            payload = QJsonDocument::fromJson(request.mid(bodyIndex + 4)).object();
        }
        return payload;
    };

    if (method == QLatin1String("OPTIONS")) {
        response = textResponse({}, "text/plain", 200);
    } else if (method == QLatin1String("GET") && (path == "/" || path == "/index.html")) {
        response = textResponse(remoteControlHtml(), "text/html; charset=utf-8", 200);
    } else if (method == QLatin1String("GET") && path == "/api/score") {
        response = jsonResponse(bus.execute(QStringLiteral("getScore")));
    } else if (method == QLatin1String("GET") && path == "/api/status") {
        response = jsonResponse(bus.execute(QStringLiteral("getStatus")));
    } else if (method == QLatin1String("GET") && path == "/api/scenes") {
        response = jsonResponse(bus.execute(QStringLiteral("listScenes")));
    } else if (method == QLatin1String("GET") && path == "/api/collections") {
        response = jsonResponse(bus.execute(QStringLiteral("listCollections")));
    } else if (method == QLatin1String("GET") && path == "/api/version") {
        response = jsonResponse(bus.execute(QStringLiteral("getVersion")));
    } else if (method == QLatin1String("POST") && path == "/api/score/adjust") {
        response = jsonResponse(bus.execute(QStringLiteral("adjustScore"), parseBody()));
    } else if (method == QLatin1String("POST") && path == "/api/scenes/active") {
        response = jsonResponse(bus.execute(QStringLiteral("setActiveScene"), parseBody()));
    } else if (method == QLatin1String("POST") && path == "/api/scenes/preview") {
        response = jsonResponse(bus.execute(QStringLiteral("setPreviewScene"), parseBody()));
    } else if (method == QLatin1String("POST") && path == "/api/studio") {
        response = jsonResponse(bus.execute(QStringLiteral("setStudioMode"), parseBody()));
    } else if (method == QLatin1String("POST") && path == "/api/collections/switch") {
        response = jsonResponse(bus.execute(QStringLiteral("switchCollection"), parseBody()));
    } else if (method == QLatin1String("POST") && path == "/api/collections/create") {
        response = jsonResponse(bus.execute(QStringLiteral("createCollection"), parseBody()));
    } else if (method == QLatin1String("POST") && path == "/api/transition") {
        response = jsonResponse(bus.execute(QStringLiteral("studioTransition")));
    } else if (method == QLatin1String("POST") && path == "/api/stream/start") {
        response = jsonResponse(bus.execute(QStringLiteral("startStream"), parseBody()));
    } else if (method == QLatin1String("POST") && path == "/api/stream/stop") {
        response = jsonResponse(bus.execute(QStringLiteral("stopStream")));
    } else if (method == QLatin1String("POST") && path == "/api/record/start") {
        response = jsonResponse(bus.execute(QStringLiteral("startRecording")));
    } else if (method == QLatin1String("POST") && path == "/api/record/stop") {
        response = jsonResponse(bus.execute(QStringLiteral("stopRecording")));
    } else {
        response = textResponse("Not Found", "text/plain", 404);
    }

    socket->write(response);
    socket->flush();
    socket->waitForBytesWritten(2000);
    socket->deleteLater();
}

} // namespace railshot
