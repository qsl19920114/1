#pragma once
#include <QTreeWidget>
namespace qvw::ui {
class AssetTree:public QTreeWidget {
 Q_OBJECT
public:
 explicit AssetTree(QWidget *parent=nullptr);
signals:
 void filesDropped(const QStringList &paths);
protected:
 void dragEnterEvent(QDragEnterEvent *) override;
 void dragMoveEvent(QDragMoveEvent *) override;
 void dropEvent(QDropEvent *) override;
};
}
