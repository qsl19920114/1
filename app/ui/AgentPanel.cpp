#include "ui/AgentPanel.h"
#include "ui/PlanReviewPanel.h"
#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace qvw::ui {
namespace {
QLabel *plainLabel(const QString &text, const QString &name) {
    auto *label = new QLabel(text); label->setObjectName(name); label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true); label->setTextInteractionFlags(Qt::TextSelectableByMouse); return label;
}
QString phaseText(const QString &phase) {
    const QMap<QString, QString> phases{{"idle", "准备创作"}, {"thinking", "正在生成方案"}, {"review", "等待审阅"},
        {"creating", "正在创建工程"}, {"applying", "正在修改工程"}, {"undoing", "正在撤销上一步"},
        {"failed", "执行失败"}, {"paused", "任务已暂停"}, {"stale", "方案版本已过期"},
        {"complete", "任务已完成"}, {"completed", "任务已完成"}, {"clarify", "需要补充信息"}, {"cancelled", "任务已停止"}};
    return phases.value(phase, phase);
}
bool busyPhase(const QString &phase) { return QStringList{"thinking", "applying", "creating", "undoing"}.contains(phase); }
}
AgentPanel::AgentPanel(QWidget *parent):QWidget(parent) {
    setObjectName("agentPanel"); setMinimumWidth(330);
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(12, 12, 12, 12); layout->setSpacing(8);
    layout->addWidget(plainLabel(QStringLiteral("Agent 创作"), "agentTitle"));
    layout->addWidget(plainLabel(QStringLiteral("真实 Codex · 使用当前 Codex 登录"), "agentModel"));
    m_goal = new QPlainTextEdit; m_goal->setObjectName("agentGoal"); m_goal->setFixedHeight(90);
    m_goal->setPlaceholderText(QStringLiteral("描述想做的作品，或说明当前工程要怎样修改…")); layout->addWidget(m_goal);
    m_selection = plainLabel(QStringLiteral("Qt 选中：无"), "agentSelection"); layout->addWidget(m_selection);
    m_scope = new QCheckBox(QStringLiteral("下一次请求仅当前组件")); m_scope->setObjectName("agentScope");
    m_scope->setToolTip(QStringLiteral("下一次请求默认作用于当前工程；勾选后只修改 Qt 中选中的组件。")); layout->addWidget(m_scope);
    m_taskScope = plainLabel(QStringLiteral("当前范围：当前工程"), "agentTaskScope"); layout->addWidget(m_taskScope);
    m_images = new QPushButton(QStringLiteral("选择本地图片…")); m_images->setObjectName("agentImages"); layout->addWidget(m_images);
    m_assetSummary = plainLabel(QStringLiteral("尚未选择图片。模型只接收名称、尺寸与登记信息；图片在本机使用。"), "agentAssetSummary"); layout->addWidget(m_assetSummary);
    m_assets = new QListWidget; m_assets->setObjectName("agentAssets"); m_assets->setIconSize(QSize(40, 44)); m_assets->setFixedHeight(78);
    m_assets->setSelectionMode(QAbstractItemView::NoSelection); m_assets->hide(); layout->addWidget(m_assets);
    m_generate = new QPushButton(QStringLiteral("生成可编辑方案")); m_generate->setObjectName("agentGenerate"); m_generate->setProperty("primary", true); layout->addWidget(m_generate);
    m_phase = plainLabel(QStringLiteral("准备创作"), "agentPhase"); layout->addWidget(m_phase);
    m_progress = plainLabel({}, "agentProgress"); m_progress->hide(); layout->addWidget(m_progress);
    m_message = plainLabel({}, "agentMessage"); m_message->hide(); layout->addWidget(m_message);
    m_review = new PlanReviewPanel(this); layout->addWidget(m_review, 1);
    auto *actions = new QHBoxLayout;
    const auto button = [actions](const QString &text, const QString &name) { auto *b = new QPushButton(text); b->setObjectName(name); actions->addWidget(b); return b; };
    m_stop = button(QStringLiteral("停止后续"), "agentStop"); m_retry = button(QStringLiteral("重试失败步骤"), "agentRetry");
    m_repair = button(QStringLiteral("根据现状修订"), "agentRepair"); layout->addLayout(actions);
    auto *history = new QHBoxLayout; m_undo = new QPushButton(QStringLiteral("撤销上一步")); m_undo->setObjectName("agentUndo"); history->addWidget(m_undo);
    m_restore = new QPushButton(QStringLiteral("恢复任务")); m_restore->setObjectName("agentRestore"); history->addWidget(m_restore); layout->addLayout(history);
    m_versions = plainLabel({}, "agentVersions"); layout->addWidget(m_versions);
    auto *toggle = new QToolButton; toggle->setObjectName("agentEventsToggle"); toggle->setText(QStringLiteral("展开任务记录")); toggle->setCheckable(true);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); toggle->setArrowType(Qt::RightArrow); layout->addWidget(toggle);
    m_events = new QPlainTextEdit; m_events->setObjectName("agentEvents"); m_events->setReadOnly(true); m_events->setMaximumBlockCount(200); m_events->setMaximumHeight(130); m_events->hide(); layout->addWidget(m_events);
    connect(toggle, &QToolButton::toggled, this, [this, toggle](bool expanded) { m_events->setVisible(expanded); toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow); toggle->setText(expanded ? QStringLiteral("收起任务记录") : QStringLiteral("展开任务记录")); });
    connect(m_goal, &QPlainTextEdit::textChanged, this, &AgentPanel::updateActions);
    connect(m_generate, &QPushButton::clicked, this, [this] {
        if (!m_generate->isEnabled()) return;
        const auto goal = m_goal->toPlainText().trimmed();
        emit scopeChanged(m_scope->isChecked() ? m_selectedEntity : QString());
        emit generateRequested(goal);
    });
    connect(m_images, &QPushButton::clicked, this, [this] {
        const auto paths = QFileDialog::getOpenFileNames(this, QStringLiteral("选择图片 · 本地使用，不上传像素"), {}, QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
        if (!paths.isEmpty() && m_images->isEnabled()) emit imagesRequested(paths);
    });
    connect(m_scope, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_status.taskId.isEmpty()) m_taskScope->setText(checked ? QStringLiteral("当前范围：%1").arg(m_selectedEntity) : QStringLiteral("当前范围：当前工程"));
        emit scopeChanged(checked ? m_selectedEntity : QString());
    });
    connect(m_review, &PlanReviewPanel::approveRequested, this, &AgentPanel::approveRequested);
    connect(m_stop, &QPushButton::clicked, this, &AgentPanel::stopRequested); connect(m_retry, &QPushButton::clicked, this, &AgentPanel::retryRequested);
    connect(m_repair, &QPushButton::clicked, this, &AgentPanel::repairRequested); connect(m_undo, &QPushButton::clicked, this, &AgentPanel::undoRequested);
    connect(m_restore, &QPushButton::clicked, this, &AgentPanel::restoreRequested); updateActions(); updateVersions();
}
void AgentPanel::showPlan(const domain::AgentPlan &plan, const domain::Snapshot &snapshot) { m_review->showPlan(plan, snapshot); updateActions(); }
void AgentPanel::showStatus(const domain::AgentStatus &status) {
    const bool newTask = !status.taskId.isEmpty() && status.taskId != m_status.taskId;
    if (status.taskId.isEmpty() && status.phase == "idle") m_review->clearPlan();
    m_status = status; m_phase->setText(phaseText(status.phase)); m_phase->setToolTip(status.phase);
    if (newTask && !status.goal.isEmpty()) m_goal->setPlainText(status.goal);
    m_taskScope->setText(QStringLiteral("%1：%2").arg(status.taskId.isEmpty() ? QStringLiteral("当前范围") : QStringLiteral("任务范围"),
        status.scope.isEmpty() ? QStringLiteral("当前工程") : status.scope));
    m_progress->setText(status.total > 0 ? QStringLiteral("已完成 %1 / %2 项").arg(status.completed).arg(status.total) : QString()); m_progress->setVisible(status.total > 0);
    m_message->setText(status.message); m_message->setVisible(!status.message.isEmpty());
    m_events->setPlainText(status.events.mid(qMax(qsizetype(0), status.events.size() - 200)).join('\n').right(65536)); updateActions(); updateVersions();
}
void AgentPanel::showAssets(const domain::AgentAssets &assets) {
    m_assets->clear();
    for (const auto &asset : assets) {
        QIcon icon;
        if (!asset.localPath.isEmpty()) {
            QImageReader reader(asset.localPath); reader.setAutoTransform(true); const auto size = reader.size();
            if (size.isValid()) reader.setScaledSize(size.scaled(40, 44, Qt::KeepAspectRatio)); icon = QIcon(QPixmap::fromImage(reader.read()));
        }
        m_assets->addItem(new QListWidgetItem(icon, QStringLiteral("%1 · %2 × %3").arg(asset.asset.originalName).arg(asset.asset.width).arg(asset.asset.height)));
    }
    m_assetSummary->setText(assets.isEmpty() ? QStringLiteral("尚未选择图片。模型只接收名称、尺寸与登记信息；图片在本机使用。") : QStringLiteral("已登记 %1 张图片 · 模型只接收名称、尺寸与登记信息；图片在本机使用。").arg(assets.size()));
    m_assets->setVisible(!assets.isEmpty()); m_review->showAssets(assets); updateActions();
}
void AgentPanel::setSelection(const QString &entityId, const QString &label) {
    const auto previous = m_selectedEntity; m_selectedEntity = entityId;
    m_selection->setText(QStringLiteral("Qt 选中：%1").arg(entityId.isEmpty() ? QStringLiteral("无") : (label.isEmpty() ? entityId : label)));
    if (previous != entityId) emit selectionChanged(entityId);
    if (m_scope->isChecked()) { if (entityId.isEmpty()) m_scope->setChecked(false); else if (previous != entityId) emit scopeChanged(entityId); }
    updateActions();
}
void AgentPanel::setAvailable(bool available) { m_available = available; updateActions(); }
void AgentPanel::setVersions(const domain::Snapshot &snapshot, const domain::ExportTask &task) {
    if (snapshot.revision != m_snapshot.revision || snapshot.sourceFingerprint != m_snapshot.sourceFingerprint || snapshot.previewSrcdoc != m_snapshot.previewSrcdoc) m_previewVersion = {};
    m_snapshot = snapshot; m_export = task; updateVersions();
}
void AgentPanel::setPreviewVersion(const domain::PreviewVersion &version) {
    if (version.isConfirmed() && (version.revision != m_snapshot.revision || version.sourceFingerprint != m_snapshot.sourceFingerprint)) return;
    m_previewVersion = version; updateVersions();
}
void AgentPanel::updateVersions() {
    const int revision = qMax(m_status.revision, m_snapshot.revision);
    const auto project = revision >= 0 ? QStringLiteral("工程 v%1").arg(revision) : QStringLiteral("工程未加载");
    const auto compiled = m_snapshot.isLoaded() ? QStringLiteral("编译快照 v%1").arg(m_snapshot.revision) : QStringLiteral("编译快照未加载"); QString exported = QStringLiteral("尚无导出");
    if (m_export.revision >= 0) {
        if (m_export.phase == "complete") {
            exported = QStringLiteral("最近导出 v%1").arg(m_export.revision);
            if (m_export.sourceFingerprint.isEmpty() || m_snapshot.sourceFingerprint.isEmpty()) exported += QStringLiteral("（版本待核对）");
            else if (m_export.sourceFingerprint != m_snapshot.sourceFingerprint) exported += QStringLiteral("（过期）");
        } else exported = QStringLiteral("导出任务 v%1（%2，未完成）").arg(m_export.revision).arg(m_export.phase);
    }
    QString preview = QStringLiteral("中央可视预览等待工程");
    if (m_snapshot.isLoaded()) preview = m_previewVersion.isConfirmed()
        ? QStringLiteral("中央可视预览 v%1（影像已加载）").arg(m_previewVersion.revision)
        : QStringLiteral("中央可视预览等待 v%1（尚未核验版本）").arg(m_snapshot.revision);
    m_versions->setText(QStringLiteral("%1 · %2 · %3\n%4").arg(project, compiled, exported, preview));
}
void AgentPanel::updateActions() {
    const bool busy = busyPhase(m_status.phase); const bool editable = m_available && !busy;
    m_goal->setEnabled(editable); m_images->setEnabled(editable); m_scope->setEnabled(editable && !m_selectedEntity.isEmpty());
    m_generate->setEnabled(editable && !m_goal->toPlainText().trimmed().isEmpty());
    m_review->setEditingEnabled(editable && m_status.phase != "stale"); m_review->setApprovalEnabled(editable && m_status.canApprove && m_status.phase != "stale");
    m_stop->setEnabled(busy); m_retry->setEnabled(editable && m_status.canRetry && (m_status.phase == "failed" || m_status.phase == "paused"));
    m_repair->setEnabled(editable && (m_status.phase == "failed" || m_status.phase == "paused" || m_status.phase == "stale"));
    m_undo->setEnabled(editable && m_status.canUndo); m_restore->setEnabled(editable);
}
}
