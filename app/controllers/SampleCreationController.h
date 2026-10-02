#pragma once
#include "controllers/DocumentController.h"
#include "controllers/EditorController.h"
namespace qvw::controllers {
// Coordinates asynchronous import with the actual Studio session identity.
class SampleCreationController:public QObject {
    Q_OBJECT
public:
    SampleCreationController(DocumentController &document,EditorController &editor,QObject *parent=nullptr);
    bool create(const QString &sample,const QString &templateDirectory,const QString &destination,const QString &name);
    void cancel();
signals:
    void message(const QString &text);
    void failed(const QString &text);
    void bound(const QString &relativePath);
private:
    void attemptBinding();
    DocumentController &m_document;
    EditorController &m_editor;
    QString m_root,m_assetPath;
    bool m_waitingForEdit=false;
};
}
