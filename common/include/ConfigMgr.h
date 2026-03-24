#pragma once
#include <string>
#include <map>
#include <unordered_map>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <algorithm>

// 存储 INI 的一个配置节
struct SectionInfo {
    std::map<std::string, std::string> _section_datas;

    // 获取配置，若不存在返回空字符串
    std::string GetValue(const std::string& key) const {
        auto it = _section_datas.find(key);
        if (it == _section_datas.end()) {
            return "";
        }
        return it->second;
    }

    std::string operator[](const std::string& key) const {
        return GetValue(key);
    }
};

class ConfigMgr {
public:
    // 禁用拷贝和赋值 (单例模式标准做法)
    ConfigMgr(const ConfigMgr&) = delete;
    ConfigMgr& operator=(const ConfigMgr&) = delete;

    ~ConfigMgr() = default;

    static ConfigMgr& Inst() {
        static ConfigMgr cfg_mgr;
        return cfg_mgr;
    }

    // 获取配置值
    std::string GetValue(const std::string& section, const std::string& key) const;

    // 获取 Section
    SectionInfo GetSection(const std::string& section) const {
        auto it = _config_map.find(section);
        if (it == _config_map.end()) {
            return SectionInfo();
        }
        return it->second;
    }

    // 语法糖：config["Section"]["Key"]
    SectionInfo operator[](const std::string& section) const {
        return GetSection(section);
    }

    bool HasSection(const std::string& section) const {
        return _config_map.find(section) != _config_map.end();
    }

    std::filesystem::path GetFileOutPath() const;

private:
    ConfigMgr();
    void InitPath();

    // 辅助函数：去除字符串两端空格
    static std::string Trim(std::string s);

    std::map<std::string, SectionInfo> _config_map;
    std::filesystem::path _static_path;
    std::filesystem::path _bin_path;
};