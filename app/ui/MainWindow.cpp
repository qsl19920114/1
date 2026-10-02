#include "ui/MainWindow.h"
#include "ui/InspectorControls.h"
#include "ui/StudioTransport.h"
#include "ui/MediaPlayerDialog.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QTimer>
#include <QTextDocument>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QGroupBox>
#include <QDir>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QSplitter>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStandardPaths>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>

namespace qvw::ui {
namespace {
QWidget *withTitle(const QString &title,QWidget *body) {
    auto *wrapper=new QWidget;auto *layout=new QVBoxLayout(wrapper);layout->setContentsMargins(12,12,12,12);
    auto *caption=new QLabel(title);caption->setObjectName("sectionTitle");layout->addWidget(caption);layout->addWidget(body,1);return wrapper;
}
QLabel *copyLabel(const QString &text) {auto *label=new QLabel(text);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);return label;}
}
MainWindow::MainWindow(QWidget *parent):QMainWindow(parent) {
    setWindowTitle(QStringLiteral("FrameLab · 灵感片场"));resize(1540,940);setMinimumSize(1100,720);
    setStyleSheet(R"CSS(
QMainWindow,QDialog,QWidget{background:#101620;color:#dce4ef;font-size:13px;font-family:"PingFang SC";}
QToolBar{background:#141d2a;border:0;border-bottom:1px solid #283446;padding:10px;spacing:8px;}
QToolButton,QPushButton{background:#243044;border:1px solid #35445c;border-radius:6px;padding:7px 12px;color:#dae5f4;}
QToolButton:hover,QPushButton:hover{background:#30405a;border-color:#60728c;}
QToolButton:disabled,QPushButton:disabled{color:#69788b;background:#1b2432;border-color:#263346;}
QPushButton[primary="true"]{background:#32c7b7;color:#081c21;border-color:#32c7b7;font-weight:600;}
QPushButton[primary="true"]:disabled{background:#24565b;color:#698b90;border-color:#24565b;}
QLabel#brand{font-size:20px;font-weight:700;color:#55ddce;padding-right:20px;}
QLabel#sectionTitle{font-size:14px;font-weight:600;color:#ebf2fc;padding-bottom:6px;}
QLabel#heroTitle{font-size:30px;font-weight:700;color:#f2f7fd;}
QLabel#heroCopy{color:#97a8bf;font-size:15px;}
QLabel#environment{background:#1c2c35;color:#7ce1cf;border-radius:5px;padding:6px 10px;}
QTreeWidget,QPlainTextEdit,QLineEdit,QComboBox,QDoubleSpinBox{background:#151f2d;border:1px solid #2c3a4d;border-radius:5px;color:#dce6f2;selection-background-color:#275b69;}
QTreeWidget{padding:4px;alternate-background-color:#182334;}
QTreeWidget::item{padding:7px 3px;} QTreeWidget::item:selected{background:#254956;color:#dffffb;}
QHeaderView::section{background:#1a2636;color:#94a8c2;border:0;padding:7px;}
QLineEdit,QComboBox,QDoubleSpinBox{padding:6px;} QPlainTextEdit{padding:6px;}
QTabWidget::pane{border:1px solid #28384a;border-radius:5px;}
QTabBar::tab{background:#152030;color:#96a9c2;padding:9px 14px;border:0;}
QTabBar::tab:selected{color:#69e2d3;background:#25384a;border-bottom:2px solid #40cbbb;}
QGroupBox{border:1px solid #2d3c51;border-radius:6px;margin-top:12px;padding-top:16px;}
QSplitter::handle{background:#253347;} QSplitter::handle:horizontal{width:3px;} QSplitter::handle:vertical{height:3px;}
QStatusBar{background:#141e2c;color:#a3b6ce;} QMenu{border:1px solid #35445a;} QMenu::item{padding:8px 24px;} QMenu::item:selected{background:#2e4659;}
QSlider::groove:horizontal{height:5px;background:#304259;border-radius:2px;} QSlider::handle:horizontal{width:12px;margin:-5px 0;background:#55d8c7;border-radius:6px;}
QProgressBar{height:5px;border:0;background:#233347;color:#9ddfd5;} QProgressBar::chunk{background:#40cbbb;}
QScrollBar:vertical{width:9px;background:#121b28;} QScrollBar::handle:vertical{background:#3b4c64;border-radius:4px;min-height:20px;} QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}
)CSS");
    auto *bar=addToolBar(QStringLiteral("工程"));bar->setMovable(false);
    auto *brand=new QLabel("FrameLab");brand->setObjectName("brand");bar->addWidget(brand);
    m_newAction=bar->addAction(QStringLiteral("新建工程…"),this,[this]{newDocumentDialog();});m_newAction->setShortcut(QKeySequence::New);
    m_documentOpenAction=bar->addAction(QStringLiteral("打开工程…"),this,[this]{
        const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("打开工程"),{},"工作台工程 (workbench.qvw.json)");if(!path.isEmpty())emit openDocumentRequested(path);
    });m_documentOpenAction->setShortcut(QKeySequence::Open);
    m_saveAction=bar->addAction(QStringLiteral("保存工程"),this,&MainWindow::saveDocumentRequested);m_saveAction->setShortcut(QKeySequence::Save);
    m_importAction=bar->addAction(QStringLiteral("导入素材…"),this,[this]{
        const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("导入图片或视频 · 视频须为 H.264 / 30fps / 至少8秒"),{},"素材 (*.png *.jpg *.jpeg *.mp4)");
        if(path.isEmpty())return;if(path.endsWith(".mp4",Qt::CaseInsensitive))emit importVideoRequested(path);else emit importImageRequested(path);
    });bar->addSeparator();
    m_undoAction=bar->addAction(QStringLiteral("撤销"),this,&MainWindow::undoRequested);m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction=bar->addAction(QStringLiteral("重做"),this,&MainWindow::redoRequested);m_redoAction->setShortcut(QKeySequence::Redo);
    auto *spacer=new QWidget;spacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);bar->addWidget(spacer);
    m_backendStatus=new QLabel(QStringLiteral("检查环境…"));m_backendStatus->setObjectName("environment");bar->addWidget(m_backendStatus);
    m_exportAction=bar->addAction(QStringLiteral("导出 MP4…"),this,[this]{
        const auto path=QFileDialog::getSaveFileName(this,QStringLiteral("导出成片"),QStringLiteral("灵感片场.mp4"),"MP4 (*.mp4)");
        if(!path.isEmpty())emit exportRequested(path.endsWith(".mp4",Qt::CaseInsensitive)?path:path+".mp4");
    });
    auto *advanced=new QToolButton;advanced->setText(QStringLiteral("更多"));advanced->setPopupMode(QToolButton::InstantPopup);auto *menu=new QMenu(advanced);
    m_sourceAction=menu->addAction(QStringLiteral("编辑源码…"),this,&MainWindow::sourceDialog);
    m_openAction=menu->addAction(QStringLiteral("打开 Run…"),this,&MainWindow::openProjectDialog);
    m_refreshAction=menu->addAction(QStringLiteral("刷新会话"),this,&MainWindow::refreshRequested);m_refreshAction->setShortcut(QKeySequence::Refresh);
    m_closeAction=menu->addAction(QStringLiteral("关闭工程"),this,&MainWindow::closeRequested);
    m_configAction=menu->addAction(QStringLiteral("选择配置…"),this,[this]{const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("选择 version-lock.json"),{},"JSON (*.json)");if(!path.isEmpty())emit configurationRequested(path);});advanced->setMenu(menu);bar->addWidget(advanced);
    m_transport=new StudioTransport(this);
    auto *columns=new QSplitter(Qt::Horizontal,this);m_leftPanel=buildProjectPanel();columns->addWidget(m_leftPanel);
    auto *middle=new QSplitter(Qt::Vertical);middle->addWidget(buildPreviewPanel());middle->addWidget(buildTaskPanel());middle->setSizes({610,200});middle->setStretchFactor(0,4);middle->setStretchFactor(1,1);
    columns->addWidget(middle);m_rightPanel=buildInspectorPanel();columns->addWidget(m_rightPanel);columns->setSizes({275,845,400});columns->setStretchFactor(1,1);setCentralWidget(columns);
    connect(m_componentTree,&QTreeWidget::currentItemChanged,this,[this]{showSelectedInspector();updateActions();});
    connect(m_assetTree,&QTreeWidget::currentItemChanged,this,[this]{updateActions();});
    connect(m_proposalRequest,&QLineEdit::textChanged,this,[this]{updateActions();});
    connect(m_transport,&StudioTransport::positionChanged,this,[this](bool ready,int frame,int last,const QString &time){
        m_play->setEnabled(ready);m_previous->setEnabled(ready);m_next->setEnabled(ready);m_seekSlider->setEnabled(ready);
        if(!m_seekSlider->isSliderDown()){m_seekSlider->setRange(0,last);m_seekSlider->setValue(frame);}m_transportTime->setText(ready?time:QStringLiteral("预览准备中"));
    });updateActions();statusBar()->showMessage(QStringLiteral("正在检查本地创作环境…"));
}
MainWindow::~MainWindow(){m_transport->clear();delete m_webView;m_webView=nullptr;delete m_webProfile;}
QWidget *MainWindow::buildProjectPanel() {
    auto *body=new QWidget;auto *layout=new QVBoxLayout(body);layout->setContentsMargins(0,0,0,0);
    m_documentTitle=copyLabel(QStringLiteral("从一段视频开始创作"));layout->addWidget(m_documentTitle);
    m_libraryTabs=new QTabWidget;
    auto *assets=new QWidget;auto *assetLayout=new QVBoxLayout(assets);
    assetLayout->addWidget(copyLabel(QStringLiteral("导入后选择素材，再应用到兼容组件。视频使用前8秒，原音静音。")));
    m_assetTree=new QTreeWidget;m_assetTree->setObjectName("assets");m_assetTree->setHeaderLabels({"素材","尺寸"});m_assetTree->setRootIsDecorated(false);m_assetTree->header()->setSectionResizeMode(0,QHeaderView::Stretch);m_assetTree->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);assetLayout->addWidget(m_assetTree,1);
    connect(m_assetTree,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item){QApplication::clipboard()->setText(item->data(0,Qt::UserRole).toString());statusBar()->showMessage(QStringLiteral("已复制素材相对路径"),4000);});
    m_applyAssetButton=new QPushButton(QStringLiteral("应用到选中组件"));m_applyAssetButton->setObjectName("applyAsset");m_applyAssetButton->setProperty("primary",true);connect(m_applyAssetButton,&QPushButton::clicked,this,&MainWindow::applySelectedAsset);assetLayout->addWidget(m_applyAssetButton);m_cancelImport=new QPushButton("取消导入");m_cancelImport->hide();assetLayout->addWidget(m_cancelImport);connect(m_cancelImport,&QPushButton::clicked,this,&MainWindow::cancelImportRequested);m_libraryTabs->addTab(assets,"素材");
    m_componentTree=new QTreeWidget;m_componentTree->setObjectName("components");m_componentTree->setHeaderLabels({"组件","帧范围"});m_componentTree->header()->setSectionResizeMode(0,QHeaderView::Stretch);m_libraryTabs->addTab(m_componentTree,"组件");
    auto *examples=new QWidget;auto *sampleLayout=new QVBoxLayout(examples);sampleLayout->addWidget(copyLabel(QStringLiteral("从 Hypit 本地测试视频创建原创横屏作品。相同内容已合并。")));
    m_sampleTree=new QTreeWidget;m_sampleTree->setObjectName("samples");m_sampleTree->setHeaderLabels({"示例"});m_sampleTree->setRootIsDecorated(false);m_sampleTree->header()->setSectionResizeMode(QHeaderView::Stretch);sampleLayout->addWidget(m_sampleTree,1);
    m_samplePreview=new QPushButton("播放示例");m_sampleUse=new QPushButton("用此视频创作…");m_sampleUse->setProperty("primary",true);sampleLayout->addWidget(m_samplePreview);sampleLayout->addWidget(m_sampleUse);
    connect(m_sampleTree,&QTreeWidget::currentItemChanged,this,[this]{updateActions();});connect(m_sampleUse,&QPushButton::clicked,this,&MainWindow::useSelectedSample);
    connect(m_samplePreview,&QPushButton::clicked,this,[this]{const auto *item=m_sampleTree->currentItem();if(!item)return;const auto path=item->data(0,Qt::UserRole).toString();if(!QFileInfo(path).isFile()){showError("示例文件不可读，请检查本地 Hypit。");return;}(new MediaPlayerDialog(path,"本地测试视频",this))->show();});
    m_libraryTabs->addTab(examples,"示例");layout->addWidget(m_libraryTabs,1);return withTitle("创作资源",body);
}
QWidget *MainWindow::buildPreviewPanel() {
    auto *body=new QWidget;auto *layout=new QVBoxLayout(body);layout->setContentsMargins(0,0,0,0);
    auto *header=new QHBoxLayout;auto *title=new QLabel("画面预览");title->setObjectName("sectionTitle");header->addWidget(title);header->addStretch();
    auto *fullStudio=new QPushButton("完整 Studio");fullStudio->setCheckable(true);connect(fullStudio,&QPushButton::toggled,this,[this](bool full){m_transport->setReduced(!full);});header->addWidget(fullStudio);
    auto *focus=new QPushButton("专注预览");focus->setCheckable(true);connect(focus,&QPushButton::toggled,this,[this](bool on){m_leftPanel->setVisible(!on);m_rightPanel->setVisible(!on);});header->addWidget(focus);layout->addLayout(header);
    m_previewStack=new QStackedWidget;m_welcome=new QWidget;auto *welcome=new QVBoxLayout(m_welcome);welcome->setContentsMargins(50,40,50,40);welcome->addStretch();
    auto *hero=new QLabel("让片段成为作品");hero->setObjectName("heroTitle");welcome->addWidget(hero);
    auto *copy=copyLabel("导入素材，调整文案与主题，再导出属于你的故事。");copy->setObjectName("heroCopy");welcome->addWidget(copy);welcome->addSpacing(20);
    auto *steps=copyLabel("01 选择视频    →    02 编辑画面    →    03 导出成片");welcome->addWidget(steps);welcome->addSpacing(18);
    auto *start=new QHBoxLayout;m_welcomeNew=new QPushButton("新建作品");m_welcomeNew->setProperty("primary",true);m_welcomeOpen=new QPushButton("打开已有工程");auto *browse=new QPushButton("浏览本地示例");start->addWidget(m_welcomeNew);start->addWidget(m_welcomeOpen);start->addWidget(browse);start->addStretch();welcome->addLayout(start);welcome->addStretch();
    connect(m_welcomeNew,&QPushButton::clicked,this,[this]{newDocumentDialog();});connect(m_welcomeOpen,&QPushButton::clicked,this,[this]{m_documentOpenAction->trigger();});connect(browse,&QPushButton::clicked,this,[this]{m_leftPanel->show();m_libraryTabs->setCurrentIndex(2);});
    m_previewStack->addWidget(m_welcome);m_previewPlaceholder=copyLabel("正在准备工程预览…");m_previewPlaceholder->setAlignment(Qt::AlignCenter);m_previewStack->addWidget(m_previewPlaceholder);layout->addWidget(m_previewStack,1);
    auto *transport=new QHBoxLayout;m_previous=new QPushButton("上一帧");m_play=new QPushButton("播放 / 暂停");m_play->setObjectName("nativePlay");m_next=new QPushButton("下一帧");m_seekSlider=new QSlider(Qt::Horizontal);m_seekSlider->setObjectName("nativeSeek");m_transportTime=new QLabel("尚未加载");
    for(auto *button:{m_previous,m_play,m_next}){button->setEnabled(false);transport->addWidget(button);}m_seekSlider->setEnabled(false);transport->addWidget(m_seekSlider,1);transport->addWidget(m_transportTime);layout->addLayout(transport);
    connect(m_play,&QPushButton::clicked,m_transport,&StudioTransport::togglePlayback);connect(m_previous,&QPushButton::clicked,m_transport,[this]{m_transport->step(-1);});connect(m_next,&QPushButton::clicked,m_transport,[this]{m_transport->step(1);});connect(m_seekSlider,&QSlider::sliderReleased,m_transport,[this]{m_transport->seek(m_seekSlider->value());});
    connect(m_seekSlider,&QAbstractSlider::actionTriggered,m_transport,[this](int action){
        // Keyboard/wheel actions update sliderPosition before value propagation.
        // Programmatic poll setValue() does not emit actionTriggered.
        if(action!=QAbstractSlider::SliderMove||!m_seekSlider->isSliderDown())m_transport->seek(m_seekSlider->sliderPosition());
    });
    m_spaceSummary=copyLabel("预览由 Hypit 提供 · 本地素材创作");layout->addWidget(m_spaceSummary);return body;
}
QWidget *MainWindow::buildInspectorPanel() {
    auto *tabs=new QTabWidget;
    m_inspectorTable=new QTreeWidget;m_inspectorTable->setHeaderLabels({"属性","类型","当前值","状态"});m_inspectorTable->header()->setSectionResizeMode(0,QHeaderView::ResizeToContents);m_inspectorTable->header()->setSectionResizeMode(2,QHeaderView::Stretch);m_inspectorTable->setColumnHidden(1,true);m_inspectorTable->setColumnWidth(3,65);m_inspectorTable->setRootIsDecorated(false);tabs->addTab(m_inspectorTable,"属性");
    auto *proposals=new QWidget;auto *form=new QVBoxLayout(proposals);form->addWidget(copyLabel("本地模拟提案\n支持：标题改为…、主题色改为#RRGGBB、图片使用第1张。确认后才修改；未连接真实模型。"));
    m_proposalRequest=new QLineEdit;m_proposalRequest->setMaxLength(4096);m_proposalRequest->setPlaceholderText("例如：标题改为一段光影故事");form->addWidget(m_proposalRequest);
    m_generateProposal=new QPushButton("生成模拟提案");m_importProposal=new QPushButton("导入提案 JSON…");form->addWidget(m_generateProposal);form->addWidget(m_importProposal);
    m_proposalSummary=new QPlainTextEdit;m_proposalSummary->setReadOnly(true);m_proposalSummary->setPlainText("尚无待确认提案。");form->addWidget(m_proposalSummary,1);
    auto *actions=new QHBoxLayout;m_confirmProposal=new QPushButton("确认修改");m_confirmProposal->setProperty("primary",true);m_discardProposal=new QPushButton("放弃提案");actions->addWidget(m_confirmProposal);actions->addWidget(m_discardProposal);form->addLayout(actions);
    connect(m_generateProposal,&QPushButton::clicked,this,[this]{emit demoProposalRequested(m_proposalRequest->text());});
    connect(m_importProposal,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,"导入外部提案（来源未核验）",{},"JSON (*.json)");if(path.isEmpty())return;QFile file(path);if(!QFileInfo(path).isFile()||!file.open(QIODevice::ReadOnly)||file.size()>65536){showError("提案文件无法读取或超过64KiB。");return;}const auto data=file.read(65537);if(data.size()>65536){showError("提案超过64KiB。");return;}emit importProposalRequested(data);});
    connect(m_confirmProposal,&QPushButton::clicked,this,&MainWindow::confirmProposalRequested);connect(m_discardProposal,&QPushButton::clicked,this,&MainWindow::discardProposalRequested);tabs->addTab(proposals,"提案");return withTitle("调整作品",tabs);
}
QWidget *MainWindow::buildTaskPanel() {
    m_taskTabs=new QTabWidget;auto *exportBody=new QWidget;auto *layout=new QVBoxLayout(exportBody);
    m_exportSummary=copyLabel("完成编辑后点击右上角“导出 MP4…”。");layout->addWidget(m_exportSummary);
    m_exportProgress=new QProgressBar;m_exportProgress->setTextVisible(false);m_exportProgress->setRange(0,100);m_exportProgress->setValue(0);layout->addWidget(m_exportProgress);
    auto *buttons=new QHBoxLayout;m_cancelExport=new QPushButton("取消构建");m_stopExport=new QPushButton("停止观察");m_resumeExport=new QPushButton("恢复观察");m_clearExportCache=new QPushButton("清理终态缓存");
    m_playFilm=new QPushButton("播放成片");m_playFilm->setObjectName("playFilm");m_playFilm->setProperty("primary",true);m_revealFilm=new QPushButton("打开所在文件夹");
    for(auto *button:{m_playFilm,m_revealFilm,m_cancelExport,m_stopExport,m_resumeExport,m_clearExportCache}){button->setEnabled(false);buttons->addWidget(button);}buttons->addStretch();layout->addLayout(buttons);layout->addStretch();m_taskTabs->addTab(exportBody,"导出任务");
    m_taskLog=new QPlainTextEdit;m_taskLog->setReadOnly(true);m_taskLog->setMaximumBlockCount(2000);m_taskTabs->addTab(m_taskLog,"运行日志");
    connect(m_cancelExport,&QPushButton::clicked,this,&MainWindow::cancelExportRequested);connect(m_stopExport,&QPushButton::clicked,this,&MainWindow::stopExportObservationRequested);connect(m_resumeExport,&QPushButton::clicked,this,&MainWindow::resumeExportRequested);connect(m_clearExportCache,&QPushButton::clicked,this,&MainWindow::clearExportCacheRequested);
    connect(m_playFilm,&QPushButton::clicked,this,[this]{if(m_exportTask.phase!="complete"||!QFileInfo(m_exportTask.destination).isFile()){showError("成片不可读，请检查导出路径。");return;}(new MediaPlayerDialog(m_exportTask.destination,"FrameLab · 成片播放",this))->show();});
    connect(m_revealFilm,&QPushButton::clicked,this,[this]{if(m_exportTask.phase=="complete"&&QFileInfo(m_exportTask.destination).isFile())QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_exportTask.destination).absolutePath()));});return m_taskTabs;
}
void MainWindow::updateActions() {
    const bool busy=m_editorBusy||m_importBusy;
    const bool observing=QStringList{"planning","submitting","working","cancelling","getting","validating"}.contains(m_exportTask.phase);
    const bool canSwitch=m_backendReady&&!busy&&!observing;
    m_newAction->setEnabled(canSwitch);m_documentOpenAction->setEnabled(canSwitch);m_openAction->setEnabled(canSwitch);
    m_welcomeNew->setEnabled(canSwitch);m_welcomeOpen->setEnabled(canSwitch);
    m_configAction->setEnabled(!busy&&!observing);
    m_closeAction->setEnabled((m_hasDocument||!m_previewUrl.isEmpty())&&!busy&&!observing);
    m_saveAction->setEnabled(m_hasDocument&&!busy);m_importAction->setEnabled(m_hasDocument&&m_editorReady&&!busy);
    const bool edit=m_editorReady&&!busy;
    m_exportAction->setEnabled(m_hasDocument&&edit&&!m_exportActive);
    m_undoAction->setEnabled(edit&&m_canUndo);m_redoAction->setEnabled(edit&&m_canRedo);
    m_sourceAction->setEnabled(edit&&!m_snapshot.sourceFiles.isEmpty());m_refreshAction->setEnabled(!busy&&!m_previewUrl.isEmpty());m_inspectorTable->setEnabled(edit);
    const auto field=compatibleAssetField();m_applyAssetButton->setEnabled(m_hasDocument&&edit&&!field.isEmpty());
    m_applyAssetButton->setToolTip(field.isEmpty()?QStringLiteral("请选择素材，以及含兼容图片或视频属性的组件。"):QStringLiteral("通过编辑事务更新组件；完成后记录撤销历史。"));
    m_generateProposal->setEnabled(m_hasDocument&&edit&&!m_proposalRequest->text().trimmed().isEmpty());m_importProposal->setEnabled(m_hasDocument&&edit);m_confirmProposal->setEnabled(m_hasProposal&&m_hasDocument&&edit);m_discardProposal->setEnabled(m_hasProposal&&!busy);
    const auto *sample=m_sampleTree->currentItem();const bool validSample=sample&&!sample->data(0,Qt::UserRole).toString().isEmpty();m_samplePreview->setEnabled(validSample);m_sampleUse->setEnabled(validSample&&canSwitch);
    m_backendStatus->setText(m_importBusy?QStringLiteral("正在校验素材…"):m_editorBusy?QStringLiteral("正在保存修改…"):m_editorReady?QStringLiteral("可以编辑"):m_hasDocument?QStringLiteral("正在准备预览…"):m_backendReady?QStringLiteral("本地环境就绪"):!m_backendError.isEmpty()?QStringLiteral("环境未就绪"):QStringLiteral("检查创作环境…"));
}
QString MainWindow::compatibleAssetField() const {
    const auto *asset=m_assetTree->currentItem();const auto *item=m_componentTree->currentItem();
    if(!asset||!item||!item->data(0,Qt::UserRole).isValid())return {};
    const auto mime=asset->data(0,Qt::UserRole+1).toString();
    const int t=item->data(0,Qt::UserRole).toInt(),c=item->data(0,Qt::UserRole+1).toInt();
    if(t<0||t>=m_snapshot.tracks.size()||c<0||c>=m_snapshot.tracks[t].clips.size())return {};
    for(const auto &field:m_snapshot.tracks[t].clips[c].inspector)if(field.isEditable()&&((field.binding=="image"&&(mime=="image/png"||mime=="image/jpeg"))||(field.binding=="video"&&mime=="video/mp4")))return field.id;
    return {};
}
void MainWindow::setBackendAvailable(bool ready){m_backendReady=ready;if(ready){m_backendError.clear();m_backendStatus->setToolTip({});}updateActions();if(ready&&!m_hasDocument)statusBar()->showMessage("环境就绪，选择示例或创建作品。");}
void MainWindow::setImportBusy(bool busy){m_importBusy=busy;m_cancelImport->setVisible(busy);updateActions();if(busy)statusBar()->showMessage("正在校验并导入视频，请稍候…");}
void MainWindow::setSamples(const domain::VideoSamples &samples) {
    m_samples=samples;m_sampleTree->clear();
    for(const auto &sample:samples){auto *item=new QTreeWidgetItem(m_sampleTree);item->setText(0,sample.available?QStringLiteral("%1\n%2 个本地副本 · 点击播放或创作").arg(sample.name).arg(sample.sources.size()):QStringLiteral("%1\n可导入自己的视频，或在 Hypit 中生成样例。").arg(sample.name));item->setData(0,Qt::UserRole,sample.available?sample.path:QString());item->setToolTip(0,QStringLiteral("来源：外部 Hypit\n%1\nSHA-256：%2\n%3").arg(sample.sources.join("\n"),sample.sha256,sample.error));}
    if(samples.isEmpty()){auto *item=new QTreeWidgetItem(m_sampleTree);item->setText(0,"未找到本地 Hypit 测试视频\n请检查配置；也可导入自己的视频。");item->setFlags(Qt::NoItemFlags);}
    else m_sampleTree->setCurrentItem(m_sampleTree->topLevelItem(0));updateActions();
}
void MainWindow::useSelectedSample(){const auto *item=m_sampleTree->currentItem();if(item&&!item->data(0,Qt::UserRole).toString().isEmpty())newDocumentDialog(item->data(0,Qt::UserRole).toString());}
void MainWindow::showProposal(const QString &summary,bool pending){m_hasProposal=pending;m_proposalSummary->setPlainText(summary.isEmpty()?QStringLiteral("尚无待确认提案。"):summary);updateActions();}
void MainWindow::showExportTask(const domain::ExportTask &task) {
    m_exportTask=task;const bool observing=QStringList{"planning","submitting","working","cancelling","getting","validating"}.contains(task.phase);m_exportActive=task.active||observing;
    const QMap<QString,QString> labels{{"idle","尚无任务"},{"planning","检查作品与构建计划"},{"submitting","准备渲染"},{"working","正在渲染画面"},{"cancelling","正在取消构建"},{"stopped","已停止观察，可恢复查看"},{"getting","正在获取成片"},{"validating","正在校验视频与全片解码"},{"complete","成片已完成 · 校验通过"},{"failed","导出未完成"},{"cancelled","构建已取消"}};
    auto summary=labels.value(task.phase,task.phase);if(!task.destination.isEmpty())summary+="\n"+task.destination;if(!task.error.isEmpty())summary+="\n"+task.error;m_exportSummary->setText(summary);
    m_exportSummary->setToolTip(QStringLiteral("Build：%1 · revision %2").arg(task.buildId).arg(task.revision));
    m_exportProgress->setRange(0,observing?0:100);if(!observing)m_exportProgress->setValue(task.phase=="complete"?100:0);
    m_cancelExport->setEnabled(task.active&&!task.buildId.isEmpty()&&task.phase!="cancelling");m_stopExport->setEnabled(observing);m_resumeExport->setEnabled(task.phase=="stopped"&&(task.active||!task.buildId.isEmpty()));m_clearExportCache->setEnabled(!task.active&&QStringList{"complete","failed","cancelled"}.contains(task.phase)&&!task.buildId.isEmpty());
    const QFileInfo file(task.destination);const bool film=task.phase=="complete"&&file.isFile()&&file.isReadable()&&file.size()>0;m_playFilm->setEnabled(film);m_revealFilm->setEnabled(film);
    if(task.phase!="idle")m_taskTabs->setCurrentIndex(0);updateActions();
}
void MainWindow::showSnapshot(const domain::Snapshot &snapshot) {
    QString selectedId;if(const auto *item=m_componentTree->currentItem())selectedId=item->toolTip(0);
    if(snapshot.revision!=m_snapshot.revision||snapshot.sourceFingerprint!=m_snapshot.sourceFingerprint)++m_editGeneration;
    m_snapshot=snapshot;m_transport->setSpace(snapshot.space);m_componentTree->clear();m_inspectorTable->clear();
    if(!snapshot.isLoaded()){m_spaceSummary->setText("预览由 Hypit 提供 · 本地素材创作");updateActions();return;}
    m_spaceSummary->setText(snapshot.space.describe()+QStringLiteral(" · 预览由 Hypit 提供"));m_spaceSummary->setToolTip(snapshot.sourcePath);
    QTreeWidgetItem *selected=nullptr;
    for(int t=0;t<snapshot.tracks.size();++t){const auto &track=snapshot.tracks[t];auto *parent=new QTreeWidgetItem(m_componentTree);parent->setText(0,track.label.isEmpty()?track.id:track.label);
        for(int c=0;c<track.clips.size();++c){const auto &clip=track.clips[c];auto *item=new QTreeWidgetItem(parent);item->setText(0,clip.label.isEmpty()?clip.id:clip.label);item->setToolTip(0,clip.id);item->setText(1,QStringLiteral("%1–%2").arg(clip.startFrame).arg(clip.endFrameExclusive));item->setData(0,Qt::UserRole,t);item->setData(0,Qt::UserRole+1,c);if(!selected||clip.id==selectedId)selected=item;}
    }
    m_componentTree->expandAll();if(selected)m_componentTree->setCurrentItem(selected);updateActions();statusBar()->showMessage(QStringLiteral("作品已加载 · %1 个可编辑属性").arg(snapshot.writableFieldCount()),5000);
}
void MainWindow::showSelectedInspector() {
    m_inspectorTable->clear();const auto *item=m_componentTree->currentItem();if(!item||!item->data(0,Qt::UserRole).isValid())return;
    const int t=item->data(0,Qt::UserRole).toInt(),c=item->data(0,Qt::UserRole+1).toInt();if(t<0||t>=m_snapshot.tracks.size()||c<0||c>=m_snapshot.tracks[t].clips.size())return;
    for(const auto &field:m_snapshot.tracks[t].clips[c].inspector){auto *row=new QTreeWidgetItem(m_inspectorTable);row->setText(0,field.label.isEmpty()?field.id:field.label);row->setText(1,domain::controlKindLabel(field.control));row->setText(3,field.isEditable()?QStringLiteral("可编辑"):QStringLiteral("只读"));row->setToolTip(3,field.disabledReason);
        std::function<void(const QVariant&)> commit;if(m_editorReady){const auto generation=m_editGeneration;const auto entity=m_snapshot.tracks[t].clips[c].id;commit=[this,generation,entity,id=field.id](const QVariant &value){QTimer::singleShot(0,this,[this,generation,entity,id,value]{if(generation==m_editGeneration&&m_editorReady&&!m_editorBusy&&!m_importBusy)emit editRequested(entity,id,value);});};}
        if(field.binding=="video"||field.binding=="image") {
            row->setText(0,field.binding=="video"?QStringLiteral("视频素材"):QStringLiteral("图片素材"));
            auto *label=copyLabel(QStringLiteral("模板默认素材"));
            for(int n=0;n<m_assetTree->topLevelItemCount();++n){const auto *asset=m_assetTree->topLevelItem(n);if(asset->data(0,Qt::UserRole).toString()==field.rawValue.toString())label->setText(asset->text(0));}
            label->setToolTip(QStringLiteral("选择左侧素材并点击应用。\n%1").arg(field.rawValue.toString()));
            m_inspectorTable->setItemWidget(row,2,label);
        } else m_inspectorTable->setItemWidget(row,2,createInspectorControl(field,m_inspectorTable,commit));
    }
}
void MainWindow::showPreview(const QUrl &url) {
    m_transport->clear();m_previewUrl=url;
    if(!m_webView){m_webView=new QWebEngineView;m_webProfile=new QWebEngineProfile(this);m_webView->setPage(new QWebEnginePage(m_webProfile,m_webView));m_previewStack->addWidget(m_webView);
        connect(m_webView,&QWebEngineView::loadFinished,this,[this](bool ok){if(m_previewUrl.isEmpty()||m_webView->url()!=m_previewUrl)return;appendLog(ok?QStringLiteral("Studio 页面加载完成。"):QStringLiteral("Studio 页面加载失败。"));if(ok){m_transport->attach(m_webView);m_transport->setSpace(m_snapshot.space);}emit previewLoaded(ok);});
    }
    m_previewStack->setCurrentWidget(m_webView);m_webView->load(url);updateActions();
}
void MainWindow::clearProject(){++m_editGeneration;m_transport->clear();m_previewUrl=QUrl();if(m_webView){m_webView->stop();m_webView->setUrl(QUrl("about:blank"));}m_editorReady=false;m_editorBusy=false;m_canUndo=m_canRedo=false;m_previewStack->setCurrentWidget(m_hasDocument?static_cast<QWidget*>(m_previewPlaceholder):m_welcome);showSnapshot({});updateActions();}
void MainWindow::showError(const QString &error){if(!m_backendReady){m_backendError=error;m_backendStatus->setToolTip(error);updateActions();}appendLog(QStringLiteral("错误：%1").arg(error));statusBar()->showMessage(error);if(!m_snapshot.isLoaded()&&m_hasDocument){m_previewPlaceholder->setText("预览未就绪\n"+error);m_previewStack->setCurrentWidget(m_previewPlaceholder);}m_taskTabs->setCurrentIndex(1);}
void MainWindow::appendLog(const QString &line){m_taskLog->appendPlainText(line);}
void MainWindow::setEditorState(bool ready,bool busy,bool canUndo,bool canRedo){const bool changed=m_editorReady!=ready;m_editorReady=ready;m_editorBusy=busy;m_canUndo=canUndo;m_canRedo=canRedo;if(changed)showSelectedInspector();updateActions();if(busy)statusBar()->showMessage("正在保存修改并检查编译…");}
void MainWindow::applySelectedAsset(){const auto field=compatibleAssetField();if(field.isEmpty()||!m_hasDocument||!m_editorReady||m_editorBusy||m_importBusy)return;const auto *item=m_componentTree->currentItem();const int t=item->data(0,Qt::UserRole).toInt(),c=item->data(0,Qt::UserRole+1).toInt();emit editRequested(m_snapshot.tracks[t].clips[c].id,field,m_assetTree->currentItem()->data(0,Qt::UserRole));}
void MainWindow::showDocument(const domain::Project &project) {
    m_hasDocument=true;setWindowTitle(QStringLiteral("%1 — FrameLab · 灵感片场").arg(project.name));m_documentTitle->setText(project.name);m_documentTitle->setToolTip(project.rootPath);
    QString selected;if(const auto *item=m_assetTree->currentItem())selected=item->data(0,Qt::UserRole).toString();m_assetTree->clear();QTreeWidgetItem *selection=nullptr;
    for(const auto &asset:project.assets){auto *item=new QTreeWidgetItem(m_assetTree);item->setText(0,(asset.mime=="video/mp4"?QStringLiteral("视频 · "):QStringLiteral("图片 · "))+asset.originalName);item->setText(1,QStringLiteral("%1×%2").arg(asset.width).arg(asset.height));item->setToolTip(0,asset.path);item->setData(0,Qt::UserRole,"./"+asset.path);item->setData(0,Qt::UserRole+1,asset.mime);if(!selection||"./"+asset.path==selected)selection=item;}
    if(selection)m_assetTree->setCurrentItem(selection);showSelectedInspector();if(m_previewUrl.isEmpty())m_previewStack->setCurrentWidget(m_previewPlaceholder);updateActions();
}
void MainWindow::clearDocument(){m_hasDocument=false;m_importBusy=false;setWindowTitle("FrameLab · 灵感片场");m_documentTitle->setText("从一段视频开始创作");m_assetTree->clear();showProposal({},false);showExportTask({});if(m_previewUrl.isEmpty())m_previewStack->setCurrentWidget(m_welcome);updateActions();}
void MainWindow::newDocumentDialog(const QString &samplePath) {
    QDialog dialog(this);dialog.setWindowTitle(samplePath.isEmpty()?"新建作品":"用示例视频创作");dialog.resize(550,300);auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;
    auto *name=new QLineEdit(samplePath.isEmpty()?QStringLiteral("我的光影故事"):QStringLiteral("对话之外 · 光影故事"));form->addRow("工程名称",name);
    auto *templates=new QComboBox;templates->addItem("视频故事 · 横屏文案 + 视频","video-story");templates->addItem("图片标题卡 · 文案 + 图片","title-card");templates->setEnabled(samplePath.isEmpty());form->addRow("创作模板",templates);
    auto *folder=new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));auto *row=new QWidget;auto *paths=new QHBoxLayout(row);paths->setContentsMargins(0,0,0,0);paths->addWidget(folder,1);auto *browse=new QPushButton("选择…");paths->addWidget(browse);form->addRow("保存位置",row);
    connect(browse,&QPushButton::clicked,&dialog,[&]{const auto path=QFileDialog::getExistingDirectory(&dialog,"选择保存位置",folder->text());if(!path.isEmpty())folder->setText(path);});layout->addLayout(form);
    layout->addWidget(copyLabel("创建同名子目录。视频模板使用前8秒，原音静音；导入须为 H.264 MP4 / 30fps / 至少8秒 / 64MiB以内。"));auto *error=copyLabel({});layout->addWidget(error);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText("创建作品");layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{const auto value=name->text().trimmed();if(value.isEmpty()||value=="."||value==".."||value.contains('/')||value.contains('\\')||value.contains(':')){error->setText("请输入不含路径分隔符的工程名称。");return;}if(!QFileInfo(folder->text()).isDir()){error->setText("请选择存在的保存目录。");return;}dialog.accept();});
    if(dialog.exec()!=QDialog::Accepted)return;const auto value=name->text().trimmed(),destination=QDir(folder->text()).filePath(value);
    if(samplePath.isEmpty())emit newTemplateDocumentRequested(templates->currentData().toString(),destination,value);else emit sampleDocumentRequested(samplePath,destination,value);
}
void MainWindow::sourceDialog() {
    if(!m_editorReady||m_editorBusy||m_snapshot.sourceFiles.isEmpty())return;
    const auto files=m_snapshot.sourceFiles;
    QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("编辑当前工程源码"));dialog.resize(850,600);
    auto *layout=new QVBoxLayout(&dialog);auto *paths=new QComboBox;paths->addItems(files.keys());
    paths->setCurrentText(m_snapshot.sourcePath);auto *text=new QPlainTextEdit;
    text->setPlainText(files.value(paths->currentText()));
    layout->addWidget(new QLabel(QStringLiteral("提交后检查编译；失败时尝试恢复本次预像，检测到外部改动则停止恢复。")));
    layout->addWidget(paths);layout->addWidget(text,1);
    connect(text->document(),&QTextDocument::modificationChanged,paths,[paths](bool modified){paths->setEnabled(!modified);});
    paths->setToolTip(QStringLiteral("每次提交一个文件；编辑后可取消本次操作再选择其他文件。"));
    connect(paths,&QComboBox::currentTextChanged,&dialog,[&](const QString &path){text->setPlainText(files.value(path));});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted&&text->toPlainText()!=files.value(paths->currentText()))
        emit sourceEditRequested(paths->currentText(),text->toPlainText());
}
void MainWindow::openProjectDialog() {
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("打开视频工程")); dialog.resize(680, 240);
    auto *layout = new QVBoxLayout(&dialog); auto *form = new QFormLayout;
    auto *run = new QLineEdit; auto *workspace = new QLineEdit; auto *runtime = new QLineEdit;
    auto addPath = [&](const QString &label, QLineEdit *edit, auto browse) {
        auto *row = new QWidget; auto *horizontal = new QHBoxLayout(row); horizontal->setContentsMargins(0,0,0,0);
        auto *button = new QPushButton(QStringLiteral("选择…")); horizontal->addWidget(edit,1); horizontal->addWidget(button);
        connect(button, &QPushButton::clicked, &dialog, browse); form->addRow(label,row);
    };
    addPath(QStringLiteral("Run 文件"), run, [&] {
        const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("选择 .svrun 文件"), run->text(), "Hypit Run (*.svrun)");
        if (path.isEmpty()) return;
        run->setText(path);
        if (workspace->text().isEmpty()) workspace->setText(QFileInfo(path).absolutePath());
        const QString suggested = QDir(workspace->text()).filePath("hypit.runtime.json");
        if (runtime->text().isEmpty() && QFileInfo::exists(suggested)) runtime->setText(suggested);
    });
    addPath(QStringLiteral("工程目录"), workspace, [&] {
        const auto path = QFileDialog::getExistingDirectory(&dialog, QStringLiteral("选择工程目录"), workspace->text());
        if (!path.isEmpty()) workspace->setText(path);
    });
    addPath(QStringLiteral("Runtime 配置"), runtime, [&] {
        const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("选择本地 Runtime 配置"), workspace->text(), "JSON (*.json)");
        if (!path.isEmpty()) runtime->setText(path);
    });
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) emit openRequested(workspace->text(), run->text(), runtime->text());
}
}
