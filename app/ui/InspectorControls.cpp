#include "ui/InspectorControls.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QColor>
#include <limits>

namespace qvw::ui {
QWidget *createInspectorControl(const domain::InspectorField &field, QWidget *parent) {
    QWidget *control = nullptr;
    switch (field.control) {
    case domain::ControlKind::Boolean: {
        auto *box = new QCheckBox(parent);
        box->setChecked(field.value == "true");
        control = box; break;
    }
    case domain::ControlKind::Number: {
        bool numeric = false; const double value = field.value.toDouble(&numeric);
        if (numeric) {
            auto *spin = new QDoubleSpinBox(parent);
            spin->setRange(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
            spin->setDecimals(6); spin->setValue(value); spin->setReadOnly(true);
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons); control = spin;
        }
        break;
    }
    case domain::ControlKind::Select: {
        auto *combo = new QComboBox(parent);
        for (const auto &option : field.options) combo->addItem(option.label, option.value);
        int selected = combo->findData(field.rawValue);
        if (selected < 0) { combo->addItem(field.value, field.rawValue); selected = combo->count() - 1; }
        combo->setCurrentIndex(selected); control = combo; break;
    }
    case domain::ControlKind::Color: {
        auto *button = new QPushButton(field.value, parent);
        const QColor color(field.value);
        if (color.isValid()) button->setStyleSheet(QStringLiteral("border-left: 18px solid %1; padding: 4px;").arg(color.name()));
        control = button; break;
    }
    default: break;
    }
    if (!control) {
        auto *line = new QLineEdit(field.value, parent);
        line->setReadOnly(true); control = line;
    }
    // M1 is a read-only connection. Do not let edits appear saved before the
    // version-safe EditorController exists (M3).
    control->setEnabled(false);
    control->setToolTip(field.disabledReason.isEmpty()
        ? QStringLiteral("当前为只读查看，原生属性写回尚未启用。") : field.disabledReason);
    return control;
}
}
