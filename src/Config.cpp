#include "Config.hpp"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>

void ServerConfig::addPort(int port)
{
    _ports.push_back(port);
}

const std::vector<int>& ServerConfig::getPorts() const
{
    return _ports;
}

const std::vector<ServerConfig>& Config::getServers() const
{
    return _servers;
}

static bool check_charac(std::string line)
{
    std::size_t i = 0;

    while (i < line.size())
    {
        if (!std::isdigit(line[i]))
            return false;
        i++;
    }
    return true;
}

bool Config::parseListen(const std::string& line, ServerConfig& server)
{
    std::size_t start = line.find("listen") + 6;
    std::size_t end = line.find(";", start);

    if (end == std::string::npos) // si ; manquant
    {
        std::cerr << "Erreur : ; manquant" << std::endl;
        return false;
    }

    while (start < end && (line[start] == ' ' || line[start] == '\t'))
        start++; // enleve espace avant portext

    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t'))
        end--; // space avant ;

    std::string portText = line.substr(start, end - start);

    if (!check_charac(portText))
    {
        std::cerr << "Erreur : port invalide" << std::endl;
        return false;
    }

    int port = std::atoi(portText.c_str());

    if (port < 1 || port > 65535)
    {
        std::cerr << "Erreur : port invalide" << std::endl;
        return false;
    }

    server.addPort(port);
    return true;
}

bool Config::parseServerBlock(std::ifstream& file)
{
    ServerConfig server;
    std::string line;
    bool foundListen = false;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        if (line == "}") // condition de fin server {}
        {
            if (!foundListen)
            {
                std::cerr << "Erreur : listen manquant" << std::endl;
                return false;
            }

            _servers.push_back(server);
            return true;
        }

        if (line.find("listen") != std::string::npos)
        {
            if (!parseListen(line, server))
                return false;

            foundListen = true;
        }
        else
        {
            std::cerr << "Erreur : regle inconnue dans server"
                      << std::endl;
            return false;
        }
    }

    std::cerr << "Erreur : } manquant" << std::endl;
    return false;
}

void Config::load(const std::string& fileName)
{
    std::ifstream file(fileName.c_str());
    std::string line;

    if (!file)
    {
        std::cerr << "Impossible d'ouvrir le fichier" << std::endl;
        return;
    }

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        if (line == "server {")
        {
            if (!parseServerBlock(file))
                return;
        }
        else
        {
            std::cerr << "Erreur : server { attendu" << std::endl;
            return;
        }
    }

    if (_servers.empty())
        std::cerr << "Erreur : aucun server trouve" << std::endl;
}