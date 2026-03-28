#pragma once

#include <cstddef>
#include <span>

#include "acrp/parse_options.hpp"

namespace acrp {

void exportCsvFiles(std::span<const std::byte> replayBytes, const CsvExportOptions& options);

}  // namespace acrp
