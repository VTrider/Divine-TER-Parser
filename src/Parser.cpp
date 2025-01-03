#include "Parser.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

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
        std::cout << "Binary Save: " << info.binarySave << '\n';
        std::cout << '\n';
    }
}

void Parser::DoTER(const std::filesystem::path& path, MapInfo& info)
{
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
        std::ifstream bzn(path, std::ios::binary);
        if (!bzn)
        {
            std::cerr << "Failed to open file: " << path << '\n';
            return;
        }

        bzn.seekg(72); // Offset to file name is consistent in binary BZNs

        char c;
        std::string fileName;

        const char eot = '\x04';
        while (bzn.get(c) && c != eot)
        {
            fileName += c;
        }
        info.file = fileName;
    }
    else
    {
        std::getline(bzn, line);
        info.file = line.substr(line.find('=') + 2);
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
                float value{};
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
                for (int i = 0; i < 2; i++)
                {
                    std::getline(bzn, line);
                }
            }
            spawnsFound++;
        }
    }
    info.baseToBaseDistance = iDistance2D(firstSpawn, secondSpawn);
}

void Parser::SearchFolder(const std::filesystem::directory_entry& dir, MapInfo& info)
{
    auto mapFolder = dir.path().parent_path();

    for (const auto& file : std::filesystem::directory_iterator(mapFolder))
    {
        DoINF(file.path(), info);
        DoBZN(file.path(), info);
    }
}

void Parser::ParseMaps(const std::filesystem::path& startPath, std::vector<MapInfo>& maps)
{
    for (const auto& dir : std::filesystem::recursive_directory_iterator(startPath))
    {
        if (ToUpper(dir.path().extension().string()) == ".TER")
        {
            MapInfo info{};

            DoTER(dir.path(), info);

            SearchFolder(dir, info);

            maps.push_back(info);
        }
    }
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