#include "ui/MediaPlayerDialog.h"
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QUrl>
namespace qvw::ui {
MediaPlayerDialog::MediaPlayerDialog(const QString &path,const QString &title,QWidget *parent):QDialog(parent) {
    setWindowTitle(title);resize(960,640);setAttribute(Qt::WA_DeleteOnClose);
    auto *layout=new QVBoxLayout(this);m_profile=new QWebEngineProfile(this);
    m_view=new QWebEngineView(this);m_view->setPage(new QWebEnginePage(m_profile,m_view));layout->addWidget(m_view,1);
    const auto url=QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath());
    const auto html=QString("<!doctype html><html><body style='margin:0;background:#0b1018;display:grid;place-items:center;height:100vh'><video controls playsinline style='width:100%;height:100%;object-fit:contain' src=\"%1\"></video></body></html>").arg(url.toString(QUrl::FullyEncoded).toHtmlEscaped());
    m_view->setHtml(html,url);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::close);
}
MediaPlayerDialog::~MediaPlayerDialog(){delete m_view;delete m_profile;}
}
