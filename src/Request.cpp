#include "Request.hpp"
#include <fstream>

/*
comment debuter ? 

    requetes -> long texte
                je dois decouper via getline et stocker
                use -> istringstream (decoupe ligne par espaces)


    GET / HTTP/1.1
Host: localhost:8080
User-Agent: curl/7.81.0
Accept: *


ligne 1 : on stock get le chemin et la version.. + parsing requete
        on s'est assurer que le fichier est conforme dans le stockage
        (il manque post)
             - si requete vide
             - si header sans nom etc..

ligne 2 : on traite la requetes (agir selon ce qui est demander)


*/

bool Request::InitRequestParser(const std::string& buff)
{
    if (buff.empty())
    {
        // std.cerr
        return false;
    }
    std::istringstream stream(buff); // le creer en flux.
    std::string line;

    std::getline(stream, line);
    std::istringstream iss(line); 
    iss >> _type >> _path >> _version; // stockage ligne 1 
    
    
    while ( std::getline(stream, line)) // a besoin de lire a partir du flux
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);   // enlève le \r en fin de ligne
        if (line.empty()) // fin
            break;
        size_t pos = line.find(":"); // trouve le :
        if (pos == std::string::npos)   // pas de ':' → header invalide
            return false;

        std::string key = line.substr(0, pos); // avant le :
        std::string value = line.substr(pos + 1); // apres le :
        if (key.empty())                // clé vide → invalide
            return false;
        if (!value.empty() && value[0] == ' ')   // enlève l'espace de début
            value.erase(0, 1);
        _headers[key] = value;
    }
    if (_headers.find("Host") == _headers.end())
            return false;   // Host obligatoire manquant
    return (true);
}

bool Request::Parser()
{
    if (_type.empty() || _path.empty() || _version.empty())
        return false;
    if (_type != "GET" && _type != "POST" && _type != "DELETE")
        return (false);
    if (_version != "HTTP/1.1")
        return false; // version non supporter
    return true;
}

bool Request::handle_get() // cherche le fichier et lit son contenu 
{
    std::string full_path = "www" + _path; // www/index.html

    std::ifstream file(full_path.c_str());
    if (!file.is_open())
        return false; // fichier absen -> error 404
    std::string content,line;
    while (std::getline(file, line))
        content += line + "\n";
    _body = content; // on stocke pour l'envoyer plus tard (le fichier html..)
    return true;
}

std::string Request::build_response() // ajoute la structure http de la reponse pour que http comprenne
{
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Length: " << _body.size() << "\r\n";
    response << "Content-Type: text/html\r\n";
    response << "\r\n";      // ligne vide
    response << _body;       // ton HTML
    return response.str();
}


// void Request::send_response(int clientfd) // envoie reponse au client -> page affiche
// {
//     std::string response = build_response();// recupere la reponse stocker
//     send(clientfd, response.c_str(), response.size(), 0); // envoie..
//     _responses.erase(clientfd); // nettoie la map.. 
//     epoll_ctl(epdf, EPOLL_CTL_DEL, clientFd, NULL); // retire de epoll
//     close(clientfd); // ferme la connexion
// }


bool Request::handle_post()
{
    return true;

}

bool Request::handle_delete()
{
    return true;

}


void Request::act_request()
{
    if (_type == "GET")
        handle_get();
    else if (_type == "POST")
        handle_post();
    else if (_type == "DELETE")
        handle_delete();
}