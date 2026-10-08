#pragma once

// Build metadata only. Runtime configuration does not participate in versioning.
#define EIEM_VERSION_MAJOR 1
#define EIEM_VERSION_MINOR 3
#define EIEM_VERSION_PATCH 2
#define EIEM_STRINGIFY2(x) #x
#define EIEM_STRINGIFY(x) EIEM_STRINGIFY2(x)
#define EIEM_VERSION EIEM_STRINGIFY(EIEM_VERSION_MAJOR) "." EIEM_STRINGIFY(EIEM_VERSION_MINOR) "." EIEM_STRINGIFY(EIEM_VERSION_PATCH)
