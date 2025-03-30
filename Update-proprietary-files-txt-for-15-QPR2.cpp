#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <regex>

#define DO_MOVE_OVERLAYFS_DIRS
#define DO_MOVE_ELF_FILES_TO_PROPRIETARY_FILES_WITHOUT_CHECKELF

#define IS_MI8937

std::vector<std::string> Split(const std::string& s,
    const std::string& delimiters) {
    std::vector<std::string> result;

    size_t base = 0;
    size_t found;
    while (true) {
        found = s.find_first_of(delimiters, base);
        result.push_back(s.substr(base, found - base));
        if (found == s.npos) break;
        base = found + 1;
    }

    return result;
}

std::string removeSoSuffix(const std::string& filename) {
    std::size_t pos = filename.rfind(".so");
    if (pos != std::string::npos) {
        return filename.substr(0, pos);
    }
    return filename; // Return original if ".so" not found
}

struct FileEntry {
    std::string line;
    std::string sourcePath;
    std::string destPath;
    std::string filename;
    std::string partition;
    std::map<std::string, std::string> params;
    std::string sha1sum;
    bool is_comment = false;
    bool is_elf_file = false;
    bool is_package = false;
#ifdef IS_MI8937
    std::string mi8937_device;
#endif

    FileEntry(const std::string& line) : line(line) {
        parseLine();
    }

    void parseLine() {
        std::string tempLine = line;
        if (tempLine.empty() || tempLine[0] == '#') {
            is_comment = true;
            return;
        }

        if (tempLine[0] == '-') {
            is_package = true;
            tempLine = tempLine.substr(1);
        }

        std::size_t pipePos = tempLine.find('|');
        if (pipePos != std::string::npos) {
            sha1sum = tempLine.substr(pipePos + 1);
            tempLine = tempLine.substr(0, pipePos);
        }

        std::size_t semicolonPos = tempLine.find(';');
        if (semicolonPos != std::string::npos) {
            std::string paramsStr = tempLine.substr(semicolonPos + 1);
            tempLine = tempLine.substr(0, semicolonPos);

            std::stringstream ss(paramsStr);
            std::string param;
            while (std::getline(ss, param, ';')) {
                std::size_t equalsPos = param.find('=');
                if (equalsPos != std::string::npos) {
                    params[param.substr(0, equalsPos)] = param.substr(equalsPos + 1);
                } else {
                    params[param] = "";
                }
            }
        }

        std::size_t colonPos = tempLine.find(':');
        if (colonPos != std::string::npos) {
            sourcePath = tempLine.substr(0, colonPos);
            destPath = tempLine.substr(colonPos + 1);
        } else {
            sourcePath = tempLine;
            destPath = sourcePath;
        }

        std::size_t lastSlashPos = destPath.rfind('/');
        if (lastSlashPos != std::string::npos) {
            filename = destPath.substr(lastSlashPos + 1);
            partition = destPath.substr(0, lastSlashPos);
            partition = partition.substr(0, partition.find('/'));
        } else {
            filename = destPath;
            partition = "";
        }

        // Check if the file is an ELF file based on the directory
        std::size_t secondSlashPos = destPath.find('/', partition.size() + 1);
        if (secondSlashPos != std::string::npos) {
            std::string firstLevelDir = destPath.substr(partition.size() + 1, secondSlashPos - partition.size() - 1);
            if (firstLevelDir == "lib" || firstLevelDir == "lib64" || firstLevelDir == "bin") {
                is_elf_file = true;
            }
        }

#ifdef IS_MI8937
        auto names = Split(destPath, "/");
        auto names_find_overlayfs = std::find(names.begin(), names.end(), "overlayfs");
        if (names_find_overlayfs != names.end()) {
            mi8937_device = *std::next(names_find_overlayfs);
        }

#ifdef DO_MOVE_OVERLAYFS_DIRS
        if ((names[0] == "odm" || names[0] == "vendor") && names[1] == "overlayfs") {
            std::string device = names[2];
            std::string o_dir = names[3];
            std::string o_dir_2, o_fn;
            if (names.size() == 6) {
                o_dir_2 = "/" + names[4];
                o_fn = names[5];
            } else {
                o_fn = names[4];
            }

            std::cout << "mkdir -p vendor/" + o_dir + "/overlayfs/" + device + o_dir_2 << std::endl;
            std::cout << "git mv " << destPath;
            destPath = "vendor/" + o_dir + "/overlayfs/" + device + o_dir_2 + "/" + filename;
            std::cout << " " << destPath << std::endl;
        }
#endif
#endif

#ifdef DO_MOVE_ELF_FILES_TO_PROPRIETARY_FILES_WITHOUT_CHECKELF
        if (is_elf_file) {
            // Modification 1: Mark every ELF files as PACKAGE
            is_package = true;
            // Modification 2: Set DISABLE_DEPS for every ELF files
            params["DISABLE_DEPS"] = "";
        }
#endif
    }

