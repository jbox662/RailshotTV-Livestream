#include "ui/MainWindow.h"

#include <QApplication>
#include <QSurfaceFormat>

#ifndef RAILSHOT_VERSION
#define RAILSHOT_VERSION "0.0.0"
#endif

namespace {

// RailShot TV identity — billiards / broadcast.
const char* kAppStyleSheet = R"(
* {
    outline: none;
}

QMainWindow {
    background-color: #0b0d0c;
    color: #e8ebe6;
    font-family: "Bahnschrift", "Segoe UI Variable", "Segoe UI", sans-serif;
    font-size: 12px;
}

QMenuBar {
    background-color: #0f1311;
    color: #c5d0c8;
    border-bottom: 1px solid #1c2420;
    padding: 2px 4px;
    spacing: 2px;
}

QMenuBar::item {
    background: transparent;
    padding: 4px 10px;
    border-radius: 3px;
}

QMenuBar::item:selected,
QMenuBar::item:pressed {
    background-color: #1e4d34;
    color: #ffffff;
}

QMenu {
    background-color: #121714;
    color: #e8ebe6;
    border: 1px solid #2a342e;
    padding: 4px;
}

QMenu::item {
    padding: 6px 28px 6px 12px;
    border-radius: 3px;
}

QMenu::item:selected {
    background-color: #1e4d34;
}

QMenu::separator {
    height: 1px;
    background: #1c2420;
    margin: 4px 8px;
}

QWidget {
    background-color: transparent;
    color: #e8ebe6;
    font-family: "Bahnschrift", "Segoe UI Variable", "Segoe UI", sans-serif;
    font-size: 12px;
}

QDialog {
    background-color: #0f1311;
}

QTabWidget::pane {
    background-color: #0f1311;
    border: 1px solid #2a342e;
    border-radius: 4px;
}

QTabBar::tab {
    background-color: #121714;
    color: #9aa69d;
    border: 1px solid #2a342e;
    padding: 7px 14px;
}

QTabBar::tab:selected {
    background-color: #1e4d34;
    color: #ffffff;
    border-color: #2f9e62;
}

QToolTip {
    background-color: #18211c;
    color: #e8ebe6;
    border: 1px solid #2f9e62;
    padding: 4px 6px;
}

QFrame#rsDockPanel {
    background-color: #121714;
    border: none;
    border-top: 1px solid #1c2420;
}

QFrame#rsDockAccent {
    background-color: #2f9e62;
    border: none;
    border-radius: 1px;
}

QWidget#rsDockHeader {
    background-color: #121714;
    border-bottom: 1px solid #1a211c;
}

QLabel#rsDockTitle {
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 1.2px;
    text-transform: uppercase;
    color: #c5d0c8;
}

QWidget#rsDockBody {
    background-color: #0f1311;
}

QWidget#rsDockFooter {
    background-color: #121714;
    border-top: 1px solid #1a211c;
}

QToolButton#rsDockToolButton {
    background-color: #1a211c;
    border: 1px solid #2a342e;
    border-radius: 4px;
    color: #d7e0d9;
    font-size: 14px;
    font-weight: 700;
    padding: 0;
}

QToolButton#rsDockToolButton:hover {
    background-color: #243028;
    border-color: #2f9e62;
    color: #ffffff;
}

QToolButton#rsDockToolButton:pressed {
    background-color: #16201a;
}

QWidget#rsContextBar {
    background-color: #0e1210;
    border-top: 1px solid #1c2420;
    border-bottom: 1px solid #1c2420;
    min-height: 36px;
}

QLabel#rsContextLabel {
    color: #8a968c;
    font-size: 12px;
    padding-left: 12px;
}

QPushButton#rsContextButton {
    background-color: #1a211c;
    border: 1px solid #2f9e62;
    border-radius: 4px;
    color: #e8ebe6;
    font-weight: 700;
    letter-spacing: 0.4px;
    padding: 5px 14px;
    min-height: 24px;
}

QPushButton#rsContextButton:hover {
    background-color: #243028;
}

QPushButton#rsContextButton:disabled {
    color: #5a655e;
    border-color: #2a342e;
    background-color: #141a17;
}

QFrame#rsPreviewFrame {
    background-color: #050606;
    border: 1px solid #1c2420;
}

QLabel#rsPreviewLabel {
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 1px;
    text-transform: uppercase;
    color: #8a968c;
    padding-left: 6px;
}

QLabel#rsPreviewMeta {
    font-size: 11px;
    color: #6f7a72;
}

QComboBox#rsPreviewScale {
    background-color: #121714;
    border: 1px solid #2a342e;
    border-radius: 4px;
    padding: 2px 8px;
    min-height: 22px;
}

QPushButton#rsTransitionButton {
    background-color: #1a211c;
    border: 1px solid #c9a227;
    border-radius: 6px;
    color: #f3e5b0;
    font-weight: 700;
    letter-spacing: 0.8px;
    padding: 12px 16px;
}

QPushButton#rsTransitionButton:hover {
    background-color: #243028;
    color: #ffe9a0;
}

QPushButton#rsControlButton {
    background-color: #1a211c;
    border: 1px solid #2a342e;
    border-radius: 4px;
    color: #e8ebe6;
    font-weight: 700;
    letter-spacing: 0.4px;
    padding: 4px 10px;
    min-height: 0;
}

