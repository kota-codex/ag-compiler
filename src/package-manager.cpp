#include "package-manager.h"

#include <iostream>
#include <fstream>
#include <optional>
#include <string>
#include <sstream>
#include <vector>
#include <filesystem>
#include <regex>
#include <charconv>
#include <functional>

#include "llvm/TargetParser/Triple.h"
#include "curl/curl.h"
#include "zip.h"
#include "ast.h"

namespace {
    std::regex file_re(R"(^([A-Za-z0-9]+)(?:-([0-9]+))?\.ag$)");
    std::regex dir_re(R"(^([A-Za-z0-9]+)(?:-([0-9]+))?$)");
}
using std::optional;
using std::nullopt;
using std::string;
using std::vector;
using std::function;
using std::unordered_map;
using std::stringstream;
namespace fs = std::filesystem;

[[noreturn]] void panic();

struct Curl {
    CURL* curl;
    Curl() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
    }
    ~Curl() {
        if (curl)
            curl_easy_cleanup(curl);
        curl_global_cleanup();
    }
    vector<char> load(const string& url) {
        vector<char> result;
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size, size_t nmemb, void* userp) {
                size_t total = size * nmemb;
                auto& buffer = *reinterpret_cast<vector<char>*>(userp);
                auto data = reinterpret_cast<char*>(contents);
                buffer.insert(buffer.end(), data, data + total);
                return total;
            });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
            CURLcode code = curl_easy_perform(curl);
            if (code != CURLE_OK) {
                std::cerr << "Download failed: " << curl_easy_strerror(code) << "\n";
            }
        }
        return result;
    }
};
Curl curl;

struct Zip {
    std::vector<char> data;
    zip_source_t* src = nullptr;
    zip_t* zip = nullptr;
    Zip(std::vector<char> data) :data(move(data)) {
        zip_error_t err{};
        src = zip_source_buffer_create(data.data(), data.size(), 0, &err);
        if (!src) {
            std::cerr << "zip_source_buffer_create failed " << zip_error_strerror(&err) << "\n";
            return;
        }
        zip = zip_open_from_source(src, 0, &err);
        if (!zip) {
            std::cerr << "zip_open_from_source failed " << zip_error_strerror(&err) << "\n";
            return;
        }
    }
    ~Zip() {
        if (zip)
            zip_close(zip);
        if (src)
            zip_source_free(src);
    }
    template<typename Fn>
    void for_each(Fn fn) {
        if (!zip)
            return;
        zip_uint64_t num_entries = zip_get_num_entries(zip, 0);
        for (zip_uint64_t i = 0; i < num_entries; i++) {
            struct zip_stat st;
            if (zip_stat_index(zip, i, 0, &st) == 0) {
                if (!fn(st))
                    break;
            }
        }
    }
    optional<vector<char>> read(const struct zip_stat& st) {
        std::vector<char> buffer(st.size);
        zip_file_t* zf = zip_fopen_index(zip, st.index, 0);
        if (!zf) {
            std::cerr << "Failed to open file in zip: " << st.name << "\n";
            return nullopt;
        }
        for (zip_uint64_t pos = 0; pos < buffer.size();) {
            zip_int64_t bytes_read = zip_fread(zf, buffer.data() + pos, buffer.size() - pos);
            if (bytes_read <= 0) {
                std::cerr << "Failed to read file in zip: " << st.name << "\n";
                zip_fclose(zf);
                return nullopt;
            }
            pos += bytes_read;
        }
        zip_fclose(zf);
        return buffer;
    }
};

