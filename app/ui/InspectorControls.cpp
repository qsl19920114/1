#include "ui/InspectorControls.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QColor>
#include <QColorDialog>
#include <memory>
#include <limits>

namespace qvw::ui {
namespace {
class CompactSpinBox:public QDoubleSpinBox {
public:
    using QDoubleSpinBox::QDoubleSpinBox;
protected:
    QString textFromValue(double value) const override {
        auto text=QDoubleSpinBox::textFromValue(value);
        const auto decimal=locale().decimalPoint();
        if(text.contains(decimal)) {
            while(!text.isEmpty()&&text.back().digitValue()==0)text.chop(1);
            if(text.endsWith(decimal))text.chop(decimal.size());
        }
        return text;
    }
};
}

QWidget *createInspectorControl(const domain::InspectorField &field, QWidget *parent, std::function<void(const QVariant &)> onCommit) {
    bool editable = field.isEditable() && bool(onCommit);
    const auto last = std::make_shared<QVariant>(field.rawValue);
    const auto commit = [onCommit,last](const QVariant &value) {
        if (!onCommit || *last==value) return;
        *last=value; onCommit(value);
    };
    QWidget *control = nullptr;
    switch (field.control) {
    case domain::ControlKind::Boolean: {
        auto *box = new QCheckBox(parent);
        box->setChecked(field.value == "true");
        if (editable) QObject::connect(box,&QCheckBox::toggled,box,[commit](bool value) {
            commit(QVariant(value));
        });
        control = box; break;
    }
    case domain::ControlKind::Number: {
        bool numeric = false; const double value = field.rawValue.isValid() ? field.rawValue.toDouble(&numeric) : field.value.toDouble(&numeric);
        if (numeric) {
            auto *spin = new CompactSpinBox(parent);
            spin->setRange(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
            spin->setDecimals(12); spin->setValue(value); spin->setReadOnly(!editable);
            if (!editable) spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
            else QObject::connect(spin,&QDoubleSpinBox::editingFinished,spin,[commit,spin,lastDisplayed=std::make_shared<double>(spin->value())] {
                if(spin->value()==*lastDisplayed)return;
                *lastDisplayed=spin->value();
                commit(QVariant(spin->value()));
            });
            control = spin;
        } else editable=false;
        break;
    }
    case domain::ControlKind::Select: {
        auto *combo = new QComboBox(parent);
        for (const auto &option : field.options) combo->addItem(option.label, option.value);
        int selected = combo->findData(field.rawValue);
        if (selected < 0) { combo->addItem(field.value, field.rawValue); selected = combo->count() - 1; }
        combo->setCurrentIndex(selected);
        if (editable) QObject::connect(combo,&QComboBox::activated,combo,[commit,combo](int index){commit(combo->itemData(index));});
        control = combo; break;
    }
    case domain::ControlKind::Color: {
        auto *button = new QPushButton(field.value, parent);
        const QColor color(field.value);
        if (color.isValid()) button->setStyleSheet(QStringLiteral("border-left: 18px solid %1; padding: 4px;").arg(color.name()));
        if (editable) QObject::connect(button,&QPushButton::clicked,button,[commit,button] {
            const auto selected=QColorDialog::getColor(QColor(button->text()),button,QStringLiteral("选择主题色"));
            if(selected.isValid()){button->setText(selected.name());commit(selected.name());}
        });
        control = button; break;
    }
    default: break;
    }
    if (!control) {
        auto *line = new QLineEdit(field.value, parent);
        line->setReadOnly(!editable);
        if (editable) QObject::connect(line,&QLineEdit::editingFinished,line,[commit,line]{commit(line->text());});
        control = line;
    }
    control->setEnabled(editable);
    control->setToolTip(field.disabledReason.isEmpty()
        ? (editable ? QStringLiteral("修改后提交，后端确认生效后记录历史。") : QStringLiteral("当前字段或会话不支持编辑。")) : field.disabledReason);
    return control;
}
}
