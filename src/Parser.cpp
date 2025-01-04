#include "Parser.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_set>
#include <thread>

#define MULTITHREAD

// Why tf isn't this in the standard library ._.
std::string Parser::ToUpper(const std::string& str)
{
    std::string newStr = str;
    std::transform(newStr.begin(), newStr.end(), newStr.begin(), ::toupper);
    return newStr;
}

void Parser::MakeJSON(const std::vector<MapInfo>& maps, const std::string& outFile)
{
    nlohmann::ordered_json jsonData;
    for (const auto& map : maps)
    {
        jsonData.push_back({ { "name", map.name },
                             { "file", map.file },
                             { "formattedSize", map.formattedSize },
                             { "size", map.size },
                             { "baseToBase", map.baseToBaseDistance },
                             { "binarySave", map.binarySave } });
    }

    std::ofstream file(outFile);
    if (file)
    {
        file << jsonData.dump(4); // Pretty print with indentation
    }
}

void Parser::OutputText(const std::vector<MapInfo>& maps)
{
    for (const auto& info : maps)
    {
        std::cout << "Map: " << info.name << '\n';
        std::cout << "File: " << info.file << '\n';
        std::cout << "Formatted Size (m): " << info.formattedSize << '\n';
        std::cout << "Size (m): " << info.size << '\n';
        std::cout << "Base to Base Distance (m): " << info.baseToBaseDistance << '\n';
        std::cout << "Binary Save: " << ((info.binarySave == true) ? "true" : "false") << '\n';
        std::cout << '\n';
    }
}

// Deletes anomalous map detections from stray files that get through the
// other filters
void Parser::CleanMaps(std::vector<MapInfo>& maps)
{
    std::vector<MapInfo> temp = maps;
    std::vector<MapInfo> cleanedMaps;
    for (const auto& map : temp)
    {
        if (map.mapFiles.size() < 6)
        {
            continue;
        }
        cleanedMaps.push_back(map);
    }
    maps = cleanedMaps;
}

void Parser::DoINF(const std::filesystem::path& path, MapInfo& info)
{
    if (!(ToUpper(path.extension().string()) == ".INF"))
    {
        return;
    }

    std::ifstream inf(path);

    std::string line;

    while (std::getline(inf, line))
    {
        static const std::string targetPhrase = "MISSIONNAME";

        if (ToUpper(line).starts_with(targetPhrase))
        {
            auto formattedName = line.substr(line.find('"'));
            formattedName.erase(std::remove(formattedName.begin(), formattedName.end(), '"'), formattedName.end());

            info.name = formattedName;
            break;
        }
    }
}

void Parser::DoTER(const std::filesystem::path& path, MapInfo& info)
{
    if (!(ToUpper(path.extension().string()) == ".TER"))
    {
        return;
    }

    std::ifstream ter(path, std::ios::binary);
    if (!ter)
    {
        std::cerr << "File not found" << '\n';
        return;
    }
    ter.seekg(0xC); // Offset of the TER size data

    std::int16_t size{};
    ter.read(reinterpret_cast<char*>(&size), sizeof(std::int16_t));
    
    size *= 2; // herppapotamus said this is right
    info.size = size;
    info.formattedSize = std::format("{}x{}", size, size);
}

// This is too much voodoo
void Parser::DoBZN(const std::filesystem::path& path, MapInfo& info)
{
    if (!(ToUpper(path.extension().string()) == ".BZN"))
    {
        return;
    }

    std::ifstream bzn(path);

    std::string line;

    int spawnsFound = 0;
    Vec3 firstSpawn{};
    Vec3 secondSpawn{};

    for (int i = 0; i < 6; i++)
    {
        std::getline(bzn, line);
    }

    info.binarySave = (line == "true") ? true : false;

    if (info.binarySave == true)
    {
        bzn.close();
        std::ifstream binaryBZN(path, std::ios::binary);
        if (!binaryBZN)
        {
            std::cerr << "Failed to open file: " << path << '\n';
            return;
        }

        binaryBZN.seekg(71); // Offset to file name is consistent in binary BZNs

        char c;
        std::string fileName;

        static constexpr char eot = '\x04';
        while (binaryBZN.get(c) && c != eot)
        {
            fileName += c;
        }
        info.file = fileName.substr(0, fileName.length() - 4);
    }
    else
    {
        std::getline(bzn, line);
        line = line.substr(line.find('=') + 2);
        info.file = line.substr(0, line.length() - 4);
    }

    // If you got more than two spawns ur out of luck!
    while (std::getline(bzn, line) && spawnsFound < 2)
    {
        static const std::string targetPhrase = "OBJCLASS = PSPWN_1";
        if (ToUpper(line).starts_with(targetPhrase))
        {
            Vec3* p_spawn = &firstSpawn;
            if (spawnsFound == 1)
            {
                p_spawn = &secondSpawn;
            }

            // Seek to the position of the spawn
            while (std::getline(bzn, line))
            {
                static const std::string positionPhrase = "  POSIT.X [1] =";
                if (ToUpper(line).starts_with(positionPhrase))
                {
                    std::getline(bzn, line);
                    break;
                }
            }

            for (int i = 0; i < 3; i++)
            {
                std::istringstream s(line); // Converts the scientific notation string into a float
                switch (i)
                {
                case 0:
                    s >> p_spawn->x;
                    break;
                case 1:
                    s >> p_spawn->y;
                    break;
                case 2:
                    s >> p_spawn->z;
                    break;
                }
                
                // The data is every other line
                for (int j = 0; i < 2; j++)
                {
                    std::getline(bzn, line);
                }
            }
            spawnsFound++;
        }
    }
    info.baseToBaseDistance = iDistance2D(firstSpawn, secondSpawn);
}

