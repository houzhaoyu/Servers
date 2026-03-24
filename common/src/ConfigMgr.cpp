#include "ConfigMgr.h"

ConfigMgr::ConfigMgr() {
    namespace fs = std::filesystem;

    // 1. 定位配置文件路径 (exe同级目录)
    fs::path config_path = fs::current_path() / "config.ini";

    if (!fs::exists(config_path)) {
        std::cerr << "Error: Config file not found at " << config_path << std::endl;
        return;
    }

    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open config file!" << std::endl;
        return;
    }

    // 2. 手动解析 INI (替代 Boost.PropertyTree)
    std::string line, current_section;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[' && line.back() == ']') {
            // 解析 [Section]
            current_section = line.substr(1, line.length() - 2);
        }
        else {
            // 解析 Key=Value
            size_t pos = line.find('=');
            if (pos != std::string::npos && !current_section.empty()) {
                std::string key = Trim(line.substr(0, pos));
                std::string value = Trim(line.substr(pos + 1));
                _config_map[current_section]._section_datas[key] = std::move(value);
            }
        }
    }

    // 3. 打印调试信息
    std::cout << "Loaded Config from: " << config_path << std::endl;
    for (const auto& [name, info] : _config_map) {
        std::cout << "[" << name << "]" << std::endl;
        for (const auto& [k, v] : info._section_datas) {
            std::cout << "  " << k << " = " << v << std::endl;
        }
    }

    InitPath();
}

std::string ConfigMgr::GetValue(const std::string& section, const std::string& key) const {
    auto it = _config_map.find(section);
    if (it == _config_map.end()) return "";
    return it->second.GetValue(key);
}

std::string ConfigMgr::Trim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
    return s;
}

std::filesystem::path ConfigMgr::GetFileOutPath() const {
    return _static_path;
}

void ConfigMgr::InitPath() {
    namespace fs = std::filesystem;
    fs::path current_path = fs::current_path();

    std::string bindir = GetValue("Output", "Path");
    std::string staticdir = GetValue("Static", "Path");

    // 默认值处理，防止配置缺失
    if (bindir.empty()) bindir = ".";
    if (staticdir.empty()) staticdir = "static";

    _bin_path = current_path / bindir;
    _static_path = _bin_path / staticdir;

    try {
        if (!fs::exists(_static_path)) {
            if (fs::create_directories(_static_path)) {
                std::cout << "Created directory: " << _static_path << std::endl;
            }
        }
        else {
            std::cout << "Directory already exists: " << _static_path << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "FileSystem Error: " << e.what() << std::endl;
    }
}