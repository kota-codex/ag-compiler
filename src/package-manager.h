#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

std::optional<std::string> read_file(std::string file_name);

struct Repo {
    struct Module {
        std::string path;
        bool is_directory;
        int64_t version;
        std::string name;
    };
    std::unordered_map<std::string, Module> modules;
    std::string url;
    std::string path;
};

struct Depot {
    std::vector<Repo> repos;
    std::string triplet;
    std::string debug_release;
    std::stringstream links;
    std::stringstream deps;

    void init(std::string triplet, bool debug_release);

    void add(std::string address);  // "directory::url"
    std::string read_source(std::string moduleName, int64_t& version, std::string& out_path);
    std::optional<std::string> import_module(Repo::Module& m, std::stringstream& out_messages);
};
