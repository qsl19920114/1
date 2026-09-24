#include "domain/InspectorField.h"

namespace qvw::domain {

ControlKind controlKindFromKey(const QString &key) {
    if (key == QStringLiteral("text")) return ControlKind::Text;
    if (key == QStringLiteral("number")) return ControlKind::Number;
    if (key == QStringLiteral("boolean")) return ControlKind::Boolean;
    if (key == QStringLiteral("select")) return ControlKind::Select;
    if (key == QStringLiteral("color")) return ControlKind::Color;
    return ControlKind::Unsupported;
}

QString controlKindLabel(ControlKind kind) {
    switch (kind) {
    case ControlKind::Text: return QStringLiteral("文本");
    case ControlKind::Number: return QStringLiteral("数值");
    case ControlKind::Boolean: return QStringLiteral("开关");
    case ControlKind::Select: return QStringLiteral("选项");
    case ControlKind::Color: return QStringLiteral("颜色");
    case ControlKind::Unsupported: break;
    }
    return QStringLiteral("暂不支持");
}

bool InspectorField::isEditable() const {
    return writable && control != ControlKind::Unsupported;
}

}
