#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

#include "acrp/api.hpp"

namespace {

std::vector<std::byte> readFileBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Failed to open replay file");
  }
  input.seekg(0, std::ios::end);
  const auto size = input.tellg();
  input.seekg(0, std::ios::beg);

  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(bytes.data()), size);
  if (!input) {
    throw std::runtime_error("Failed to read replay file");
  }
  return bytes;
}

void writeFileBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    throw std::runtime_error("Failed to open output file");
  }
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output) {
    throw std::runtime_error("Failed to write output file");
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: export_pack <input.acreplay> <output.pack> <car_index> <lap_index>\n";
    return 1;
  }

  const auto replayBytes = readFileBytes(argv[1]);
  const auto carIndex = static_cast<std::uint32_t>(std::stoul(argv[3]));
  const auto lapIndex = static_cast<std::size_t>(std::stoul(argv[4]));
  const auto parsedCar = acrp::parseCar(replayBytes, carIndex);
  if (lapIndex >= parsedCar.lapPacks.size()) {
    throw std::runtime_error("Lap index is out of range");
  }

  writeFileBytes(argv[2], parsedCar.lapPacks[lapIndex].bytes);
  std::cout << "exported lap pack " << lapIndex << " for car " << carIndex << " to " << argv[2]
            << '\n';
  return 0;
}
