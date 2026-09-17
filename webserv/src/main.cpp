#include "Config.hpp"
#include "Server.hpp"
#include <iostream>



#include <csignal>

volatile sig_atomic_t g_running = 1; // ctrl + C 

void handleSignal(int sig)
{
    (void)sig;
    g_running = 0;   // arrête la boucle
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage : ./webserv config.conf" << std::endl;
        return 1;
    }
    signal(SIGINT, handleSignal); 

    Config config;
    config.load(argv[1]);

    const std::vector<ServerConfig>& servers = config.getServers();
    if (servers.empty())
        return 1;

    Server server;   // constructeur vide

    for (std::size_t i = 0; i < servers.size(); i++)
    {
        const std::vector<int>& ports = servers[i].getPorts();
        for (std::size_t j = 0; j < ports.size(); j++)         // tous les ports
            server.start(ports[j], servers[i]);                // port + sa config
    }

    server.run();
    return 0;
}