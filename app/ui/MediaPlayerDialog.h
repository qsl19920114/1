#pragma once
#include <QDialog>
class QWebEngineView;
class QWebEngineProfile;
namespace qvw::ui {
class MediaPlayerDialog:public QDialog {
public:
    MediaPlayerDialog(const QString &path,const QString &title,QWidget *parent=nullptr);
    ~MediaPlayerDialog() override;
private:
    QWebEngineView *m_view;
    QWebEngineProfile *m_profile;
};
}
