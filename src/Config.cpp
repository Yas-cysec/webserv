#include "Config.hpp"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>

// port
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

// listen
bool Config::parseListen(const std::string& line, ServerConfig& server)
{
    std::size_t start = line.find("listen") + std::string("listen").length();
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

// root
bool Config::parse_root(const std::string &line, ServerConfig &server)
{
    std::size_t start = line.find("root") + std::string("root").length(); // debut apres root.. 
    std::size_t end = line.find(";", start); // trouve ; a partir de start.. 

    if (end == std::string::npos)
    {
        std::cerr << "Erreur : ; manquant apres root" << std::endl;
        return false;
    }
    // retirer les espaces.. 
    while (start < end && (line[start] == ' ' || line[start] == '\t'))
        start++;
    while (end > start && (line[end-1] == ' ' || line[end-1] == '\t'))
        end--;
    std::string value = line.substr(start, end - start);
    if (value.empty())  // valeur vide → erreur de config
    {
        std::cerr << "Erreur : root vide" << std::endl;
        return false;
    }
    server.set_root(value); // stock root.. 
    return (true);
}

void ServerConfig::set_root(const std::string& root)
{
    _root = root;
}

const std::string& ServerConfig::get_root() const
{
    return _root;
}

// index 
void ServerConfig::set_index(const std::string& index) 
{ 
    _index = index; 
}

const std::string& ServerConfig::get_index() const 
{ 
    return _index; 
}

bool Config::parse_index(const std::string& line, ServerConfig& server)
{
    std::size_t start = line.find("index") + std::string("index").length();
    std::size_t end = line.find(";", start);

    if (end == std::string::npos)
    {
        std::cerr << "Erreur : ; manquant apres index" << std::endl;
        return false;
    }
    while (start < end && (line[start] == ' ' || line[start] == '\t'))
        start++;
    while (end > start && (line[end-1] == ' ' || line[end-1] == '\t'))
        end--;
    std::string value = line.substr(start, end - start);
    if (value.empty())
    {
        std::cerr << "Erreur : index vide" << std::endl;
        return false;
    }
    server.set_index(value);
    return true;
}

// maxbody 

void ServerConfig::set_maxbodySize(std::size_t size)
{
    _maxBodySize = size;
}

std::size_t ServerConfig::get_maxbodySize() const
{
    return _maxBodySize;
}

bool Config::parse_max_body_size(const std::string& line, ServerConfig& server)
{
    std::string kw = "client_max_body_size";
    std::size_t start = line.find(kw) + kw.length();
    std::size_t end = line.find(";", start);
    if (end == std::string::npos)
    {
        std::cerr << "Erreur : ; manquant apres client_max_body_size" << std::endl;
        return false;
    }
    while (start < end && (line[start] == ' ' || line[start] == '\t'))
        start++;
    while (end > start && (line[end-1] == ' ' || line[end-1] == '\t'))
        end--;
    std::string value = line.substr(start, end - start);
    if (!check_charac(value))
    {
        std::cerr << "Erreur : client_max_body_size invalide" << std::endl;
        return false;
    }
    server.set_maxbodySize(atoi(value.c_str()));
    return true;
}



//---------------------------------------------------

// locations 

void ServerConfig::addLocation(const Location& loc)
{
    _locations.push_back(loc);
}

const std::vector<Location>& ServerConfig::getLocations() const
{
    return _locations;
}

std::string Config::cleanSpaces(const std::string& s)
{
    std::size_t start = s.find_first_not_of(" \t");
    std::size_t end = s.find_last_not_of(" \t");

    if (start == std::string::npos)
        return "";
    return s.substr(start, end - start + 1);
}

        // directives
bool Config::parse_methods(const std::string& line, Location& loc)
{
    std::string kw = "allow_methods";
    std::size_t start = line.find(kw) + kw.length();
    std::size_t end = line.find(";", start);

    if (end == std::string::npos)
    {
        std::cerr << "Erreur : ; manquant apres allow_methods" << std::endl;
        return false;
    }
    std::string methods_str = line.substr(start, end - start);   // " GET POST"

    std::istringstream iss(methods_str); // transforme en flux
    std::string method;
    while (iss >> method)                    // découpe par espaces
    {
        if (method == "GET" || method == "POST" || method == "DELETE")
            loc._methods.push_back(method);      // vrai méthode → on range
        else
        {
            std::cerr << "Erreur : methode inconnue : " << method << std::endl;
            return false;                        // faux → erreur de config
        }
    }
    return true;
}


std::string Config::extract_value(const std::string& line, const std::string& word)
{
        std::size_t start = line.find(word) + word.length();
        std::size_t end = line.find(";", start);
        if (end == std::string::npos)
            return "";
         return cleanSpaces(line.substr(start, end - start)); // tu return le mot trouver
}

bool Config::parse_autoindex(const std::string& line, Location& loc)
{
    std::string value = extract_value(line, "autoindex");
    if (value == "on")
        loc._autoindex = true;
    else if (value == "off")
        loc._autoindex = false;
    else
    {
        std::cerr << "Erreur : autoindex doit etre on ou off" << std::endl;
        return false;
    }
    return true;
}


bool Config::parseLocation(const std::string& firstLine, std::ifstream& file, ServerConfig& server)
{
    Location loc;

    // 1 - extraire le chemin depuis "location /images (ex) {"
    std::string kw = "location";
    std::size_t start = firstLine.find(kw) + kw.length();
    std::size_t end = firstLine.find("{", start);

    std::string path = firstLine.substr(start, end - start); // par ex /images.. 
    loc._path = cleanSpaces(path);


    // 2 - je lis les regles jusqu'a }
    std::string line;
    while (std::getline(file, line))
    {
        line = cleanSpaces(line); 
        if (line.empty())
            continue;
        if (line == "}")
        {
            server.addLocation(loc);
            return true;
        }
        if (line.find("allow_methods") != std::string::npos)
        {
            if (!parse_methods(line, loc))
                return false;
        }
        else if (line.find("autoindex") != std::string::npos)
        {
            if (!parse_autoindex(line, loc))
                return false;
        }
        else if (line.find("return") != std::string::npos)
        {
                loc._redirect = extract_value(line, "return");
        }
        else if (line.find("root") != std::string::npos)
        {
                loc._root= extract_value(line, "root");
        }
        else if (line.find("index") != std::string::npos)
        {
            loc._index = extract_value(line, "index");
        }
        else if (line.find("upload") != std::string::npos)
        {
            loc._upload = extract_value(line, "upload");
        }
        else
        {
            std::cerr << "Erreur : regle inconnue dans location" << std::endl;
            return false;
        }
    }
    std::cerr << "Erreur : } manquant dans location" << std::endl;
    return false;
}


//---------------------------------------------------


// server name 

void ServerConfig::setServerName(const std::string& name) 
{ 
    _serverName = name; 
}
const std::string& ServerConfig::getServerName() const 
{ 
    return _serverName; 
}

// parse error pages..

void ServerConfig::addError_page(int code, const std::string& path)
{
    _errorPages[code] = path;
}

const std::map<int, std::string>& ServerConfig::getError_pages() const
{
    return _errorPages;
}

bool Config::parse_error_page(const std::string& line, ServerConfig& server)
{
    std::string kw = "error_page";
    std::size_t start = line.find(kw) + kw.length();
    std::size_t end = line.find(";", start);

    if (end == std::string::npos)
    {
        std::cerr << "Erreur : ; manquant apres error_page" << std::endl;
        return false;
    }

    std::string rest = line.substr(start, end - start);   // " 404 /erreurs/404.html"
    std::istringstream iss(rest);
    int code;
    std::string path;
    iss >> code >> path;          // extrait le nombre PUIS le chemin

    server.addError_page(code, path);
    return true;
}

// parse 

bool Config::parseServerBlock(std::ifstream& file)
{
    ServerConfig server;
    std::string line;
    bool foundListen = false;

    while (std::getline(file, line))
    {
        line = cleanSpaces(line);  
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
        
        else if (line.find("location") != std::string::npos)
        {
                if (!parseLocation(line, file, server))   // note : on passe "file" aussi
                    return false;
        }

        else if (line.find("client_max_body_size") != std::string::npos)
        {
            if (!parse_max_body_size(line, server))
                return false;
        }

        else if (line.find("root") != std::string::npos)
        {
            if (!parse_root(line, server))
                return false;
        }

        else if (line.find("index") != std::string::npos)
        {
            if (!parse_index(line, server))
                return false;
        }

        else if (line.find("server_name") != std::string::npos)
        {
            server.setServerName(extract_value(line, "server_name"));
        }
    
        else if (line.find("error_page") != std::string::npos)
        {
            if (!parse_error_page(line, server)) 
                return false;
        }

        else
        {
            std::cerr << "Erreur : regle inconnue dans server" << std::endl;
            return false;
        }

    }

    std::cerr << "Erreur : } manquant" << std::endl;
    return false;
}

void Config::load(const std::string& fileName) // charge le fichier..
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

