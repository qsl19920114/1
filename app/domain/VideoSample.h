#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
namespace qvw::domain {
struct VideoSample {
    QString name;
    QString path;
    QString sha256;
    QStringList sources;
    bool available=true;
    QString error;
    double durationSeconds = 0;
    int width = 0;
    int height = 0;
};
using VideoSamples=QVector<VideoSample>;
}
