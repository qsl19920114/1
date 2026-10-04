#pragma once
#include "domain/Snapshot.h"
#include <QWidget>
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
namespace qvw::ui {
class ComponentBrowser : public QWidget {
    Q_OBJECT
public:
    explicit ComponentBrowser(QWidget *parent=nullptr);
    QTreeWidget *tree() const { return m_tree; }
    void setSnapshot(const domain::Snapshot &snapshot);
    void resetFilters();
    bool selectEntity(const QString &entityId,bool reveal=true);
    QString selectedEntityId() const;
signals:
    void componentActivated(const QString &entityId,int startFrame);
protected:
    bool eventFilter(QObject *watched,QEvent *event) override;
private:
    void filterComponents();
    void activate(QTreeWidgetItem *item);
    QList<QTreeWidgetItem*> visibleComponents() const;
    domain::Snapshot m_snapshot;
    QTreeWidget *m_tree;
    QLineEdit *m_search;
    QComboBox *m_filter;
    QLabel *m_count;
    QLabel *m_empty;
    QPushButton *m_reset;
};
}
