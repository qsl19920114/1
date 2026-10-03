#include "ui/HistoryPanel.h"
#include "ui/MediaPlayerDialog.h"
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFileInfo>
#include <QDesktopServices>
#include <QJsonObject>
#include <QHeaderView>
namespace qvw::ui {
namespace {
QString recordKind(const QJsonObject &record) {
 if(record.contains("kind")) return record["kind"].toString();
 const auto id=record["id"].toString();
 for(const auto &kind:QStringList{"agent","edit","export"}) if(id.startsWith(kind+"/")) return kind;
 return "other";
}
bool validGoal(const QJsonObject &record) {
 const auto goal=record["goal"];
 return goal.isString()&&!goal.toString().trimmed().isEmpty()&&goal.toString().toUtf8().size()<=8192;
}
QString projectPath(const QString &path) {
 if(path.isEmpty()) return {};
 const QFileInfo file(path);const auto canonical=file.canonicalFilePath();
 return canonical.isEmpty()?file.absoluteFilePath():canonical;
}
}
HistoryPanel::HistoryPanel(QWidget *parent):QWidget(parent){
 setObjectName("historyPanel");auto *layout=new QVBoxLayout(this);
 auto *filters=new QHBoxLayout;
 m_search=new QLineEdit;m_search->setObjectName("historySearch");m_search->setPlaceholderText("搜索目标、工程或执行详情");m_search->setClearButtonEnabled(true);filters->addWidget(m_search,1);
 m_kind=new QComboBox;m_kind->setObjectName("historyKind");m_kind->addItem("全部类型","all");m_kind->addItem("Agent 创作","agent");m_kind->addItem("手动修改","edit");m_kind->addItem("成片导出","export");filters->addWidget(m_kind);
 m_currentOnly=new QCheckBox("仅当前工程");m_currentOnly->setObjectName("historyCurrentProject");m_currentOnly->setEnabled(false);filters->addWidget(m_currentOnly);layout->addLayout(filters);
 m_count=new QLabel;m_count->setObjectName("historyCount");layout->addWidget(m_count);
 auto *splitter=new QSplitter;
 m_tree=new QTreeWidget;m_tree->setObjectName("historyRecords");m_tree->setHeaderLabels({"任务 / 修改 / 成片","工程","时间"});m_tree->setRootIsDecorated(false);m_tree->header()->setSectionResizeMode(0,QHeaderView::Stretch);splitter->addWidget(m_tree);
 m_detail=new QPlainTextEdit;m_detail->setObjectName("historyDetail");m_detail->setReadOnly(true);m_detail->setPlaceholderText("选择记录，查看目标、执行结果与修改详情。历史记录不会自动执行。");splitter->addWidget(m_detail);layout->addWidget(splitter);
 auto *buttons=new QHBoxLayout;m_reuse=new QPushButton("复用目标到当前工程");m_reuse->setObjectName("historyReuseGoal");m_open=new QPushButton("打开所属工程");m_open->setObjectName("historyOpenProject");m_play=new QPushButton("播放成片");m_play->setObjectName("historyPlay");m_reveal=new QPushButton("打开成片文件夹");m_reveal->setObjectName("historyReveal");
 for(auto *button:{m_reuse,m_open,m_play,m_reveal}){button->setEnabled(false);buttons->addWidget(button);}buttons->addStretch();layout->addLayout(buttons);
 connect(m_search,&QLineEdit::textChanged,this,&HistoryPanel::refreshRecords);
 connect(m_kind,&QComboBox::currentIndexChanged,this,&HistoryPanel::refreshRecords);
 connect(m_currentOnly,&QCheckBox::toggled,this,&HistoryPanel::refreshRecords);
 connect(m_tree,&QTreeWidget::currentItemChanged,this,[this]{selected();});
 connect(m_reuse,&QPushButton::clicked,this,[this]{if(!m_available)return;if(auto *item=m_tree->currentItem()){const auto record=item->data(0,Qt::UserRole).toJsonObject();if(validGoal(record))emit reuseGoalRequested(record["goal"].toString());}});
 connect(m_open,&QPushButton::clicked,this,[this]{if(!m_available)return;if(auto *item=m_tree->currentItem()){const auto path=item->data(0,Qt::UserRole).toJsonObject()["project"].toString();if(QFileInfo(path).isFile())emit openProjectRequested(path);}});
 connect(m_play,&QPushButton::clicked,this,[this]{if(auto *item=m_tree->currentItem()){const auto path=item->data(0,Qt::UserRole).toJsonObject()["artifact"].toString();if(QFileInfo(path).isFile())(new MediaPlayerDialog(path,"历史成片",this))->show();}});
 connect(m_reveal,&QPushButton::clicked,this,[this]{if(auto *item=m_tree->currentItem()){const auto path=item->data(0,Qt::UserRole).toJsonObject()["artifact"].toString();if(QFileInfo(path).isFile())QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));}});
 refreshRecords();
}
void HistoryPanel::showRecords(const QJsonArray &records){m_records=records;refreshRecords();}
void HistoryPanel::setCurrentProject(const QString &manifest){m_currentProject=projectPath(manifest);m_currentOnly->setEnabled(!m_currentProject.isEmpty());if(m_currentProject.isEmpty())m_currentOnly->setChecked(false);refreshRecords();}
void HistoryPanel::setAvailable(bool available){m_available=available;selected();}
void HistoryPanel::refreshRecords(){
 QString selectedId;if(auto *item=m_tree->currentItem())selectedId=item->data(0,Qt::UserRole).toJsonObject()["id"].toString();
 m_tree->clear();QTreeWidgetItem *selection=nullptr;const auto keyword=m_search->text().trimmed(),kind=m_kind->currentData().toString();
 for(const auto &v:m_records){const auto record=v.toObject();
  if(kind!="all"&&recordKind(record)!=kind)continue;
  if(m_currentOnly->isChecked()&&projectPath(record["project"].toString())!=m_currentProject)continue;
  bool matches=keyword.isEmpty();for(const auto &field:{"title","projectName","detail","goal"})matches=matches||record[field].toString().contains(keyword,Qt::CaseInsensitive);
  if(!matches)continue;
  auto *item=new QTreeWidgetItem(m_tree,{record["title"].toString(),record["projectName"].toString(),record["time"].toString()});item->setData(0,Qt::UserRole,record);if(!selectedId.isEmpty()&&record["id"].toString()==selectedId)selection=item;
 }
 if(!selection&&m_tree->topLevelItemCount())selection=m_tree->topLevelItem(0);m_tree->setCurrentItem(selection);
 m_count->setText(m_records.isEmpty()?QStringLiteral("暂无历史记录"):QStringLiteral("显示 %1 / %2 条记录%3").arg(m_tree->topLevelItemCount()).arg(m_records.size()).arg(m_tree->topLevelItemCount()==0?QStringLiteral(" · 无匹配记录"):QString()));selected();
}
void HistoryPanel::selected(){
 auto *item=m_tree->currentItem();const auto record=item?item->data(0,Qt::UserRole).toJsonObject():QJsonObject{};
 const auto goal=record["goal"].toString();
 m_detail->setPlainText((goal.isEmpty()?QString():"目标："+goal+"\n\n")+record["detail"].toString()+(record.isEmpty()?QString():"\n\n工程："+record["project"].toString()));
 m_reuse->setEnabled(m_available&&validGoal(record));
 m_reuse->setToolTip(record.isEmpty()?QStringLiteral("请选择包含完整目标的记录"):!validGoal(record)?QStringLiteral("此记录没有可复用的完整目标，或目标超过 8192 UTF-8 字节。旧记录的截断标题不能作为目标。"):!m_available?QStringLiteral("当前有任务进行中，完成后可复用目标"):QStringLiteral("把完整目标填入当前工程的创作输入框，由你修改并点击生成方案"));
 const auto project=record["project"].toString(),artifact=record["artifact"].toString();m_open->setEnabled(m_available&&QFileInfo(project).isFile());m_play->setEnabled(!artifact.isEmpty()&&QFileInfo(artifact).isFile());m_reveal->setEnabled(m_play->isEnabled());if(!artifact.isEmpty()&&!QFileInfo(artifact).isFile())m_detail->appendPlainText("\n成片文件已移动或不存在："+artifact);
}
}
