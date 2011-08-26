#include <boost/filesystem/fstream.hpp>

#include "file_storage.hpp"

using namespace yappi::helpers;
using namespace yappi::persistance::backends;

namespace fs = boost::filesystem;

file_storage_t::file_storage_t(helpers::auto_uuid_t uuid):
    m_storage_path("/var/lib/yappi/" + uuid.get() + ".tasks") /* [CONFIG] */
{
    if(fs::exists(m_storage_path) && !fs::is_directory(m_storage_path)) {
        throw std::runtime_error(m_storage_path.string() + " is not a directory");
    }

    if(!fs::exists(m_storage_path)) {
        try {
            fs::create_directories(m_storage_path);
        } catch(const std::runtime_error& e) {
            throw std::runtime_error("cannot create " + m_storage_path.string());
        }
    }
}

bool file_storage_t::put(const std::string& key, const Json::Value& value) {
    Json::StyledWriter writer;
    fs::path filepath = m_storage_path / key;
    fs::ofstream stream(filepath, fs::ofstream::out | fs::ofstream::trunc);
   
    if(!stream) {    
        syslog(LOG_ERR, "storage: failed to write %s", filepath.string().c_str());
        return false;
    }     

    std::string json = writer.write(value);
    
    stream << json;
    stream.close();

    return true;
}

bool file_storage_t::exists(const std::string& key) const {
    fs::path filepath = m_storage_path / key;
    return fs::exists(filepath) && fs::is_regular(filepath);
}

Json::Value file_storage_t::get(const std::string& key) const {
    Json::Reader reader(Json::Features::strictMode());
    Json::Value root(Json::objectValue);
    fs::path filepath = m_storage_path / key;
    fs::ifstream stream(filepath, fs::ifstream::in);
     
    if(stream) { 
        if(!reader.parse(stream, root)) {
            syslog(LOG_ERR, "storage: malformed json in %s - %s",
                filepath.string().c_str(), reader.getFormatedErrorMessages().c_str());
        }
    }

    return root;
}

Json::Value file_storage_t::all() const {
    Json::Value root(Json::objectValue);
    Json::Reader reader(Json::Features::strictMode());

    fs::directory_iterator it(m_storage_path), end;

    while(it != end) {
        if(fs::is_regular(it->status())) {
            Json::Value value = get(it->leaf());

            if(!value.empty()) {
                root[it->leaf()] = value;
            }
        }

        ++it;
    }

    return root;
}

void file_storage_t::remove(const std::string& key) {
    fs::remove(m_storage_path / key);
}

void file_storage_t::purge() {
    syslog(LOG_NOTICE, "storage: purging");
    
    fs::remove_all(m_storage_path);
    fs::create_directories(m_storage_path);
}