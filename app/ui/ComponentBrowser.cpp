#include "ui/ComponentBrowser.h"
#include <QComboBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace qvw::ui {
ComponentBrowser::ComponentBrowser(QWidget *parent):QWidget(parent) {
    setObjectName("componentBrowser");
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    m_search=new QLineEdit;m_search->setObjectName("componentSearch");m_search->setPlaceholderText("搜索组件、轨道或 ID");m_search->setAccessibleName("搜索组件、轨道或 ID");m_search->setClearButtonEnabled(true);layout->addWidget(m_search);
    auto *filters=new QHBoxLayout;
    m_filter=new QComboBox;m_filter->setObjectName("componentFilter");m_filter->setAccessibleName("组件可编辑性筛选");m_filter->addItem("全部组件","all");m_filter->addItem("可编辑组件","editable");filters->addWidget(m_filter);
    m_reset=new QPushButton("清除筛选");m_reset->setObjectName("clearComponentFilters");filters->addWidget(m_reset);layout->addLayout(filters);
    m_count=new QLabel;m_count->setObjectName("componentCount");m_count->setAccessibleName("可见组件数量");layout->addWidget(m_count);
    m_empty=new QLabel("没有匹配的组件，请调整搜索或清除筛选。");m_empty->setWordWrap(true);m_empty->setObjectName("componentEmpty");m_empty->setAccessibleName("组件搜索结果提示");layout->addWidget(m_empty);
    m_tree=new QTreeWidget;m_tree->setObjectName("components");m_tree->setAccessibleName("工程组件");m_tree->setHeaderLabels({"组件","帧范围"});m_tree->header()->setSectionResizeMode(0,QHeaderView::Stretch);layout->addWidget(m_tree,1);
    m_tree->installEventFilter(this);m_search->installEventFilter(this);
    connect(m_search,&QLineEdit::textChanged,this,&ComponentBrowser::filterComponents);
    connect(m_filter,&QComboBox::currentIndexChanged,this,&ComponentBrowser::filterComponents);
    connect(m_reset,&QPushButton::clicked,this,&ComponentBrowser::resetFilters);
    connect(m_tree,&QTreeWidget::itemClicked,this,[this](QTreeWidgetItem *item){activate(item);});
    filterComponents();
}
QString ComponentBrowser::selectedEntityId() const {
    const auto *item=m_tree->currentItem();
    return item&&item->parent()&&!item->isHidden()&&!item->parent()->isHidden()?item->toolTip(0):QString();
}
void ComponentBrowser::setSnapshot(const domain::Snapshot &snapshot) {
    const auto selectedId=selectedEntityId();
    const bool initial=!m_snapshot.isLoaded();
    m_snapshot=snapshot;
    // MainWindow refreshes its inspector after this transaction. Intermediate rows
    // must never be interpreted against a different snapshot's index roles.
    const QSignalBlocker block(m_tree);
    m_tree->clear();
    if(snapshot.isLoaded())for(int t=0;t<snapshot.tracks.size();++t){
        const auto &track=snapshot.tracks[t];auto *parent=new QTreeWidgetItem(m_tree);
        parent->setText(0,track.label.isEmpty()?track.id:track.label);parent->setToolTip(0,track.id);parent->setFlags(parent->flags()&~Qt::ItemIsSelectable);
        for(int c=0;c<track.clips.size();++c){
            const auto &clip=track.clips[c];auto *item=new QTreeWidgetItem(parent);
            item->setText(0,clip.label.isEmpty()?clip.id:clip.label);item->setToolTip(0,clip.id);item->setText(1,QStringLiteral("%1–%2").arg(clip.startFrame).arg(clip.endFrameExclusive));
            item->setData(0,Qt::UserRole,t);item->setData(0,Qt::UserRole+1,c);
        }
    }
    m_tree->expandAll();filterComponents();
    if(!selectedId.isEmpty())selectEntity(selectedId,false);
    else if(initial){const auto items=visibleComponents();if(!items.isEmpty())m_tree->setCurrentItem(items.first());}
}
void ComponentBrowser::filterComponents() {
    const auto query=m_search->text().trimmed();const bool editable=m_filter->currentData()=="editable";
    int visible=0,total=0;
    for(int t=0;t<m_tree->topLevelItemCount();++t){
        auto *parent=m_tree->topLevelItem(t);const auto &track=m_snapshot.tracks[t];int trackVisible=0;
        const bool trackMatch=track.label.contains(query,Qt::CaseInsensitive)||track.id.contains(query,Qt::CaseInsensitive);
        for(int c=0;c<parent->childCount();++c){
            auto *item=parent->child(c);const auto &clip=track.clips[c];++total;
            const bool writable=std::any_of(clip.inspector.cbegin(),clip.inspector.cend(),[](const auto &field){return field.isEditable();});
            const bool match=(query.isEmpty()||trackMatch||clip.label.contains(query,Qt::CaseInsensitive)||clip.id.contains(query,Qt::CaseInsensitive))&&(!editable||writable);
            item->setHidden(!match);if(match){++visible;++trackVisible;}
        }
        parent->setHidden(trackVisible==0);
    }
    if(auto *item=m_tree->currentItem();item&&(item->isHidden()||(item->parent()&&item->parent()->isHidden()))){m_tree->setCurrentItem(nullptr);m_tree->clearSelection();}
    m_count->setText(QStringLiteral("%1 / %2 个组件").arg(visible).arg(total));m_empty->setVisible(visible==0);
    m_reset->setEnabled(!m_search->text().isEmpty()||editable);
}
void ComponentBrowser::resetFilters() {
    const QSignalBlocker searchBlock(m_search),filterBlock(m_filter);
    m_search->clear();m_filter->setCurrentIndex(0);filterComponents();
}
bool ComponentBrowser::selectEntity(const QString &entityId,bool reveal) {
    for(int t=0;t<m_tree->topLevelItemCount();++t){auto *parent=m_tree->topLevelItem(t);
        for(int c=0;c<parent->childCount();++c){auto *item=parent->child(c);if(item->toolTip(0)!=entityId)continue;
            if(item->isHidden()||parent->isHidden()){if(!reveal)return false;resetFilters();}
            parent->setExpanded(true);m_tree->setCurrentItem(item);m_tree->scrollToItem(item);return true;
        }
    }
    return false;
}
QList<QTreeWidgetItem*> ComponentBrowser::visibleComponents() const {
    QList<QTreeWidgetItem*> items;
    for(int t=0;t<m_tree->topLevelItemCount();++t){auto *parent=m_tree->topLevelItem(t);if(parent->isHidden())continue;
        for(int c=0;c<parent->childCount();++c)if(!parent->child(c)->isHidden())items.append(parent->child(c));
    }
    return items;
}
void ComponentBrowser::activate(QTreeWidgetItem *item) {
    if(!item||!item->parent()||item->isHidden()||item->parent()->isHidden())return;
    const int t=item->data(0,Qt::UserRole).toInt(),c=item->data(0,Qt::UserRole+1).toInt();
    const auto &clip=m_snapshot.tracks[t].clips[c];emit componentActivated(clip.id,clip.startFrame);
}
bool ComponentBrowser::eventFilter(QObject *watched,QEvent *event) {
    if(event->type()==QEvent::KeyPress){
        auto *key=static_cast<QKeyEvent*>(event);
        if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){
            auto *item=m_tree->currentItem();
            if(watched==m_search&&selectedEntityId().isEmpty()){const auto items=visibleComponents();if(!items.isEmpty()){item=items.first();selectEntity(item->toolTip(0),false);}}
            activate(item);return true;
        }
        if((watched==m_tree||watched==m_search)&&(key->key()==Qt::Key_Down||key->key()==Qt::Key_Up)){
            const auto items=visibleComponents();if(items.isEmpty())return true;
            const int current=items.indexOf(m_tree->currentItem());
            const int next=current<0?(key->key()==Qt::Key_Down?0:items.size()-1):qBound(0,current+(key->key()==Qt::Key_Down?1:-1),int(items.size())-1);
            selectEntity(items[next]->toolTip(0),false);m_tree->setFocus();return true;
        }
    }
    return QWidget::eventFilter(watched,event);
}
}
