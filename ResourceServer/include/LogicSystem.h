#pragma once
#include "Singleton.h"
#include "FileInfo.h"
#include <vector>
#include "BaseLogic.h"

class LogicSystem : public BaseLogic, public Singleton<LogicSystem> {
    friend class Singleton<LogicSystem>;
public:
    //void AddMD5File(std::string md5, std::shared_ptr<FileInfo> fileinfo);
    //std::shared_ptr<FileInfo> GetFileInfo(std::string md5);

private:
    LogicSystem() : BaseLogic(8) { RegisterHandlers(); Start(); }
    // 实现具体的上传/下载回调
    void RegisterHandlers() override;

    std::mutex _file_mtx;
    std::unordered_map<std::string, std::shared_ptr<FileInfo>> _map_md5_files;
};