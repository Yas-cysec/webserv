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

void Request::parse_request_line(std::istringstream& stream)
{
    std::string line;
    std::getline(stream, line);
    std::istringstream iss(line);
    iss >> _type >> _path >> _version;
}

bool Request::parse_headers(std::istringstream& stream)
{
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            break;

        size_t pos = line.find(":");
        if (pos == std::string::npos)
            return false;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        if (key.empty())
            return false;
        if (!value.empty() && value[0] == ' ')
            value.erase(0, 1);
        _headers[key] = value;
    }
    if (_headers.find("Host") == _headers.end())
        return false;
    return true;
}

void Request::parse_body(std::istringstream& stream)
{
    char c;
    while (stream.get(c))       // lit UN caractère à la fois
        _requestBody += c;      // l'ajoute tel quel
}


bool Request::InitRequestParser(const std::string& buff)
{
    if (buff.empty())
    {
        // std.cerr
        return false;
    }
    std::istringstream stream(buff); // le creer en flux.

    parse_request_line(stream); // 1 ere ligne parser
    if (!parse_headers(stream)) // parser headers
        return false;
    parse_body(stream); // parse le body
    return true;
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


// autoindex 



std::string Request::list_directory(const std::string& path)
{
    std::string html = "<html><body><h1>Index of " + _path + "</h1><ul>";
    DIR* dir = opendir(path.c_str());
    if (dir == NULL)
        return "";
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)   // lit chaque fichier du dossier
    {
        std::string name = entry->d_name;
        html += "<li><a href=\"" + name + "\">" + name + "</a></li>";
    }
    closedir(dir);

    html += "</ul></body></html>";
    return html;
}



const Location* Request::getMatchedLocation()
{
    const std::vector<Location>& locs = _config.getLocations();
    for (std::size_t i = 0; i < locs.size(); i++)
    {
        if (_path.find(locs[i]._path) == 0)
            return &locs[i];
    }
    return NULL;
}

bool Request::tryAutoindex(const std::string& full_path)
{
    const Location* loc = getMatchedLocation();
    if (loc != NULL && loc->_autoindex)
    {
        std::string listing = list_directory(full_path);
        if (!listing.empty())
        {
            _status = 200;
            _body = listing;
            return true;
        }
    }
    return false;
}

// redirection : 

bool Request::handleRedirect()
{
    const Location* loc = getMatchedLocation();
    if (loc != NULL && !loc->_redirect.empty())
    {
        _status = 301;
        _redirectUrl = loc->_redirect;
        _body = "";
        return true;   // il y a une redirection
    }
    return false;      // pas de redirection
}

bool Request::handle_get() // cherche le fichier et lit son contenu 
{


    if (_path.find("..") != std::string::npos)   // contient ".." ?
    {
        setError(403);
        return false;
    }

     if (handleRedirect())          // redirection ?
        return true;
    
    if (_path == "/")
        _path = "/" + _config.get_index();

    std::string full_path = _config.get_root() + _path; // www/index.html
    std::ifstream file(full_path.c_str());

    if (!file.is_open())
    {
         if (tryAutoindex(full_path)) // autoindex ?
            return true;
        setError(404);
        return false; // fichier absen -> error 404
    }

    std::string content,line;
    while (std::getline(file, line))
        content += line + "\n";
    if (content.empty())               // rien lu → sûrement un dossier ou vide
    {
        if (tryAutoindex(full_path))   // autoindex ?
            return true;
        setError(404);
        return false;
    }

    _status = 200;
    _body = content; // on stocke pour l'envoyer plus tard (le fichier html..)
    return true;
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
    if (_requestBody.size() > _config.get_maxbodySize()) 
    {
        setError(413);
        return false;
    }
    if (_headers.find("Content-Length") == _headers.end())
    {
        setError(400);
        return false;
    }
    
    std::string full_path;
    const Location* loc = getMatchedLocation();
    if (loc != NULL && !loc->_upload.empty())
        full_path = loc->_upload + _path;        // dossier upload de la location
    else
        full_path = _config.get_root() + _path;  // sinon le root normal


    std::ofstream file(full_path.c_str()); // ouvre en ecriture.. 
     if (!file.is_open())
    {
        setError(500);
        return false;
    }
    file << _requestBody; // ecrit les donnees recus dans le fichier
    file.close();

    _status = 201; // 201 = created (ressource creer)
    _body = "<h1>File uploaded</h1>";
    return true;
}

bool Request::handle_delete()
{
    std::string full_path = _config.get_root() + _path;
    if (std::remove(full_path.c_str()) == 0) // suppresion reussi
    {
        _status = 200;
        _body = "<h1>File deleted</h1>";
        return true;
    }
    else 
    {
        setError(404);
        return false;
    }
    return true;

}



bool Request::is_method_allowed()
{
    // cherche la location qui correspond au chemin
    for (std::size_t i = 0; i < _config.getLocations().size(); i++)
    {
        // le chemin commence-t-il par le path de la location ?
        if (_path.find(_config.getLocations()[i]._path) == 0)
        {
            // location trouvée → la méthode est-elle dans allow_methods ?
            for (std::size_t j = 0; j < _config.getLocations()[i]._methods.size(); j++)
            {
                if (_config.getLocations()[i]._methods[j] == _type)
                    return true;   // méthode autorisée
            }
            return false;          // location trouvée mais méthode pas autorisée
        }
    }
    return true;   // aucune location ne matche → pas de restriction → autorisé
}


