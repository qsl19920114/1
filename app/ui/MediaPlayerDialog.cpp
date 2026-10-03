#include "ui/MediaPlayerDialog.h"
#include "ui/MediaPlayerWidget.h"
#include <QCloseEvent>
#include <QKeyEvent>
#include <QDialogButtonBox>
#include <QShortcut>
#include <QVBoxLayout>
namespace qvw::ui {
MediaPlayerDialog::MediaPlayerDialog(const QString &path, const QString &title, QWidget *parent) : QDialog(parent) {
    setWindowTitle(title); resize(960, 640); setAttribute(Qt::WA_DeleteOnClose);
    auto *layout = new QVBoxLayout(this);
    m_player = new MediaPlayerWidget(path, this); layout->addWidget(m_player, 1);
    connect(m_player, &MediaPlayerWidget::fullscreenRequested, this, &MediaPlayerDialog::toggleFullscreen);
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WindowShortcut);
    connect(escape, &QShortcut::activated, this, [this] {
        if (isFullScreen()) toggleFullscreen();
        else reject();
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
}
MediaPlayerDialog::~MediaPlayerDialog() = default;
void MediaPlayerDialog::toggleFullscreen() {
    if (isFullScreen()) setWindowState(m_previousState);
    else { m_previousState = windowState(); showFullScreen(); }
}
void MediaPlayerDialog::reject() {
    m_player->shutdown(); QDialog::reject();
}
void MediaPlayerDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && isFullScreen()) { toggleFullscreen(); event->accept(); return; }
    QDialog::keyPressEvent(event);
}
void MediaPlayerDialog::closeEvent(QCloseEvent *event) { m_player->shutdown(); QDialog::closeEvent(event); }
}
