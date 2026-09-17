#ifndef CGI_HPP
# define CGI_HPP

#include <iostream>
#include <string>
#include <ctime>
#include <csignal>

#include "Request.hpp"


struct CgiProcess
{
    int clientFd;          // à qui renvoyer la réponse
    pid_t pid;             // le script (pour le tuer)
    time_t startTime;      // quand il a démarré (timeout)
    std::string output;    // ce qu'on a lu jusqu'ici
};


/*
class Cgi
{
    private:
            std::string _interpreter; // if py ou php
            std::string _scriptPath;
            std::string _contentHtml;
            std::string _methods;
            std::string _queryString;
            std::string _contentLen;
            std::string _contentType;
            std::string _pathInfo;
            std::string _body;
            

    public: 
        Cgi(const std::string& interpreter, const std::string& scriptPath, 
                const std::string &method, const std::string &queryString,
                const std::string &contentLen, const std::string &contentType,
                const std::string &pathInfo, const std::string &body);
        std::string execute();
        bool waitWithTimeout(pid_t pid);
        void env_var(std::vector<std::string > &env);
};

// on fais 3 choses : 
lancer le script (fork + exec)

recuperer le html (lecture du pipe)

renvoyer ca au client
*/



# endif 