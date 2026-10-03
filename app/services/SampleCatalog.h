#pragma once
#include "domain/VideoSample.h"
namespace qvw::services {
class SampleCatalog {
public:
    // Known pinned outputs plus an optional local schemaVersion 1 catalog.
    // Catalog paths must remain within its directory; no downloads at startup.
    static domain::VideoSamples discover(const QString &distribution, const QString &catalogPath = {});
};
}
