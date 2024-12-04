/*
 * Added for feeding infomation for traces
 * Edited by : hw park 
 * Date : 2024.12.03
 * email : hwpark@dgist.ac.kr
*/

#ifndef TRACE_FEEDER_H
#define TRACE_FEEDER_H

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <variant>
#include <regex>
#include <optional>

namespace champsim
{
    class tracefeeder
    {
        private:
            std::vector<std::string> filePaths;
            std::vector<std::unordered_map<uint64_t, std::tuple<uint64_t, uint64_t, uint64_t, std::bitset<64>>>> data;

        public:
            tracefeeder() = default;
            tracefeeder(const std::vector<std::string> fPaths) : filePaths(fPaths) {}
           
           bool readCSV() {
                data.clear(); // Clear existing data

                for (const auto& filePath : filePaths) {
                    std::ifstream file(filePath);
                    if (!file.is_open()) {
                        std::cerr << "Error: File not found: " << filePath << std::endl;
                        return false;
                    }

                    std::string line;

                    // Read the first line to get column names (ignored as we know the column names)
                    if (!std::getline(file, line)) {
                        std::cerr << "Error: Empty file or unable to read column names." << std::endl;
                        return false;
                    }

                    std::unordered_map<uint64_t, std::tuple<uint64_t, uint64_t, uint64_t, std::bitset<64>>> fileData;

                    // Read the rest of the data
                    while (std::getline(file, line)) {
                        std::stringstream lineStream(line);
                        std::string cell;

                        uint64_t vfn, pfn, hits, prefetchs;
                        std::bitset<64> hit_bits_accumulated;

                        // Read vfn (hex to uint64_t)
                        if (std::getline(lineStream, cell, ',') && cell.find("0x") == 0) {
                            vfn = std::stoull(cell, nullptr, 16);
                        } else {
                            std::cerr << "Error: Invalid vfn format." << std::endl;
                            continue;
                        }

                        // Read pfn (hex to uint64_t)
                        if (std::getline(lineStream, cell, ',') && cell.find("0x") == 0) {
                            pfn = std::stoull(cell, nullptr, 16);
                        } else {
                            std::cerr << "Error: Invalid pfn format." << std::endl;
                            continue;
                        }

                        // Read hits (decimal to uint64_t)
                        if (std::getline(lineStream, cell, ',') && !cell.empty()) {
                            hits = std::stoull(cell);
                        } else {
                            std::cerr << "Error: Invalid hits format." << std::endl;
                            continue;
                        }

                        // Read prefetchs (decimal to uint64_t)
                        if (std::getline(lineStream, cell, ',') && !cell.empty()) {
                            prefetchs = std::stoull(cell);
                        } else {
                            std::cerr << "Error: Invalid prefetchs format." << std::endl;
                            continue;
                        }

                        // Read hit_bits_accumulated (binary string to std::bitset<64>)
                        if (std::getline(lineStream, cell, ',') && !cell.empty() && cell.length() <= 65) {
                            hit_bits_accumulated = std::bitset<64>(cell);
                        } else {
                            std::cerr << "Error: Invalid hit_bits_accumulated format." << std::endl;
                            continue;
                        }

                        // Store the row with vfn as the key
                        fileData[vfn] = std::make_tuple(pfn, hits, prefetchs, hit_bits_accumulated);
                    }
                    file.close();

                    // Store the data for this file
                    data.push_back(fileData);
                }
                return true;
            }

            // Function to find a row given feed index and vfn key
            std::optional<std::tuple<uint64_t, uint64_t, uint64_t, std::bitset<64>>>
            find(size_t feed_idx, uint64_t vaddr) {
                uint64_t vfn = vaddr >> 12; // vfn is the vaddr shifted right by 12 bits
                // Check if feed index is valid
                if (feed_idx >= data.size()) {
                    std::cerr << "Error: Invalid feed index." << std::endl;
                    return std::nullopt;
                }

                const auto& fileData = data[feed_idx];

                // Find the row with the given vfn key
                auto it = fileData.find(vfn);
                if (it == fileData.end()) {
                    // std::cerr << "Error: vfn not found." << std::endl;
                    return std::nullopt;
                }

                return it->second;
            }

            void printData() {
                for (size_t i = 0; i < data.size(); ++i) {
                    std::cout << "Data from file: " << filePaths[i] << std::endl;
                    for (const auto& [vfn, row] : data[i]) {
                        const auto& [pfn, hits, prefetchs, hit_bits_accumulated] = row;
                        std::cout << "vfn: 0x" << std::hex << vfn << ", pfn: 0x" << pfn
                                  << ", hits: " << std::dec << hits
                                  << ", prefetchs: " << prefetchs
                                  << ", hit_bits_accumulated: " << hit_bits_accumulated << std::endl;
                    }
                    std::cout << "---------------------------------" << std::endl;
                }
            }
    };
}

#endif // TRACE_FEEDER_H
