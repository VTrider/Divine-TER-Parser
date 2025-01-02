#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct MapInfo
{
    std::string name;
    std::string formattedSize;
};

static void SearchFolder(std::filesystem::directory_entry dir, MapInfo& info)
{
    auto mapFolder = dir.path().parent_path();

    for (const auto& mapFile : std::filesystem::directory_iterator(mapFolder))
    {
        if (mapFile.path().extension() == ".INF" || mapFile.path().extension() == ".inf")
        {
            std::ifstream inf(mapFile.path());

            std::string line;

            while (std::getline(inf, line))
            {
                static std::string targetPhrase = "missionName";

                if (line.starts_with(targetPhrase) || line.starts_with("MissionName"))
                {
                    auto formattedName = line.substr(line.find('"'));
                    formattedName.erase(std::remove(formattedName.begin(), formattedName.end() + 1, '"'));
                    info.name = formattedName;
                    break;
                }
            }
            break;
        }
    }
}

static int ParseMaps(std::filesystem::path startPath, std::vector<MapInfo>& maps)
{
    for (const auto& dir : std::filesystem::recursive_directory_iterator(startPath))
    {
        if (dir.path().extension() == ".TER" || dir.path().extension() == ".ter")
        {
            MapInfo info{};

            std::ifstream ter(dir.path(), std::ios::binary);
            if (!ter)
            {
                std::cerr << "File not found" << '\n';
                return -1;
            }
            ter.seekg(0xC);

            std::int16_t size{};
            ter.read(reinterpret_cast<char*>(&size), sizeof(std::int16_t));

            size *= 2; // herppapotamus said this is right
            info.formattedSize = std::format("{}x{}", size, size);

            SearchFolder(dir, info);

            maps.push_back(info);
        }
    }
}

int main()
{
    std::string path;
    std::cout << "Paste the root VSR directory here or anywhere that stores the map files" << '\n';
    std::cin >> std::ws;
    std::getline(std::cin, path);

    std::filesystem::path startPath = path;

    std::vector<MapInfo> maps;

    try
    {
        ParseMaps(startPath, maps);
    }
    catch (const std::exception& e)
    {
        std::cerr << "You fked up!" << '\n' << e.what() << '\n';
    }
    
    std::cout << std::endl;

    for (const auto& info : maps)
    {
        std::cout << "Map: " << info.name << '\n';
        std::cout << "Size (m): " << info.formattedSize << '\n';
        std::cout << std::endl;
    }

    std::cin.get();

	return 0;
}