#ifndef _REQUEST_HPP_
# define _REQUEST_HPP_

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unistd.h> 
#include "Config.hpp"
#include <fstream>
#include <dirent.h> 
#include "Cgi.hpp"


class Request
{
    private: 
        std::string _type; // get ou post ou delete
        std::string _path; // chemin : index.html
        std::string _version; // HTTP/1.1
        std::map<std::string, std::string> _headers; // host -> valeur
        int _status; // code erreur status.. 
        std::string _body;
        std::string _requestBody;
        // config 
        ServerConfig _config;
        // redirect
        std::string _redirectUrl;
        //cgi 
        bool _isCgi;
        std::string _cgiInterpreter; 
        std::string _cgiScriptPath;
        std::string _cgiQuery;



    public :
        bool InitRequestParser(const std::string &buff);
        bool Parser();
        void act_request();
        bool handle_get();
        bool handle_post();
        bool handle_delete();
        std::string build_response();
        bool parse_request_line(std::istringstream& stream);
        bool parse_headers(std::istringstream& stream);
        void parse_body(std::istringstream& stream);
        void setError(int code);
        std::string intToString(int n);

        void set_config(const ServerConfig& config);
        std::string getHost();





        bool is_method_allowed();
        // autoindex.. 
        std::string list_directory(const std::string& path);
        const Location* getMatchedLocation();
        bool tryAutoindex(const std::string& full_path);

        // redirection
        bool handleRedirect();

        // cgi
        Request() : _isCgi(false) {}
        bool tryCgi();
        bool isCgi() const { return _isCgi; }
        std::string getCgiInterpreter() const { return _cgiInterpreter; }
        std::string getCgiScriptPath() const { return _cgiScriptPath; }
        std::string getCgiQuery() const { return _cgiQuery; }
        std::string getMethod() const { return _type; }
        std::string getBody() const { return _requestBody; }
        std::string getHeader(const std::string& k) { return _headers[k]; }


};




# endif