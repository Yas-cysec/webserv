#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <fstream>
#include <string>
#include <vector>

class ServerConfig
{
private:
    std::vector<int> _ports;

public:
    void addPort(int port);
    const std::vector<int>& getPorts() const;
};

class Config
{
private:
    std::vector<ServerConfig> _servers;

    bool parseListen(const std::string& line, ServerConfig& server);
    bool parseServerBlock(std::ifstream& file);

public:
    void load(const std::string& fileName);
    const std::vector<ServerConfig>& getServers() const;
};

#endif