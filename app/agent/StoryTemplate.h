#pragma once
#include <QJsonArray>
#include <QString>
namespace qvw::agent {
class StoryTemplate {
public:
    static bool prepare(const QString &installedTemplate,const QJsonArray &scenes,
                        const QString &destination,QString *error);
};
}
