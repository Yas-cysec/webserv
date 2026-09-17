#include "Request.hpp"



bool Request::parse_request_line(std::istringstream& stream)
{
    std::string line;
    std::getline(stream, line);
    std::istringstream iss(line);

    if (!(iss >> _type >> _path >> _version))   // moins de 3 mots ?
    {
        setError(400);
        return false;
    }

    // méthode connue ?
    if (_type != "GET" && _type != "POST" && _type != "DELETE")
    {
        setError(405);
        return false;
    }

    // version valide ?
    if (_version != "HTTP/1.1" && _version != "HTTP/1.0")
    {
        setError(400);
        return false;
    }

    // path commence par / ?
    if (_path.empty() || _path[0] != '/')
    {
        setError(400);
        return false;
    }

    return true;
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
        if (key.empty() || key.find_first_of(" \t") != std::string::npos)
            return false;
        size_t start = value.find_first_not_of(" \t");
        if (start == std::string::npos)
            value.clear();
        else
            value.erase(0, start);
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
        return false;
    std::istringstream stream(buff); // le creer en flux : permet de simplement recup chaque.

    if (!parse_request_line(stream))
        return false; // 1 ere ligne parser
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




bool Request::tryCgi()
{
    const Location* loc = getMatchedLocation();
    if (loc == NULL || loc->_cgiExtension.empty())
        return false;
    if (_path.find(loc->_cgiExtension) == std::string::npos)
        return false;

    std::size_t pos = _path.find("?");
    if (pos != std::string::npos)
    {
        _cgiQuery = _path.substr(pos + 1);
        _path = _path.substr(0, pos);
    }

    std::string scriptPath = _config.get_root() + _path;
    std::ifstream test(scriptPath.c_str());
    if (!test.is_open())
    {
        setError(404);
        return true;      // géré (erreur) — pas un CGI à lancer
    }
    test.close();

    _isCgi = true;              // ← drapeau : Server doit lancer le CGI
    _cgiInterpreter = loc->_cgiInterpreter;
    _cgiScriptPath = scriptPath;
    return true;
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
    
    if (!_path.empty() && _path[_path.size() - 1] == '/')
    {
        const Location* loc = getMatchedLocation();
        std::string indexFile = _config.get_index();

        if (loc != NULL && !loc->_index.empty())
            indexFile = loc->_index;
        
        if (!indexFile.empty())
            _path += indexFile;
    }


    // url parse pour cgi.. 
    std::string queryString = "";
    std::size_t pos = _path.find("?"); // 3 chemin : path + ? (sep) + param : /index.html?lang=fr
    if (pos != std::string::npos) // y'a un ? 
    {
        queryString = _path.substr(pos + 1); // param
        _path = _path.substr(0, pos); 
    }

        // cgi
    if (tryCgi())
        return true;

    const Location* loc = getMatchedLocation();
    std::string root = _config.get_root();

    if (loc != NULL && !loc->_root.empty())
        root = loc->_root;

    std::string full_path = root + _path;
    std::ifstream file(full_path.c_str()); // ouvre le 

    if (!file.is_open()) // si ca ouvre pas
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


bool Request::handle_post()
{
    if (_path.find("..") != std::string::npos)
    {
     setError(403);
        return false;
    }   
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

    if (tryCgi())
        return true;

    std::string full_path;
    const Location* loc = getMatchedLocation();
    if (loc != NULL && !loc->_upload.empty())
    {
        std::string relativePath = _path.substr(loc->_path.size());
        full_path = loc->_upload + relativePath;
    }   
    else
    {
    full_path = _config.get_root() + _path;
    }

    std::ofstream file(full_path.c_str(), std::ios::binary); // ouvre en ecriture.. 
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
    if (_path.find("..") != std::string::npos)
    {
        setError(403);
        return false;
    }

    const Location* loc = getMatchedLocation();
    std::string full_path;

    if (loc != NULL && !loc->_upload.empty())
    {
        std::string relativePath = _path.substr(loc->_path.size());
        full_path = loc->_upload + relativePath;
    }
    else
    {
        std::string root = _config.get_root();

        if (loc != NULL && !loc->_root.empty())
            root = loc->_root;

        full_path = root + _path;
    }

    if (std::remove(full_path.c_str()) == 0)
    {
        _status = 200;
        _body = "<h1>File deleted</h1>";
        return true;
    }

    setError(404);
    return false;
}

bool Request::is_method_allowed()
{
    // cherche la location qui correspond au chemin
    for (std::size_t i = 0; i < _config.getLocations().size(); i++)
    {
        // le chemin commence-t-il par le path de la location ?
        if (_path.find(_config.getLocations()[i]._path) == 0)
        {
            if (_config.getLocations()[i]._methods.empty())   // ← pas de restriction
                return true;
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

static std::string get_content_type(const std::string& path)
{
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html")
        return "text/html";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css")
        return "text/css";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js")
        return "application/javascript";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".png")
        return "image/png";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".jpg")
        return "image/jpeg";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".txt")
        return "text/plain";
    return "application/octet-stream";
}


std::string Request::build_response() // ajoute la structure http de la reponse pour que http comprenne
{
    std::stringstream response;
    std::string contentType = "text/html";

    if (_status == 200)
        response << "HTTP/1.1 200 OK\r\n";
    else if (_status == 201)
        response << "HTTP/1.1 201 Created\r\n";
    else if (_status == 301)                          
    {
        response << "HTTP/1.1 301 Moved Permanently\r\n";
        response << "Location: " << _redirectUrl << "\r\n";   // ← le header Location
    }
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
    else
        response << "HTTP/1.1 500 Internal Server Error\r\n";

    if (_status == 200)
        contentType = get_content_type(_path);
    response << "Content-Length: " << _body.size() << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << _body;

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
    std::map<int, std::string>::const_iterator it = _config.getError_pages().find(code); // demande a map get error de trouver code
    if (it != _config.getError_pages().end()) // si on a trouver
    {
        // charger le fichier custom
        std::string full_path = _config.get_root() + it->second; // le path complet
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