optional<int64_t> unzip_from_memory(std::vector<char> data, const std::string& out_dir, const std::string& package) {
    optional<int64_t> version = nullopt;
    string base_path;
    Zip zip(move(data));
    zip.for_each([&](struct zip_stat& f) {
        if (!version) {
            static const std::regex re(R"(^([a-zA-Z0-9]+)-(\d+)/)");
            std::cmatch m;
            if (!std::regex_match(f.name, m, re) || package != m[2]) {
                std::cerr << "Downloaded zip does not contain folder " << package << "-version\n";
                return false;
            }
            version = atoll(m[2].str().c_str());
            base_path = ast::format_str(m[1].str(), '-', *version);
        } else if (*fs::path(f.name).begin() != base_path) {
            std::cerr << "Downloaded zip contains multiple root folders";
            version = nullopt;
            return false;
        }
        std::string out_path = out_dir + "/" + f.name;
        if (!out_path.empty() && out_path.back() == '/') {
            std::filesystem::create_directories(out_path);
        } else {
            std::filesystem::create_directories(std::filesystem::path(out_path).parent_path());
            auto buffer = zip.read(f);
            if (!buffer) {
                version = nullopt;
                return false;
            }
            std::ofstream(out_path, std::ios::binary).write((*buffer).data(), (*buffer).size());
        }
        return true;
    });
    if (!version) {
        if (!base_path.empty())
            fs::remove_all(ast::format_str(out_dir, '/', base_path));
    }
    return version;
}

optional<string> read_file(string file_name) {
    std::ifstream f(file_name, std::ios::binary | std::ios::ate);
    if (f) {
        std::string r(f.tellg(), '\0');
        f.seekg(0, std::ios::beg);
        f.read(r.data(), r.size());
        return r;
    } else {
        return nullopt;
    }
}

void Depot::init(string llvm_triplet, bool is_debug) {
    llvm::Triple t(llvm_triplet);
    using namespace llvm;
    std::string arch;
    switch (t.getArch()) {
    case Triple::ArchType::x86_64:  arch = "x64"; break;
    case Triple::ArchType::aarch64: arch = "arm64"; break;
    case Triple::ArchType::wasm64:  arch = "wasm64"; break;
    case Triple::ArchType::riscv64: arch = "riscv64"; break;
    default: arch = "unsupported_" + t.getArchName().str(); break;
    }
    std::string os;
    switch (t.getOS()) {
    case Triple::OSType::Win32:   os = "windows"; break;
    case Triple::OSType::Linux:   os = "linux"; break;
    case Triple::OSType::Darwin:  os = "osx"; break;
    case Triple::OSType::FreeBSD: os = "freebsd"; break;
    case Triple::OSType::IOS:     os = "ios"; break;
    case Triple::OSType::Emscripten: os = "emscripten"; break;
    default: os = "unsupported_" + t.getOSName().str(); break;
    }
    std::string suffix;
    switch (t.getEnvironment()) {
    case Triple::GNU:     suffix = "-gnu"; break;
    case Triple::Android: os = "android"; break;
    default: break;
    }
    this->vcpkg_triplet = arch + "-" + os + suffix;
    this->debug_release = is_debug ? "Debug" : "Release";
}

void Depot::add(string address) {
    repos.push_back(Repo());
    auto& r = repos.back();
    r.path = move(address);
    if (auto pos = r.path.find("::"); pos != std::string::npos) {
        r.url = r.path.substr(pos + 2);
        r.path.resize(pos);
    }
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(r.path, ec)) {
        if (ec) {
            std::cerr << "Error reading dir entry in " << r.path << ": " << ec.message() << "\n";
            continue;
        }
        string name = entry.path().filename().string();
        std::smatch match;
        if (std::regex_match(
            name,
            match,
            entry.is_regular_file() ? file_re : dir_re))
        {
            std::string name = match[1];
            auto version = atoll(match[2].str().c_str()); // 0 if none
            Repo::Module& m = r.modules[name];
            if (version < m.version)
                continue;
            m.version = version;
            m.path = entry.path().string();
            m.is_directory = entry.is_directory();
            m.name = name;
        }
    }
    if (ec)
        std::cerr << "Error opening " << r.path << ": " << ec.message() << "\n";
}

