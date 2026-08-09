#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

void processLogFile(const std::string& logFilePath, std::ofstream& csvFile) {
    std::ifstream logFile(logFilePath);
    std::string line;

    while (std::getline(logFile, line)) {
        size_t found = line.find("DeadPagePercentage");
        if (found != std::string::npos) {
            // Extract filename
            std::string filename = fs::path(logFilePath).filename().string();
            csvFile << filename << ",";

            // Extract values after DeadPagePercentage
            std::getline(logFile, line);
            std::istringstream iss(line);
            std::vector<std::string> values;
            std::string value;
            while (iss >> value) {
                values.push_back(value);
            }

            // Write the array of values to the CSV file
            for (const auto& v : values) {
                csvFile << v << ",";
            }
            csvFile << std::endl;

            break;  // Assuming you want to stop after finding the first occurrence
        }
    }

    logFile.close();
}

int main() {
    std::ofstream csvFile("output.csv");
    csvFile << "Filename,DeadPagePercentage Values" << std::endl;  // CSV header

    // Specify the folder containing log files
    std::string folderPath = "/home/waqar/MappingTableWork/mappingWrokStats/hcftl/DeadLRUVsLRU/Comparison3/lruOnly_65568/";

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            processLogFile(entry.path().string(), csvFile);
        }
    }

    csvFile.close();
    return 0;
}
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

void processLogFile(const std::string& logFilePath, std::ofstream& csvFile) {
    std::ifstream logFile(logFilePath);
    std::string line;

    while (std::getline(logFile, line)) {
        size_t found = line.find("DeadPagePercentage");
        if (found != std::string::npos) {
            // Extract filename
            std::string filename = fs::path(logFilePath).filename().string();
            csvFile << filename << ",";

            // Extract values after DeadPagePercentage
            std::getline(logFile, line);
            std::istringstream iss(line);
            std::vector<std::string> values;
            std::string value;
            while (iss >> value) {
                values.push_back(value);
            }

            // Write the array of values to the CSV file
            for (const auto& v : values) {
                csvFile << v << ",";
            }
            csvFile << std::endl;

            break;  // Assuming you want to stop after finding the first occurrence
        }
    }

    logFile.close();
}

int main() {
    std::ofstream csvFile("output.csv");
    csvFile << "Filename,DeadPagePercentage Values" << std::endl;  // CSV header

    // Specify the folder containing log files
    std::string folderPath = "/home/waqar/MappingTableWork/mappingWrokStats/hcftl/DeadLRUVsLRU/Comparison3/lruOnly_65568/";

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            processLogFile(entry.path().string(), csvFile);
        }
    }

    csvFile.close();
    return 0;
}
