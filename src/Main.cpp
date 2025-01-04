#include "Parser.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    std::ios_base::sync_with_stdio(false); // Go fast
    std::cin.tie(nullptr);

    bool makeJSON = false;
    std::string JSONName;
    bool quiet = false;

    if (argc == 1)
    {
        std::cout << "Usage: parser.exe path_to_search (root VSR directory for example) [-j] [filename] (output json) [-q] (quiet)" << '\n';
        return -1;
    }
    else if (argc > 2)
    {
        for (int i = 2; i < argc; i++)
        {
            if (strcmp(argv[i], "-j") == 0)
            {
                makeJSON = true;
                JSONName = argv[++i];
            }
            else if (strcmp(argv[i], "-q") == 0)
            {
                quiet = true;
            }
        }
    }

    std::filesystem::path startPath = argv[1];
    std::vector<MapInfo> maps;

    Parser p(startPath, maps, makeJSON, JSONName, quiet);

	return 0;
}