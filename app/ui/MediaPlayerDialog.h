#pragma once
#include <QDialog>
namespace qvw::ui {
class MediaPlayerWidget;
class MediaPlayerDialog : public QDialog {
public:
    MediaPlayerDialog(const QString &path, const QString &title, QWidget *parent = nullptr);
    ~MediaPlayerDialog() override;
    MediaPlayerWidget *player() const { return m_player; }
    void reject() override;
protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    void toggleFullscreen();
    MediaPlayerWidget *m_player;
    Qt::WindowStates m_previousState;
};
}