void Request::act_request() // quel requetes c'est ? 

{
    if (!is_method_allowed())      // ← vérif AVANT tout
    {
        _status = 405;
        _body = "<h1>405 Method Not Allowed</h1>";
        return;
    }

    if (_type == "GET")
        handle_get();
    else if (_type == "POST")
        handle_post();
    else if (_type == "DELETE")
        handle_delete();
    else 
    {
        _status = 405;
        _body = "<h1>405 Method Not Allowed</h1>";
    }
}

std::string Request::build_response() // ajoute la structure http de la reponse pour que http comprenne

{
    std::stringstream response;
    if (_status == 200)
        response << "HTTP/1.1 200 OK\r\n";
    else if (_status == 201)
        response << "HTTP/1.1 201 Created\r\n";
    else if (_status == 400)
        response << "HTTP/1.1 400 Bad Request\r\n";
    else if (_status == 403)
        response << "HTTP/1.1 403 Forbidden\r\n";
    else if (_status == 404)
        response << "HTTP/1.1 404 Not Found\r\n";
    else if (_status == 405)
        response << "HTTP/1.1 405 Method Not Allowed\r\n";
    else if (_status == 413)
        response << "HTTP/1.1 413 Payload Too Large\r\n";
    else if (_status == 500)
        response << "HTTP/1.1 500 Internal Server Error\r\n";
    response << "Content-Length: " << _body.size() << "\r\n";
    response << "Content-Type: text/html\r\n";
    response << "\r\n";      // ligne vide
    response << _body;       //  HTML
    return response.str();
}



std::string Request::intToString(int n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}



void Request::setError(int code)
{
    _status = code;
    // il y a une page custom dans la config ??
    std::map<int, std::string>::const_iterator it = _config.getError_pages().find(code);
    if (it != _config.getError_pages().end())
    {
        // charger le fichier custom
        std::string full_path = _config.get_root() + it->second;
        std::ifstream file(full_path.c_str());
        if (file.is_open())
        {
            std::string content, line;
            while (std::getline(file, line))
                content += line + "\n";
            _body = content;
            return;
        }
    }
    // pas de page custom -> body par default 
    _body = "<h1>Error " + intToString(code) + "</h1>";
}


// set config : 
void Request::set_config(const ServerConfig& config)
{
    _config = config; 
}


std::string Request::getHost()
{
    if (_headers.find("Host") != _headers.end())
        return _headers["Host"];
    return "";
}





/*



À corriger avant de partir trop loin :
Mettre les sockets en non-bloquant avec fcntl(..., O_NONBLOCK).
epoll seul ne rend pas tes sockets non-bloquants.

Garder un buffer par client.
Là, tu fais un seul recv() puis tu supposes que toute la requête est arrivée. Ce n’est pas toujours vrai.

Gérer les envois partiels.
send() peut envoyer seulement une partie de la réponse. Il faut garder ce qui reste à envoyer.

Valider vraiment la requête.
Tu appelles InitRequestParser(), mais tu n’utilises pas son résultat et tu n’appelles pas Parser(). Une requête invalide doit devenir 400, pas continuer normalement.

Corriger le GET statique :
fichier vide doit être 200, pas 404 ; MIME types ; empêcher ../.

POST doit garder le body exactement comme reçu.
Ton getline() rajoute des retours à la ligne, donc un vrai fichier binaire serait modifié.


1. Mettre tous les sockets en non-bloquant
2. Garder un buffer de lecture par client
3. Attendre la requête complète avant de la parser
4. Garder un buffer d’écriture par client
5. Reprendre l’envoi si send() n’a envoyé qu’une partie
6. Nettoyer un client qui coupe la connexion
7. Répondre 400 si la requête est invalide
8. Sécuriser GET : pas de ../, fichier vide = 200, bon Content-Type
9. Garder le body POST exactement comme reçu
*/


/*
comprehension du non bloquant : 
notre projet gere client apres client mais tres rapidement illusion de parallele

un buffer par client 

on regele juste en mettant une option Ononblock pour le fd
on use fcntl (modifie les reglages d'un fd) 

en gros la logique c : 

    disons on a 3 Monsieurs A et B sont lent et C et complet.. 
        A envoie un bout (on lis -> et met dans _buffer[a] ->
        check si c pas complet -> on laisse et passe a autre chose
        B -> pareil 
        C envoie tout -> tu lis (_buffer[C] complet -> tu traite C 
            et tu reponds.. 
voila la logique.. 

avant nous on faisait : 
    recv (Fonction qui lit ce que le client t'envoie) prend data
    et met dans buffer.. ca c normal mais en gros le pb c que 
    on utiliser recv que 1 fois considerant que tout est envoyer 
    d'un coup... 

        
    en gros on stoc un buffer par client on continue le programme

    concernant le send aussi on considere qu'on envoie tout d'un coup.. faut gerer
    ce non bloquant.. 
*/