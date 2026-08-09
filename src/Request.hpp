#ifndef _REQUEST_HPP_
# define _REQUEST_HPP_

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unistd.h> 


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
    public :
        bool InitRequestParser(const std::string &buff);
        bool Parser();
        void act_request();
        bool handle_get();
        bool handle_post();
        bool handle_delete();
        std::string build_response();
        void parse_request_line(std::istringstream& stream);
        bool parse_headers(std::istringstream& stream);
        void parse_body(std::istringstream& stream);
        void setError(int code);

    private: 
    


};




# endif