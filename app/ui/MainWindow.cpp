#include "ui/MainWindow.h"

#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace qvw::ui {
namespace {

QWidget *withTitle(const QString &title, QWidget *body) {
    auto *wrapper = new QWidget;
    auto *layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *caption = new QLabel(title);
    QFont captionFont = caption->font();
    captionFont.setBold(true);
    caption->setFont(captionFont);

    layout->addWidget(caption);
    layout->addWidget(body, 1);
    return wrapper;
}

}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Qt 视频工作台"));
    resize(1280, 800);

    auto *columns = new QSplitter(Qt::Horizontal, this);
    columns->addWidget(buildProjectPanel());

    auto *middle = new QSplitter(Qt::Vertical, columns);
    middle->addWidget(buildPreviewPanel());
    middle->addWidget(buildTaskPanel());
    middle->setStretchFactor(0, 3);
    middle->setStretchFactor(1, 1);
    columns->addWidget(middle);

    columns->addWidget(buildInspectorPanel());
    columns->setStretchFactor(0, 2);
    columns->setStretchFactor(1, 5);
    columns->setStretchFactor(2, 3);

    setCentralWidget(columns);
    statusBar()->showMessage(QStringLiteral("未加载工程"));
}

QWidget *MainWindow::buildProjectPanel() {
    m_componentTree = new QTreeWidget;
    m_componentTree->setHeaderLabels({QStringLiteral("组件"), QStringLiteral("帧范围")});
    m_componentTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    return withTitle(QStringLiteral("工程与组件"), m_componentTree);
}

QWidget *MainWindow::buildPreviewPanel() {
    m_previewPlaceholder = new QLabel(QStringLiteral("预览将在 T010 接入 Studio 后显示"));
    m_previewPlaceholder->setAlignment(Qt::AlignCenter);
    m_previewPlaceholder->setMinimumHeight(280);
    m_previewPlaceholder->setFrameShape(QFrame::StyledPanel);

    auto *body = new QWidget;
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_previewPlaceholder, 1);

    m_spaceSummary = new QLabel(QStringLiteral("画幅：未加载"));
    layout->addWidget(m_spaceSummary);

    return withTitle(QStringLiteral("预览"), body);
}

QWidget *MainWindow::buildInspectorPanel() {
    m_inspectorTable = new QTreeWidget;
    m_inspectorTable->setHeaderLabels({
        QStringLiteral("属性"), QStringLiteral("控件"),
        QStringLiteral("当前值"), QStringLiteral("可编辑"),
    });
    m_inspectorTable->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    return withTitle(QStringLiteral("属性"), m_inspectorTable);
}

QWidget *MainWindow::buildTaskPanel() {
    m_taskLog = new QPlainTextEdit;
    m_taskLog->setReadOnly(true);
    m_taskLog->setMaximumBlockCount(2000);
    return withTitle(QStringLiteral("任务与日志"), m_taskLog);
}

void MainWindow::showSnapshot(const domain::Snapshot &snapshot) {
    m_componentTree->clear();
    m_inspectorTable->clear();

    if (!snapshot.isLoaded()) {
        statusBar()->showMessage(QStringLiteral("未加载工程"));
        return;
    }

    m_spaceSummary->setText(QStringLiteral("画幅：%1").arg(snapshot.space.describe()));

    for (const domain::Track &track : snapshot.tracks) {
        auto *trackItem = new QTreeWidgetItem(m_componentTree);
        trackItem->setText(0, track.label.isEmpty() ? track.id : track.label);

        for (const domain::Clip &clip : track.clips) {
            auto *clipItem = new QTreeWidgetItem(trackItem);
            clipItem->setText(0, clip.label.isEmpty() ? clip.id : clip.label);
            clipItem->setText(1, QStringLiteral("%1–%2")
                                     .arg(clip.startFrame)
                                     .arg(clip.endFrameExclusive));

            for (const domain::InspectorField &field : clip.inspector) {
                auto *row = new QTreeWidgetItem(m_inspectorTable);
                row->setText(0, field.label.isEmpty() ? field.id : field.label);
                row->setText(1, domain::controlKindLabel(field.control));
                row->setText(2, field.value);
                row->setText(3, field.isEditable() ? QStringLiteral("是") : QStringLiteral("否"));
                // Read-only fields carry the server's own explanation; show it
                // verbatim rather than inventing UI wording (API_CONTRACT §4).
                if (!field.isEditable() && !field.disabledReason.isEmpty()) {
                    row->setToolTip(3, field.disabledReason);
                }
            }
        }
    }
    m_componentTree->expandAll();

    statusBar()->showMessage(QStringLiteral("revision %1 · 可写字段 %2 个")
                                 .arg(snapshot.revision)
                                 .arg(snapshot.writableFieldCount()));
}

void MainWindow::appendLog(const QString &line) {
    m_taskLog->appendPlainText(line);
}

}
