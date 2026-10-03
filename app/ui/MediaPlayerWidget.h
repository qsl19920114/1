#pragma once
#include <QWidget>
#include <QElapsedTimer>

class QWebEngineView;
class QWebEngineProfile;
class QTimer;
class QPushButton;
class QSlider;
class QLabel;
class QComboBox;
class QCheckBox;

namespace qvw::ui {
class MediaPlayerWidget : public QWidget {
    Q_OBJECT
public:
    struct PlaybackState {
        bool ready = false, paused = true, ended = false, muted = false, loop = false;
        double position = 0, duration = 0, volume = 1, rate = 1;
        QString error;
    };
    explicit MediaPlayerWidget(const QString &path, QWidget *parent = nullptr);
    ~MediaPlayerWidget() override;
    PlaybackState playbackState() const { return m_state; }
public slots:
    void playPause();
    void stop();
    void seek(double seconds);
    void setPlaybackRate(double rate);
    void setVolume(int percent);
    void setMuted(bool muted);
    void setLoop(bool loop);
    void shutdown();
signals:
    void playbackStateChanged();
    void fullscreenRequested();
private:
    void poll();
    void command(const QString &script);
    void updateControls();
    void fail(const QString &message);
    QWebEngineView *m_view;
    QWebEngineProfile *m_profile;
    QTimer *m_timer;
    QPushButton *m_play, *m_stop, *m_back, *m_forward, *m_mute, *m_fullscreen;
    QSlider *m_seek, *m_volume;
    QLabel *m_time, *m_status;
    QComboBox *m_rate;
    QCheckBox *m_loop;
    PlaybackState m_state;
    QElapsedTimer m_loading, m_pollAge;
    quint64 m_revision = 0;
    bool m_hasDecoded = false, m_pollPending = false, m_closed = false, m_failed = false, m_updating = false;
};
}
