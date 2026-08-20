#ifndef CGI_HPP
# define CGI_HPP

#include <iostream>
#include <string>

class Cgi
{
    private:
            std::string _interpreter; // if py ou php
            std::string _scriptPath;
            std::string _contentHtml;

    public: 
        Cgi(const std::string &interpreter, const std::string &scriptPath);
        std::string execute();
};

// on fais 3 choses : 
/*
lancer le script (fork + exec)

recuperer le html (lecture du pipe)

renvoyer ca au client
*/



# endif 