Parser::folderStatus Parser::SearchFolder(const std::filesystem::directory_entry& dir, std::vector<MapInfo>& foundMaps)
{
    auto mapFolder = dir.path().parent_path(); // Path right now is to the TER

    std::unordered_set<std::string> foundStems;
    std::vector<std::filesystem::path> folderFiles;

    // Identify unique maps if feukers like Aegeis put multiple in one folder
    for (const auto& file : std::filesystem::directory_iterator(mapFolder))
    {
        if (file.is_directory())
        {
            continue;
        }
        if (!mapFileExtensions.contains(ToUpper(file.path().extension().string())))
        {
            continue;
        }

        if (!foundStems.contains(ToUpper(file.path().stem().string())))
        {
            MapInfo info{};
            auto stem = file.path().stem();
            info.mapFileStem = ToUpper(stem.string());

            foundStems.insert(ToUpper(stem.string()));
            foundMaps.push_back(info);
        }
        folderFiles.push_back(file.path());
    }

    // Sort files by their corresponding map
    for (auto& map : foundMaps)
    {
        for (const auto& file : folderFiles)
        {
            if (map.mapFileStem == ToUpper(file.stem().string()))
            {
                map.mapFiles.push_back(file);
            }
        }
    }

    // Finally parse the map data
    for (auto& info : foundMaps)
    {
        for (const auto& file : info.mapFiles)
        {
            DoINF(file, info);
            DoTER(file, info);
            DoBZN(file, info);
        }
    }

    using enum folderStatus;

    return (foundMaps.size() > 1) ? multipleFound : singleFound;
}

void Parser::ParseMaps(const std::filesystem::path& startPath, std::vector<MapInfo>& maps)
{
#ifdef MULTITHREAD
    std::vector<std::thread> threads;
    std::mutex mapsMutex;
    for (const auto& dir : std::filesystem::recursive_directory_iterator(startPath))
    {
        static std::filesystem::path pathToIgnore;

        // Hack to go up a directory if the current folder has already been processed
        if (dir.path().parent_path() == pathToIgnore)
        {
            continue;
        }

        if (ToUpper(dir.path().extension().string()) == ".TER")
        {
            threads.emplace_back([this, dir, &maps, &mapsMutex]()
                {
                    std::vector<MapInfo> foundMaps;

                    folderStatus status = SearchFolder(dir, foundMaps);

                    {
                        std::lock_guard<std::mutex> lock(mapsMutex);
                        maps.insert(maps.end(), foundMaps.begin(), foundMaps.end());

                        if (status == folderStatus::multipleFound)
                        {
                            pathToIgnore = dir.path().parent_path();
                        }
                    }
                });
        }
    }

    for (auto& t : threads)
    {
        t.join();
    }
#endif

#ifdef SINGLETHREAD
    // For some reason using the iterator directly caused access violation so I have to
    // use range based for loop
    for (const auto& dir : std::filesystem::recursive_directory_iterator(startPath))
    {
        static std::filesystem::path pathToIgnore;

        // Hack to go up a directory if the current folder has already been processed
        if (dir.path().parent_path() == pathToIgnore)
        {
            continue;
        }

        if (ToUpper(dir.path().extension().string()) == ".TER")
        {
            std::vector<MapInfo> foundMaps;

            folderStatus status = SearchFolder(dir, foundMaps);

            maps.insert(maps.end(), foundMaps.begin(), foundMaps.end());

            if (status == folderStatus::multipleFound)
            {
                pathToIgnore = dir.path().parent_path();
            }
        }
    }
#endif

    CleanMaps(maps);

    std::sort(maps.begin(), maps.end(), [](const MapInfo& a, const MapInfo& b)
        {
            return a.name < b.name; // Alphabetical
        });
}

Parser::Parser(const std::filesystem::path& startPath, std::vector<MapInfo>& maps, bool makeJSON, const std::string& JSONName, bool quiet)
    : m_makeJSON(makeJSON), m_JSONName(JSONName), m_quiet(quiet)
{
    try
    {
        ParseMaps(startPath, maps);

        if (m_makeJSON)
        {
            MakeJSON(maps, m_JSONName);
        }
        if (!m_quiet)
        {
            OutputText(maps);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "You fked up!" << '\n' << e.what() << '\n';
    }
}