#include "config.h"
#include "env.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

ConfigVarBase::ptr Config::LookupBase(const std::string &name) {
    RWMutexType::ReadLock lock{GetMutex()};
    auto it = GetDatas().find(name);
    return it == GetDatas().end() ? nullptr : it->second;
}

static void ListAllMember(const std::string &prefix, const YAML::Node &node,
                          std::list<std::pair<std::string, const YAML::Node>> &output) {
    if (prefix.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678") != std::string::npos) {
        SYLAR_LOG_ERROR(g_logger) << "Config invalid name: " << prefix << " : " << node;
        return;
    }

    output.push_back({prefix, node});
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }
}

void Config::LoadFromYaml(const YAML::Node &root) {
    std::list<std::pair<std::string, const YAML::Node>> all_nodes;
    ListAllMember("", root, all_nodes);

    for (auto &[name, node] : all_nodes) {
        if (name.empty()) {
            continue;
        }

        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (ConfigVarBase::ptr var = LookupBase(name); var) {
            if (node.IsScalar()) {
                var->fromString(node.Scalar());
            } else {
                std::stringstream ss;
                ss << node;
                var->fromString(ss.str());
            }
        }
    }
}

static std::map<std::string, uint64_t> s_file2modifytime;
static sylar::Mutex s_mutex;

void Config::LoadFromConfDir(const std::string &path, bool force) {
    std::string absolute_path = sylar::EnvMgr::GetInstance()->getAbsolutePath(path);
    std::vector<std::string> files;
    FSUtil::ListAllFile(files, absolute_path, ".yml");

    for (const auto &file : files) {
        {
            struct stat st;
            lstat(file.c_str(), &st);
            sylar::Mutex::Lock lock{s_mutex};
            if (!force && s_file2modifytime[file] == (uint64_t)st.st_mtime) {
                continue;
            }
            s_file2modifytime[file] = st.st_mtime;
        }

        try {
            YAML::Node root = YAML::LoadFile(file);
            LoadFromYaml(root);
            SYLAR_LOG_INFO(g_logger) << "LoadConfFile file=" << file << " ok";
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "LoadConfFile file=" << file << " failed";
        }
    }
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
    RWMutexType::ReadLock lock{GetMutex()};
    for (const auto &[_, varBase] : GetDatas()) {
        cb(varBase);
    }
}

}  // namespace sylar