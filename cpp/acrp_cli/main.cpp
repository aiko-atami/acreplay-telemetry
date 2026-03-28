#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "acrp/csv_export.hpp"
#include "acrp/status.hpp"

namespace {

int noInputError() {
  std::fputs("An input file is required.\nUse --help for a list of all options.\n", stderr);
  return EXIT_FAILURE;
}

std::vector<std::byte> readFileBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw acrp::ParseError(acrp::StatusCode::io_error, "Failed to open replay file");
  }
  input.seekg(0, std::ios::end);
  const auto size = input.tellg();
  input.seekg(0, std::ios::beg);

  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(bytes.data()), size);
  if (!input) {
    throw acrp::ParseError(acrp::StatusCode::io_error, "Failed to read replay file");
  }
  return bytes;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    return noInputError();
  }

  std::vector<std::string> inputPaths;
  std::string outputPath;
  std::string driverName;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "-help") == 0 || std::strcmp(argv[i], "--help") == 0) {
      std::puts(
          "Assetto Corsa Replay Parser\n"
          "Usage: acrp_cli [OPTIONS] [INPUT FILE(S)]\n\n"
          "with options:\n"
          "-o, --output PATH\n\tOutput path with optional file name.\n"
          "\tDefault is \"<input-filename>.csv\" in the current directory.\n\n"
          "--driver-name NAME\n\tName of driver whose car is to be parsed.\n"
          "\tParses all cars if unspecified.");
      return EXIT_SUCCESS;
    }
    if (std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--output") == 0) {
      if (i + 1 >= argc) {
        std::fputs("--output option requires one argument.\n", stderr);
        return EXIT_FAILURE;
      }
      outputPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--driver-name") == 0) {
      if (i + 1 >= argc) {
        std::fputs("--driver-name option requires one argument.\n", stderr);
        return EXIT_FAILURE;
      }
      driverName = argv[++i];
      continue;
    }
    inputPaths.emplace_back(argv[i]);
  }

  if (inputPaths.empty()) {
    return noInputError();
  }

  try {
    for (std::size_t index = 0; index < inputPaths.size(); ++index) {
      const auto path = std::filesystem::path(inputPaths[index]);
      auto bytes = readFileBytes(path);
      acrp::CsvExportOptions options{
          .outputPath = outputPath.empty() ? "." : outputPath,
          .driverName = driverName,
          .inputStem = path.stem().string(),
      };
      acrp::exportCsvFiles(bytes, options);
      if (index + 1 < inputPaths.size()) {
        std::puts("-----------------------------");
      }
    }
  } catch (const acrp::ParseError& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
