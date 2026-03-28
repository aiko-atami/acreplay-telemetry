#pragma once

#include <string>

namespace acrp {

struct CsvExportOptions {
  std::string outputPath;
  std::string driverName;
  std::string inputStem;
};

}  // namespace acrp
