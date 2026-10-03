#pragma once
#include "domain/Snapshot.h"
#include "domain/Project.h"
#include <QWidget>
class QListWidget;
class QLabel;
namespace qvw::ui {
class SceneStrip : public QWidget {
 Q_OBJECT
public:
 explicit SceneStrip(QWidget *parent=nullptr);
 void setSnapshot(const domain::Snapshot &,const domain::Project &);
 void setPosition(int frame);
 void setSelection(const QString &id);
signals:
 void sceneActivated(const QString &entityId,int frame);
private:
 QListWidget *m_list;
 QLabel *m_target;
};
}
