#pragma once
#include "domain/InspectorField.h"
class QWidget;
namespace qvw::ui {
QWidget *createInspectorControl(const domain::InspectorField &field, QWidget *parent = nullptr);
}
