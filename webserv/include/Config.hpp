#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>

class Location
{
    public :
        std::string _path; // "/images"
        std::vector<std::string> _methods; // [get, post]
        bool _autoindex;
        Location() : _autoindex(false) {}
        std::string _redirect;
        std::string _root;
        std::string _index;
        std::string _upload;
        std::string _cgiExtension; // .py 
        std::string _cgiInterpreter; // usr/bin/python3
};


class ServerConfig
{
private:
    std::vector<int> _ports;
    std::string _root;
    std::string _index;
    std::size_t _maxBodySize;
    std::vector<Location> _locations;
    std::string _serverName;
    std::map<int, std::string> _errorPages; // 404 -> "erreur/404.html"

public:
    ServerConfig() : _root("www"), _index("index.html"), _maxBodySize(1000000) {}; // 1 mo body
    // port
    void addPort(int port);
    const std::vector<int>& getPorts() const;
    // root
    void set_root(const std::string& root);
    const std::string &get_root() const;
    // index
    void set_index(const std::string &index);
    const std::string &get_index() const;
    // Maxbody
    void set_maxbodySize(std::size_t bodysize);
    std::size_t  get_maxbodySize() const;
    // location
    void addLocation(const Location &loc);
    const std::vector<Location>& getLocations() const;
    // server name 
    void setServerName(const std::string& name);
    const std::string& getServerName() const;
    // parse error .. 
    void addError_page(int code, const std::string& path);
    const std::map<int, std::string>& getError_pages() const;
};


class Config
{
private:
    std::vector<ServerConfig> _servers;

    bool parseListen(const std::string &line, ServerConfig &server);
    bool parse_root(const std::string &line, ServerConfig &server);
    bool parse_index(const std::string& line, ServerConfig& server);
    bool parse_max_body_size(const std::string& line, ServerConfig& server);
    // location et directives.. 
    bool parseLocation(const std::string& firstLine, std::ifstream& file, ServerConfig& server);
    // directives de locations... 
    bool parse_methods(const std::string& line, Location& loc);
    bool parse_autoindex(const std::string& line, Location &loc);
    bool parse_return(const std::string& line, Location& loc);
    bool parse_(const std::string& line, Location& loc);

    // parse error pages.. 
    bool parse_error_page(const std::string& line, ServerConfig& server);

    // utils..
    bool parseServerBlock(std::ifstream& file);
    std::string cleanSpaces(const std::string& s);
    std::string extract_value(const std::string &line, const std::string &word);
    
    // cgi 

    bool parse_cgi(const std::string &line, Location &loc);

public:
    void load(const std::string& fileName);
    const std::vector<ServerConfig>& getServers() const;



};

#endif

/*
// recap : 
    on a scraper le fichier de config
    tout les listen on a mis dans bon
        server en mettant le port exact





    


*/