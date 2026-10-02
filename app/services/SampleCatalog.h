#pragma once
#include "domain/VideoSample.h"
namespace qvw::services {
class SampleCatalog {
public:
    // Only known local sample outputs from the pinned external distribution.
    static domain::VideoSamples discover(const QString &distribution);
};
}