QPushButton#rsControlButton:hover {
    border-color: #2f9e62;
    background-color: #243028;
}

QPushButton#rsControlButton:checked {
    background-color: #1e4d34;
    border-color: #2f9e62;
    color: #d8ffe8;
}

QPushButton#rsControlButtonLive {
    background-color: #5a1c1c;
    border: 1px solid #c04545;
    border-radius: 4px;
    color: #ffffff;
    font-weight: 700;
    letter-spacing: 0.4px;
    padding: 4px 10px;
    min-height: 0;
}

QPushButton#rsControlButtonLive:hover {
    background-color: #702222;
}

QListWidget {
    background-color: #0c100e;
    border: none;
    padding: 4px;
}

QListWidget::item {
    padding: 0;
    border: none;
    border-radius: 4px;
    min-height: 30px;
    margin: 1px 2px;
}

QListWidget::item:selected {
    background-color: #1e4d34;
    color: #ffffff;
}

QListWidget::item:hover:!selected {
    background-color: #18211c;
}

QWidget#rsSourceRow {
    background: transparent;
}

QLabel#rsSourceName {
    color: #e0e6e1;
    font-size: 12px;
    padding-left: 4px;
}

QToolButton#rsSourceEye, QToolButton#rsSourceLock {
    background: transparent;
    border: none;
    color: #9aa69d;
    font-size: 12px;
    padding: 0;
    min-width: 22px;
    max-width: 22px;
}

QToolButton#rsSourceEye:hover, QToolButton#rsSourceLock:hover {
    color: #2f9e62;
}

QToolButton#rsSourceEye:checked, QToolButton#rsSourceLock:checked {
    color: #c9a227;
}

QFrame#rsMixerStrip {
    background-color: #121714;
    border: none;
    border-right: 1px solid #1c2420;
}

QLabel#rsMixerName {
    font-size: 11px;
    color: #c5d0c8;
    font-weight: 700;
    letter-spacing: 0.6px;
    text-transform: uppercase;
}

QProgressBar#rsVolumeMeter {
    background-color: #080a09;
    border: 1px solid #1c2420;
    border-radius: 2px;
    min-height: 10px;
    max-height: 10px;
}

QProgressBar#rsVolumeMeter::chunk {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #2f9e62, stop:0.7 #c9a227, stop:1 #c04545);
}

QSlider#rsVolumeSlider::groove:horizontal {
    height: 4px;
    background: #1c2420;
    border-radius: 2px;
}

QSlider#rsVolumeSlider::handle:horizontal {
    background: #e8ebe6;
    border: 1px solid #2f9e62;
    width: 12px;
    height: 12px;
    margin: -5px 0;
    border-radius: 6px;
}

QSlider#rsVolumeSlider::sub-page:horizontal {
    background: #2f9e62;
    border-radius: 2px;
}

QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit, QPlainTextEdit {
    background-color: #0c100e;
    border: 1px solid #2a342e;
    border-radius: 4px;
    padding: 5px 8px;
    color: #e8ebe6;
    min-height: 22px;
    selection-background-color: #1e4d34;
}

QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus, QPlainTextEdit:focus {
    border-color: #2f9e62;
}

QComboBox::drop-down, QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    border: none;
    background: #1a211c;
    width: 18px;
}

QLabel#rsFieldLabel {
    color: #8a968c;
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 1px;
    text-transform: uppercase;
}

QSplitter::handle {
    background-color: #0b0d0c;
    width: 2px;
}

QSplitter::handle:hover {
    background-color: #2f9e62;
}

QStatusBar {
    background-color: #080a09;
    color: #6f7a72;
    border-top: 1px solid #1c2420;
}

QPushButton {
    background-color: #1a211c;
    border: 1px solid #2a342e;
    border-radius: 4px;
    padding: 5px 10px;
    color: #e8ebe6;
}

QPushButton:hover {
    border-color: #2f9e62;
}

QCheckBox#rsMuteToggle {
    spacing: 6px;
    color: #c5d0c8;
}

QCheckBox#rsMuteToggle::indicator {
    width: 14px;
    height: 14px;
    border: 1px solid #2a342e;
    border-radius: 3px;
    background: #0c100e;
}

QCheckBox#rsMuteToggle::indicator:checked {
    background: #2f9e62;
    border-color: #3bb872;
}

QScrollArea {
    border: none;
    background: transparent;
}

QScrollBar:vertical, QScrollBar:horizontal {
    background: #0c100e;
    border: none;
    margin: 0;
}

QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background: #2a342e;
    border-radius: 4px;
    min-height: 24px;
    min-width: 24px;
}

QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
    background: #2f9e62;
}

QScrollBar::add-line, QScrollBar::sub-line {
    width: 0;
    height: 0;
}
)";

} // namespace

int main(int argc, char* argv[]) {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    app.setApplicationName("RailShot TV Broadcaster");
    app.setApplicationDisplayName("RailShot TV Broadcaster");
    app.setApplicationVersion(RAILSHOT_VERSION);
    app.setOrganizationName("RailShot TV");
    app.setOrganizationDomain("railshot.tv");
    app.setDesktopFileName("RailShotTV");
    app.setStyleSheet(kAppStyleSheet);

    railshot::MainWindow window;
    window.show();

    return app.exec();
}
