#include "ui/SceneStrip.h"
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QImageReader>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QMap>
#include <QSignalBlocker>
namespace qvw::ui {
SceneStrip::SceneStrip(QWidget *parent):QWidget(parent) {
 auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(3);
 m_target=new QLabel("场景 · 点击定位并选择组件");m_target->setObjectName("sceneTarget");m_target->setTextFormat(Qt::PlainText);layout->addWidget(m_target);
 m_list=new QListWidget;m_list->setObjectName("scenes");m_list->setViewMode(QListView::IconMode);m_list->setFlow(QListView::LeftToRight);m_list->setWrapping(false);m_list->setMovement(QListView::Static);m_list->setIconSize({112,63});m_list->setGridSize({145,100});m_list->setFixedHeight(112);m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);layout->addWidget(m_list);
 connect(m_list,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){emit sceneActivated(item->data(Qt::UserRole).toString(),item->data(Qt::UserRole+1).toInt());});
}
void SceneStrip::setSnapshot(const domain::Snapshot &snapshot,const domain::Project &project) {
 QSignalBlocker blocker(m_list);m_list->clear();
 struct Scene {const domain::Clip *clip=nullptr;QString image;};QMap<QPair<int,int>,Scene> scenes;
 for(const auto &track:snapshot.tracks)for(const auto &clip:track.clips){
  if(clip.endFrameExclusive<=clip.startFrame)continue;
  auto &scene=scenes[{clip.startFrame,clip.endFrameExclusive}];if(!scene.clip)scene.clip=&clip;
  for(const auto &field:clip.inspector)if(field.binding=="image")for(const auto &asset:project.assets)if(field.rawValue.toString()=="./"+asset.path||field.rawValue.toString()==asset.path){
   const auto root=QFileInfo(project.rootPath).canonicalFilePath();const auto path=QFileInfo(QDir(root).filePath(asset.path)).canonicalFilePath();
   if(!root.isEmpty()&&path.startsWith(root+'/')&&QFileInfo(path).isFile()){scene.image=path;scene.clip=&clip;}
  }
 }
 int index=0;for(auto it=scenes.cbegin();it!=scenes.cend();++it){
  const auto &clip=*it->clip;QPixmap thumbnail(112,63);thumbnail.fill(QColor("#24384a"));
  if(!it->image.isEmpty()){QImageReader reader(it->image);reader.setAutoTransform(true);const auto size=reader.size();if(size.isValid()&&qint64(size.width())*size.height()<=40000000){reader.setScaledSize(size.scaled(112,63,Qt::KeepAspectRatio));const auto image=reader.read();if(!image.isNull()){QPainter painter(&thumbnail);painter.drawImage(QRect(QPoint((112-image.width())/2,(63-image.height())/2),image.size()),image);}}}
  else{QPainter painter(&thumbnail);painter.setPen(QColor("#69e2d3"));painter.drawText(thumbnail.rect().adjusted(5,5,-5,-5),Qt::AlignCenter|Qt::TextWordWrap,clip.label.isEmpty()?QString::number(index+1):clip.label.left(28));}
  const double fps=snapshot.space.frameRate>0?snapshot.space.frameRate:30.;
  auto *item=new QListWidgetItem(QIcon(thumbnail),QString("%1 · %2–%3s").arg(++index).arg(clip.startFrame/fps,0,'f',1).arg(clip.endFrameExclusive/fps,0,'f',1),m_list);
  item->setData(Qt::UserRole,clip.id);item->setData(Qt::UserRole+1,clip.startFrame);item->setData(Qt::UserRole+2,clip.endFrameExclusive);item->setToolTip(clip.label+"\n"+clip.id);
 }
 setVisible(m_list->count()>0);
}
void SceneStrip::setPosition(int frame){QSignalBlocker blocker(m_list);for(int i=0;i<m_list->count();++i){auto *item=m_list->item(i);if(frame>=item->data(Qt::UserRole+1).toInt()&&frame<item->data(Qt::UserRole+2).toInt()){m_list->setCurrentRow(i);return;}}m_list->setCurrentRow(-1);}
void SceneStrip::setSelection(const QString &id){m_target->setText(id.isEmpty()?QStringLiteral("场景 · 点击定位并选择组件"):QStringLiteral("当前修改对象：%1").arg(id));}
}
