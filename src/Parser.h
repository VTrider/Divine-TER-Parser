#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct MapInfo
{
    std::string name;
    std::string file;
    std::uint32_t size;
    std::string formattedSize;
    std::uint32_t baseToBaseDistance;
    bool binarySave;
};

struct Vec3
{
    float x;
    float y;
    float z;
};

inline std::uint32_t iDistance2D(const Vec3& v, const Vec3& w)
{
    return std::lround(std::sqrtf(std::powf(w.x - v.x, 2) + std::powf(w.z - v.z, 2)));
}

class Parser
{
private:
    bool m_makeJSON = false;
    std::string m_JSONName;
    bool m_quiet = false;

    // Helpers
    std::string ToUpper(const std::string& str);
    void MakeJSON(const std::vector<MapInfo>& maps, const std::string& outFile);
    void OutputText(const std::vector<MapInfo>& maps);
    

    void DoTER(const std::filesystem::path& path, MapInfo& info);
    void DoINF(const std::filesystem::path& path, MapInfo& info);
    void DoBZN(const std::filesystem::path& path, MapInfo& info);
    void SearchFolder(const std::filesystem::directory_entry& dir, MapInfo& info);
    void ParseMaps(const std::filesystem::path& startPath, std::vector<MapInfo>& maps);

public:
    Parser(const std::filesystem::path& startPath, std::vector<MapInfo>& maps, bool makeJSON, const std::string& JSONName, bool quiet);
    Parser(const Parser& p) = delete;
    Parser(const Parser&& p) = delete;
};