optional<string> Depot::import_module(Repo::Module& m, stringstream& out_messages) {
    if (!m.is_directory) {
        auto r = read_file(m.path);
        if (!r)
            out_messages << "Can't read :" << m.path << "\n";
        return r;
    }
    unordered_map<string, fs::path> content;
    function<bool(const string&, const string&, bool)> scan_dir = [&](const string& path, const string& my_triple, bool search_deb_rel) {
        string tripple_dir, deb_rel_dir;
        bool has_dirs = false;
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(path, ec)) {
            if (ec) {
                std::cerr << "Error reading dir entry in " << path << ": " << ec.message() << "\n";
                return false;
            }
            if (*entry.path().filename().generic_string().c_str() == '-')
                continue;
            if (entry.is_regular_file()) {
                content[entry.path().filename().generic_string()] = entry.path();
            } else {
                has_dirs = true;
                auto name = entry.path().filename().generic_string();
                if (search_deb_rel && name == debug_release)
                    deb_rel_dir = entry.path().generic_string();
                else if (!my_triple.empty()) {
                    if (my_triple == name)
                        tripple_dir = entry.path().generic_string();
                }
            }
        }
        if (ec)
            std::cerr << "Error opening dir " << path << ": " << ec.message() << "\n";
        if (!deb_rel_dir.empty() && !scan_dir(deb_rel_dir, "", false))
            return false; // deb_rel dir has unwanted subdirs
        if (!tripple_dir.empty() && !scan_dir(tripple_dir, "", true))
            return false;
        if (has_dirs && deb_rel_dir.empty() && tripple_dir.empty()) {
            out_messages << path << " has dirs";
            if (search_deb_rel || !my_triple.empty()) {
                out_messages << " but no matching "
                    << (search_deb_rel ? debug_release : "")
                    << (search_deb_rel && !my_triple.empty() ? " or " : "")
                    << my_triple;
            }
            out_messages << "\n";
            return false;
        }
        return true;
    };
    if (!scan_dir(m.path, vcpkg_triplet, true))
        return nullopt;
    auto agi = content.find(m.name + ".ag");
    if (agi == content.end()) {
        out_messages << "no " << m.name << ".ag in " << m.path << std::endl;
        return nullopt;
    }
    auto r = read_file(agi->second.generic_string());
    if (!r) {
        out_messages << "Can't read :" << agi->second << std::endl;
        return nullopt;
    }
    content.erase(agi);
    for (const auto& i : content) {
        deps.push_back(i.second.string());
    }
    return r;
}

std::tuple<string, string> Depot::get_links_and_deps() {
    std::stringstream links;
    std::stringstream depends;
    deps.reverse();
    for (auto s : deps) {
        auto ext = std::filesystem::path(s).extension().string();
        auto& stream = ext == ".lib" || ext == ".a" ? links : depends;
        if (!stream.str().empty())
            stream << ' ';
        stream << ((s.find(' ') == std::string::npos)
            ? s
            : ast::format_str('"', s, '"'));
    }
    deps.reverse();
    return { links.str(), depends.str() };
}

string Depot::read_source(string moduleName, int64_t& version, string& out_path) {
    stringstream messages;
    for (auto& repo : repos) {
        if (auto mi = repo.modules.find(moduleName); mi != repo.modules.end()) {
            auto& m = mi->second;
            if (m.version < version) {
                messages << m.path << " version " << m.version << " mismatches requested " << version << std::endl;
                continue;
            }
            if (auto r = import_module(m, messages)) {
                version = m.version;
                out_path = m.path;
                return *r;
            }
        }
    }
    for (auto& repo : repos) {
        if (repo.url.empty())
            continue;
        auto mi = repo.modules.find(moduleName);
        if (mi != repo.modules.end() && !mi->second.is_directory)
            continue; // file-based modules arent cloud-updateable
        auto data = curl.load(ast::format_str(repo.url, '/', moduleName, '/', version));
        if (data.empty())
            continue;
        auto newVersion = unzip_from_memory(data, repo.path, moduleName);
        if (!newVersion)
            continue;
        auto newPath = ast::format_str(repo.path, '/', moduleName, '-', *newVersion);
        if (mi != repo.modules.end()) {
            auto& m = mi->second;
            fs::rename(
                m.path,
                ast::format_str(repo.path, '/', moduleName, "_old-", m.version));
            m.path = newPath;
            m.version = *newVersion;
        } else {
            repo.modules.insert({ moduleName, Repo::Module{ newPath, true, *newVersion, moduleName } });
        }
        auto& m = repo.modules[moduleName];
        if (auto r = import_module(m, messages)) {
            version = m.version;
            out_path = m.path;
            return *r;
        }
    }
    std::cerr << "Can't locale module :" << moduleName << " " << messages.str() << std::endl;
    panic();
}
