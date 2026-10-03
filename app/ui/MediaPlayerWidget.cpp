#include "ui/MediaPlayerWidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <algorithm>
#include <cmath>

namespace qvw::ui {
namespace {
QString clockText(double seconds) {
    const auto total = static_cast<qint64>(std::max(0.0, seconds));
    if (total >= 3600) return QString("%1:%2:%3").arg(total / 3600).arg(total / 60 % 60, 2, 10, QLatin1Char('0')).arg(total % 60, 2, 10, QLatin1Char('0'));
    return QString("%1:%2").arg(total / 60, 2, 10, QLatin1Char('0')).arg(total % 60, 2, 10, QLatin1Char('0'));
}
}
MediaPlayerWidget::MediaPlayerWidget(const QString &path, QWidget *parent) : QWidget(parent) {
    setObjectName("mediaPlayer");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_profile = new QWebEngineProfile(this);
    m_view = new QWebEngineView(this); m_view->setObjectName("mediaView");
    m_view->setPage(new QWebEnginePage(m_profile, m_view));
    m_view->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    m_view->setContextMenuPolicy(Qt::NoContextMenu);
    layout->addWidget(m_view, 1);
    auto *timeline = new QHBoxLayout;
    m_seek = new QSlider(Qt::Horizontal, this); m_seek->setObjectName("mediaSeek");
    m_seek->setRange(0, 1000); m_seek->setAccessibleName(tr("播放进度"));
    m_time = new QLabel(this); m_time->setObjectName("mediaTime");
    timeline->addWidget(m_seek, 1); timeline->addWidget(m_time); layout->addLayout(timeline);
    auto *controls = new QHBoxLayout;
    const auto button = [this, controls](const QString &name, const QString &text) {
        auto *b = new QPushButton(text, this); b->setObjectName(name); b->setAutoDefault(false); controls->addWidget(b); return b;
    };
    m_back = button("mediaBack", tr("−5 秒"));
    m_play = button("mediaPlayPause", tr("播放"));
    m_stop = button("mediaStop", tr("停止"));
    m_forward = button("mediaForward", tr("+5 秒"));
    m_mute = button("mediaMute", tr("静音")); m_mute->setCheckable(true);
    m_volume = new QSlider(Qt::Horizontal, this); m_volume->setObjectName("mediaVolume");
    m_volume->setRange(0, 100); m_volume->setMaximumWidth(100); m_volume->setAccessibleName(tr("音量")); controls->addWidget(m_volume);
    m_rate = new QComboBox(this); m_rate->setObjectName("mediaRate"); m_rate->setAccessibleName(tr("播放速度"));
    for (double rate : {.5, 1., 1.5, 2.}) m_rate->addItem(QString::number(rate) + "×", rate);
    controls->addWidget(m_rate);
    m_loop = new QCheckBox(tr("循环"), this); m_loop->setObjectName("mediaLoop"); controls->addWidget(m_loop);
    controls->addStretch(); m_fullscreen = button("mediaFullscreen", tr("全屏"));
    layout->addLayout(controls);
    m_status = new QLabel(this); m_status->setObjectName("mediaStatus"); m_status->setWordWrap(true); layout->addWidget(m_status);
    connect(m_play, &QPushButton::clicked, this, &MediaPlayerWidget::playPause);
    connect(m_stop, &QPushButton::clicked, this, &MediaPlayerWidget::stop);
    connect(m_back, &QPushButton::clicked, this, [this] { seek(m_state.position - 5); });
    connect(m_forward, &QPushButton::clicked, this, [this] { seek(m_state.position + 5); });
    connect(m_mute, &QPushButton::clicked, this, &MediaPlayerWidget::setMuted);
    connect(m_volume, &QSlider::valueChanged, this, [this](int value) { if (!m_updating) setVolume(value); });
    connect(m_rate, &QComboBox::currentIndexChanged, this, [this](int) { if (!m_updating) setPlaybackRate(m_rate->currentData().toDouble()); });
    connect(m_loop, &QCheckBox::toggled, this, [this](bool checked) { if (!m_updating) setLoop(checked); });
    connect(m_fullscreen, &QPushButton::clicked, this, &MediaPlayerWidget::fullscreenRequested);
    connect(m_seek, &QSlider::sliderReleased, this, [this] { seek(m_state.duration * m_seek->value() / 1000); });
    connect(m_seek, &QSlider::valueChanged, this, [this](int value) { if (!m_updating && !m_seek->isSliderDown()) seek(m_state.duration * value / 1000); });
    m_timer = new QTimer(this); m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &MediaPlayerWidget::poll);
    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) { if (!m_closed) { if (!ok) fail(tr("无法加载播放器页面")); else poll(); } });
    connect(m_view->page(), &QWebEnginePage::renderProcessTerminated, this, [this] { if (!m_closed) fail(tr("视频播放进程已退出，请关闭后重新打开")); });
    updateControls();
    const auto url = QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath());
    // Only an HTML-escaped, fully encoded local URL enters markup. Commands never interpolate paths.
    const auto html = QStringLiteral("<!doctype html><html><head><meta charset='utf-8'><style>html,body{margin:0;background:#0b1018;width:100%;height:100%;overflow:hidden}video{width:100%;height:100%;object-fit:contain}</style></head><body><video preload='auto' playsinline src=\"%1\"></video></body></html>").arg(url.toString(QUrl::FullyEncoded).toHtmlEscaped());
    m_loading.start(); m_view->setHtml(html, url); m_timer->start();
}
MediaPlayerWidget::~MediaPlayerWidget() {
    shutdown();
    // Page must die before its off-the-record profile; callbacks use QPointer and m_closed.
    delete m_view; delete m_profile;
}
void MediaPlayerWidget::shutdown() {
    if (m_closed) return;
    m_closed = true; ++m_revision; m_timer->stop();
    m_view->page()->runJavaScript(QStringLiteral("(()=>{const v=document.querySelector('video');if(v){v.pause();v.removeAttribute('src');v.load();}})()"));
    m_state.ready = false; m_state.paused = true; updateControls();
}
void MediaPlayerWidget::fail(const QString &message) {
    ++m_revision; m_failed = true; m_state.ready = false; m_state.error = message; m_timer->stop(); updateControls(); emit playbackStateChanged();
}
void MediaPlayerWidget::poll() {
    if (m_closed || m_failed) return;
    if (m_pollPending) {
        if (m_pollAge.elapsed() > 5000) fail(tr("播放器没有响应，请关闭后重新打开"));
        return;
    }
    if (!m_state.ready && m_loading.elapsed() > 15000) { fail(tr("无法读取视频，请检查文件是否可播放")); return; }
    m_pollPending = true; m_pollAge.start();
    const quint64 revision = m_revision;
    QPointer<MediaPlayerWidget> guard(this);
    m_view->page()->runJavaScript(QStringLiteral(R"JS((()=>{
        const v=document.querySelector('video');if(!v)return '';
        return JSON.stringify({ready:v.readyState>=2&&v.videoWidth>0&&Number.isFinite(v.duration)&&v.duration>0,
            paused:v.paused,ended:v.ended,position:v.currentTime,duration:Number.isFinite(v.duration)?v.duration:0,
            volume:v.volume,muted:v.muted,rate:v.playbackRate,loop:v.loop,error:v.error?v.error.code:0,playError:window.__playError||''});
    })())JS"), [guard, revision](const QVariant &result) {
        if (!guard || guard->m_closed || guard->m_failed) return;
        guard->m_pollPending = false;
        if (revision != guard->m_revision) return;
        const auto object = QJsonDocument::fromJson(result.toString().toUtf8()).object();
        if (object.isEmpty()) return;
        const int error = object.value("error").toInt();
        if (error) { guard->fail(tr("无法播放视频（媒体错误 %1）").arg(error)); return; }
        auto &state = guard->m_state;
        // Once a real frame has decoded, seeking/buffering must not disable Stop.
        // A media error or closed page revokes readiness separately.
        guard->m_hasDecoded |= object.value("ready").toBool();
        state.ready = guard->m_hasDecoded && object.value("duration").toDouble() > 0; state.paused = object.value("paused").toBool(); state.ended = object.value("ended").toBool();
        state.position = object.value("position").toDouble(); state.duration = object.value("duration").toDouble();
        state.volume = object.value("volume").toDouble(); state.muted = object.value("muted").toBool();
        state.rate = object.value("rate").toDouble(); state.loop = object.value("loop").toBool();
        state.error = object.value("playError").toString();
        if (state.ready) guard->m_loading.restart();
        guard->updateControls(); emit guard->playbackStateChanged();
    });
}
void MediaPlayerWidget::command(const QString &script) {
    if (m_closed || !m_state.ready) return;
    ++m_revision;
    // script contains only program constants and finite clamped numeric values.
    m_view->page()->runJavaScript("(()=>{const v=document.querySelector('video');if(!v)return;window.__playError='';" + script + "})()");
}
void MediaPlayerWidget::playPause() {
    command(QStringLiteral("if(v.paused){v.play().catch(e=>{if(e.name!=='AbortError')window.__playError='无法开始播放：'+e.message;});}else{v.pause();}"));
}
void MediaPlayerWidget::stop() { command("v.pause();v.currentTime=0;"); }
void MediaPlayerWidget::seek(double seconds) {
    if (!std::isfinite(seconds) || !m_state.ready) return;
    command(QString("v.currentTime=%1;").arg(std::clamp(seconds, 0.0, m_state.duration), 0, 'g', 16));
}
void MediaPlayerWidget::setPlaybackRate(double rate) {
    if (rate != .5 && rate != 1 && rate != 1.5 && rate != 2) return;
    command(QString("v.playbackRate=%1;").arg(rate));
}
void MediaPlayerWidget::setVolume(int percent) { command(QString("v.volume=%1;").arg(std::clamp(percent, 0, 100) / 100.0)); }
void MediaPlayerWidget::setMuted(bool muted) { command(muted ? "v.muted=true;" : "v.muted=false;"); }
void MediaPlayerWidget::setLoop(bool loop) { command(loop ? "v.loop=true;" : "v.loop=false;"); }
void MediaPlayerWidget::updateControls() {
    m_updating = true;
    for (QWidget *control : std::initializer_list<QWidget*>{m_play, m_stop, m_back, m_forward, m_mute, m_fullscreen, m_seek, m_volume, m_rate, m_loop}) control->setEnabled(m_state.ready && !m_closed);
    m_play->setText(m_state.ended ? tr("重播") : m_state.paused ? tr("播放") : tr("暂停"));
    m_mute->setChecked(m_state.muted); m_mute->setText(m_state.muted ? tr("取消静音") : tr("静音"));
    m_volume->setValue(qRound(m_state.volume * 100));
    m_rate->setCurrentIndex(m_rate->findData(m_state.rate)); m_loop->setChecked(m_state.loop);
    if (!m_seek->isSliderDown()) m_seek->setValue(m_state.duration > 0 ? qRound(m_state.position / m_state.duration * 1000) : 0);
    m_time->setText(clockText(m_state.position) + " / " + clockText(m_state.duration));
    m_status->setText(!m_state.error.isEmpty() ? m_state.error : m_closed ? tr("已关闭") : !m_state.ready ? tr("正在加载视频…") : m_state.ended ? tr("播放结束") : m_state.paused ? tr("已暂停") : tr("正在播放"));
    m_updating = false;
}
}
