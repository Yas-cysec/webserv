#include "Config.hpp"
#include "Server.hpp"
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage : ./webserv config.conf" << std::endl;
        return 1;
    }

    Config config;
    config.load(argv[1]);

    const std::vector<ServerConfig>& servers = config.getServers();
    if (servers.empty())
        return 1;

    // on prend le premier server de la config
    Server server(servers[0]);

    // on crée une porte pour chaque port
    const std::vector<int>& ports = servers[0].getPorts();
    for (std::size_t i = 0; i < ports.size(); i++)
        server.start(ports[i]);

    // on lance le serveur (boucle infinie)
    server.run();

    return 0;
}