    std::string toString() const {
        if (is_comment) return line;
        std::string result = (is_package ? "-" : "") + sourcePath;
        if (sourcePath != destPath) {
            result += ":" + destPath;
        }
        if (!params.empty()) {
            result += ";";
            for (const auto& pair : params) {
                result += pair.first;
                if (!pair.second.empty()) {
                    result += "=" + pair.second;
                }
                result += ";";
            }
            result.pop_back(); // Remove trailing semicolon
        }
        if (!sha1sum.empty()) {
            result += "|" + sha1sum;
        }
        return result;
    }
};

int getPartitionOrder(const std::string& partition) {
    if (partition == "system") return 0;
    if (partition == "system_ext") return 1;
    if (partition == "product") return 2;
    if (partition == "odm") return 3;
    if (partition == "vendor") return 4;
    return 5; // Other partitions
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <proprietary-files.txt> ...\n";
        return 1;
    }

    std::map<std::string, std::vector<FileEntry*>> fileMap;
    std::vector<std::vector<FileEntry*>> allFileEntries; // Store parsed FileEntry objects

    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i]);
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << argv[i] << std::endl;
            return 1;
        }

        std::vector<FileEntry*> fileEntries;
        std::string line;
        while (std::getline(file, line)) {
            FileEntry* entry = new FileEntry(line);
            fileEntries.push_back(entry);
            if (!entry->filename.empty()) {
                fileMap[entry->filename].push_back(entry);
            }
        }
        allFileEntries.push_back(fileEntries);
        file.close();
    }

#ifdef DO_MOVE_ELF_FILES_TO_PROPRIETARY_FILES_WITHOUT_CHECKELF
    for (auto& [filename, entries] : fileMap) {
        if (entries.size() > 1) {
            std::map<std::string, int> partitionCount;
            for (const auto& entry : entries) {
                partitionCount[entry->partition]++;
            }
            if (partitionCount.size() > 1) { // Only add MODULE_SUFFIX if multiple partitions are present
                std::sort(entries.begin(), entries.end(), [](const FileEntry* a, const FileEntry* b) {
                    return getPartitionOrder(a->partition) < getPartitionOrder(b->partition);
                });

                for (size_t i = 1; i < entries.size(); ++i) {
                    // Modification 3: Set MODULE_SUFFIX=_<Partition> in case of having conflict
                    entries[i]->params["MODULE_SUFFIX"] = "_" + entries[i]->partition;
                }
            }
        }

        std::map<std::string, int> partitionCounts;
        for (const auto& entry : entries) {
            if (!entry->partition.empty()) {
                partitionCounts[entry->partition]++;
            }
        }

        for (auto& entry : entries) {
            if (partitionCounts[entry->partition] > 1) {
                if (entry->is_elf_file) {
                    // Modification 4: Mark ELF files with the same name in the same partition as PACKAGE, for multilib.
                    entry->is_package = true;
                }
            }
#ifdef IS_MI8937
            if (entry->is_package && !entry->mi8937_device.empty()) {
                entry->params["MODULE"] = entry->mi8937_device + "_" + removeSoSuffix(entry->filename);
            }
#endif
        }
    }
#endif

    int fileIndex = 0;
    for (int i = 1; i < argc; ++i) {
        std::ofstream outFile(argv[i]);
        if (!outFile.is_open()) {
            std::cerr << "Error opening file for writing: " << argv[i] << std::endl;
            return 1;
        }

        for (const auto& entry : allFileEntries[fileIndex]) {
            outFile << entry->toString() << std::endl;
        }
        outFile.close();
        fileIndex++;
    }

    // Clean up allocated memory
    for (const auto& fileEntries : allFileEntries) {
        for (const auto& entry : fileEntries) {
            delete entry;
        }
    }

    return 0;
}