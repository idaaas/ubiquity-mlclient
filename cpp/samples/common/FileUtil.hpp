#pragma once

#include <fstream>
#include <random>
#include <algorithm>
#include <iostream>
#include <cstring>

namespace Common {

struct CsvData {
    std::string nominal;
    float numerical;
    int8_t categorical;
    int8_t boolean;
};

using CsvDataMap = std::unordered_map<std::string, CsvData>;

static constexpr size_t MAX_LINE_SIZE = 0x10000;
static char cLine[MAX_LINE_SIZE];
static constexpr size_t MAX_FIELDS_COUNT = 0x100;
const char *tokens[MAX_FIELDS_COUNT];

void parseCsvLine(
        const std::string &line,
        const std::vector<std::string> fields,
        CsvDataMap &destination)
{
    if (line.size() + 1 > MAX_LINE_SIZE) {
        return;
    }
    memcpy(cLine, line.c_str(), line.size() + 1);
    if (fields.size() / 2 > MAX_FIELDS_COUNT) {
        return;
    }
    size_t characterIndex = 0;
    for (size_t tokenIndex = 0; tokenIndex < fields.size() / 2 && characterIndex < line.size(); ++tokenIndex) {
        tokens[tokenIndex] = cLine + characterIndex;
        while (0 != cLine[characterIndex] && ',' != cLine[characterIndex]) {
            ++characterIndex;
        }
        cLine[characterIndex] = 0;
        ++characterIndex;
    }
    for (size_t fieldIndex = 0; fieldIndex < fields.size() / 2; ++fieldIndex) {
        if (fields[fieldIndex * 2 + 1] == "nominal") {
            destination[fields[fieldIndex * 2]].nominal = tokens[fieldIndex];
        } else if (fields[fieldIndex * 2 + 1] == "numerical") {
            destination[fields[fieldIndex * 2]].numerical = static_cast<float>(atof(tokens[fieldIndex]));
        } else if (fields[fieldIndex * 2 + 1] == "categorical") {
            destination[fields[fieldIndex * 2]].numerical = atoi(tokens[fieldIndex]);
        } else if (fields[fieldIndex * 2 + 1] == "boolean") {
            destination[fields[fieldIndex * 2]].numerical = std::string(tokens[fieldIndex]) == "true";
        }
    }
}

std::vector<CsvDataMap> loadDataFromCsvFile(
        const std::string &filePath,
        const std::vector<std::string> fields)
{
    std::vector<CsvDataMap> resultVector{};
    std::ifstream fileStream(filePath, std::ios::in);
    if (!fileStream) {
        std::cerr << "Error : Failed to open csv file `" << filePath << "`." << std::endl;
        return resultVector;
    }
    std::string line;
    size_t lineCount = 0;
    while (std::getline(fileStream, line)) {
        resultVector.push_back(CsvDataMap{});
        parseCsvLine(line, fields, resultVector[lineCount]);
        ++lineCount;
    }

    fileStream.close();
    return resultVector;
}

};
