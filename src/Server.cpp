#include "Server.hpp"
#include <sys/socket.h>

extern volatile sig_atomic_t g_running; // gestion du controle C 

Server::Server()
{
}

struct sockaddr_in makeaddr(int port)
{
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    return addr;

}

// -------------------------------------------------

bool Server::start(int port, const ServerConfig& config)
{
    std::map<int, int>::iterator it = _portToFd.find(port);

    if (it != _portToFd.end())
    {
        // Port déjà ouvert : on associe seulement la nouvelle config
        _fdToConfigs[it->second].push_back(config);
        return true;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0); // creer le fd de canal de comm 
    if (fd == -1)
    {
        std::cerr << "Erreur : socket impossible" << std::endl;
        return false;
    }
    fcntl(fd, F_SETFL, O_NONBLOCK); // ICI POUR LES NON BLOQUANT !!!!!!!!!
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) // permet de suppriemr le time wait de 30sec en fin de co
    {
        std::cerr << "setsockopt impossible" << std::endl;
        close(fd);
        return false;
    }
    struct sockaddr_in addr = makeaddr(port); // pour bind qui use cette struct

    if (bind (fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) // attacher port au canal socket fd
    {
        std::cerr << "Erreur : bind impossible sur le port " << port << std::endl;
        close(fd);
        return false;
    }
    if (listen(fd, SOMAXCONN) == -1) // ouvrir la co et port
    {
        std::cerr << "Erreur : listen impossible" << std::endl;
        close(fd);
        return false;
    }
    _serverFds.push_back(fd); // rajout de mon fd dans mon vector 
    _portToFd[port] = fd;
    _fdToConfigs[fd].push_back(config); // je lie le port a sa config.. 
    std::cout << "Serveur en ecoute sur le port " << port << std::endl;
    return true;
}



// -------------------------------------------------

int Server::epollHold() // init de epoll
{
    int epfd = epoll_create1(0);// creation de mon epoll qui renvoie fd 
    if (epfd == -1)
    {
        std::cerr << "Erreur : epoll_create1 impossible" << std::endl;
        return -1;
    }

    size_t i = 0;
    while (i < _serverFds.size()) // pour chaques fd 
    {
        struct epoll_event ev;
        ev.events = EPOLLIN; // pret a recevoir la lecture ?
        ev.data.fd = _serverFds[i];

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, _serverFds[i], &ev) == -1) // ajout dans la liste a surveiller
        {
            std::cerr << "Erreur : epoll_ctl impossible" << std::endl;
            close(epfd);
            return -1;
        }
        i++;
    }

    return epfd;
}
// -------------------------------------------------

bool Server::isServerFd(int fd)
{
    size_t i = 0;
    while (i < _serverFds.size())
    {
        if (_serverFds[i] == fd)
            return true;
        i++;
    }
    return false;
}


// -------------------------------------------------

void Server::acceptClient(int serverFd, int epfd)
{
    int clientFd = accept(serverFd, NULL, NULL); // client entre
    if (clientFd == -1)
    {
        std::cerr << "Erreur : accept impossible" << std::endl;
        return;
    }
    fcntl(clientFd, F_SETFL, O_NONBLOCK);  // ICI. NON BLOQUANT !!!
    _clientFds.push_back(clientFd);
    _clientToServerFd[clientFd] = serverFd;   // ← retient : ce client vient de cette porte
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = clientFd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, clientFd, &ev);
}
// -------------------------------------------------




bool Server::is_complete(const std::string& buffer) // request complete ?
{
    // 1. header finis ? 
    size_t pos = buffer.find("\r\n\r\n");
    if (pos == std::string::npos)
        return false;   // headers pas finis → pas complet

    // 2 : post avec body ? 
    std::string content_len = "Content-Length:";
    size_t pos_content_length = buffer.find(content_len);
    if (pos_content_length == std::string::npos)
        return true; // pas de contente lendgt -> pas de body -> complet (get delete)
    // 3 : POST : body a t'il atteint la taille annoncer ? 
    size_t clStart = pos_content_length + content_len.length();
    int contentLength = atoi(buffer.c_str() + clStart); // taille du body
    size_t bodyStart = pos + 4; // debut body apres \r etc.. 
    size_t bodyReceived = buffer.size() - bodyStart; 
    return bodyReceived >= (size_t)contentLength;   // body complet ?

}



ServerConfig Server::choose_config(const std::vector<ServerConfig>& configs, Request& req)
{
    if (configs.size() == 1)          // une seule config → pas de choix
        return configs[0];
    // plusieurs → départager par server_name (Host)
    std::string host = req.getHost();   // le Host de la requête
    for (std::size_t i = 0; i < configs.size(); i++)
    {
        if (configs[i].getServerName() == host)
             return configs[i];  // matche → on la prend
    }
     return configs[0];                  // aucun match → la première par défaut
}


// -------------------------------------------------

void Server::readClient(int clientFd, int epfd)
{
    char buffer[4096]; // stock  la requete car ecriture
    int bytes = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) // 0 = client parti  -1 = erreur
    {
        close (clientFd);
        _readBuffers.erase(clientFd);   // nettoie le buffer du client parti
        _responses.erase(clientFd);  // reponse en attente 
        _clientToServerFd.erase(clientFd); // nettoie 
        epoll_ctl(epfd, EPOLL_CTL_DEL, clientFd, NULL);  // arret de surveiller...  
        return;
    }
    buffer[bytes] = '\0';
    _readBuffers[clientFd] += buffer;

     if (!is_complete(_readBuffers[clientFd]))   // requête pas complète ?
        return;  // attend prochain 

    // Parser requetes 
    Request req;
    if (req.InitRequestParser(_readBuffers[clientFd]))
    {
        int serverFd = _clientToServerFd[clientFd];
        req.set_config(choose_config(_fdToConfigs[serverFd], req));
        req.act_request();
    }

    // NEW : si c un cgi on lance (non bloquant au lieu de repondre direct)
    if (req.isCgi())
    {
        startCgi(req, clientFd, epfd);
        _readBuffers[clientFd].clear();
        return;   // on ne répond pas maintenant, le CGI répondra plus tard
    }
    // sinon reponse nomrla comme avant 

    // stock reponse pour client 
    _responses[clientFd] = req.build_response();
    _readBuffers[clientFd].clear();            // vide pour la prochaine requête

    // basculuer ce client en EPOLLOUT (pret a ecrire)
    struct epoll_event ev;
    ev.events = EPOLLOUT;
    ev.data.fd = clientFd;
    epoll_ctl(epfd, EPOLL_CTL_MOD, clientFd, &ev);
}

//-----------------cgi--------------------

void Server::startCgi(Request& req, int clientFd, int epfd)
{
    int inpipe[2];
    int outpipe[2];

    if (pipe(inpipe) == -1)
        return;
    if (pipe(outpipe) == -1)
    {
        close(inpipe[0]);
        close(inpipe[1]);
        return;
    }

    // préparer args + env AVANT le fork
    std::string interp = req.getCgiInterpreter();
    std::string script = req.getCgiScriptPath();
    std::string scriptDirectory = ".";
    std::string scriptName = script;

    std::string::size_type slash = script.find_last_of('/');
    if (slash != std::string::npos)
    {
        scriptDirectory = script.substr(0, slash);
        scriptName = script.substr(slash + 1);
    }

    char* args[] = {
        (char*)interp.c_str(),
        (char*)scriptName.c_str(),
        NULL
    };

    std::vector<std::string> env;
    env.push_back("REQUEST_METHOD=" + req.getMethod());
    env.push_back("QUERY_STRING=" + req.getCgiQuery());
    env.push_back("CONTENT_LENGTH=" + req.getHeader("Content-Length"));
    env.push_back("CONTENT_TYPE=" + req.getHeader("Content-Type"));

    std::vector<char*> envp;
    for (std::size_t i = 0; i < env.size(); i++)
        envp.push_back((char*)env[i].c_str());
    envp.push_back(NULL);

    pid_t pid = fork();
    if (pid == -1)
    {
        close(inpipe[0]);
        close(inpipe[1]);
        close(outpipe[0]);
        close(outpipe[1]);
        return;
    }

    if (pid == 0) // enfant
    {
        if (chdir(scriptDirectory.c_str()) == -1)
            _exit(1);

        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);

        close(inpipe[0]);
        close(inpipe[1]);
        close(outpipe[0]);
        close(outpipe[1]);

        execve(interp.c_str(), args, &envp[0]);
        _exit(1);
    }

    // parent
    close(inpipe[0]);
    close(outpipe[1]);

    fcntl(inpipe[1], F_SETFL, O_NONBLOCK);

    // pipe sortie en NON-BLOQUANT
    fcntl(outpipe[0], F_SETFL, O_NONBLOCK);

    // enregistrer le CGI en cours
    CgiProcess proc;
    proc.clientFd = clientFd;
    proc.pid = pid;
    proc.startTime = time(NULL);
    proc.output = "";
    proc.inputFd = -1;
    _cgiProcesses[outpipe[0]] = proc;

    // ajouter le pipe sortie à epoll
    struct epoll_event outputEvent;
    outputEvent.events = EPOLLIN;
    outputEvent.data.fd = outpipe[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, outpipe[0], &outputEvent);

    std::string body = req.getBody();

    if (body.empty())
        close(inpipe[1]);
    else
    {
        _cgiProcesses[outpipe[0]].inputFd = inpipe[1];
        _cgiInputBuffers[inpipe[1]] = body;

        struct epoll_event inputEvent;
        inputEvent.events = EPOLLOUT;
        inputEvent.data.fd = inpipe[1];
        epoll_ctl(epfd, EPOLL_CTL_ADD, inpipe[1], &inputEvent);
    }
}

bool Server::isCgiPipe(int fd)
{
    return _cgiProcesses.find(fd) != _cgiProcesses.end();
}

void Server::readCgiOutput(int pipeFd, int epfd)
{
    CgiProcess& proc = _cgiProcesses[pipeFd];

    char buffer[4096];
    int bytes = read(pipeFd, buffer, sizeof(buffer) - 1);

    if (bytes > 0)
    {
        buffer[bytes] = '\0';
        proc.output += buffer;      // accumule, on n'a pas fini
        return;
    }

    // bytes <= 0 : le script a fini (pipe fermé)
    int clientFd = proc.clientFd;

    int status;
    waitpid(proc.pid, &status, 0);

    // construire la réponse HTTP avec la sortie du script
    std::ostringstream resp;

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        // script planté → 500
        std::string body = "<h1>Error 500</h1>";

        resp << "HTTP/1.1 500 Internal Server Error\r\n";
        resp << "Content-Length: " << body.size() << "\r\n";
        resp << "Content-Type: text/html\r\n";
        resp << "Connection: close\r\n";
        resp << "\r\n";
        resp << body;
    }
    else
    {
        std::string body = proc.output;
        std::string::size_type headerEnd;

        headerEnd = body.find("\r\n\r\n");

        if (headerEnd != std::string::npos)
            body.erase(0, headerEnd + 4);
        else
        {
            headerEnd = body.find("\n\n");

            if (headerEnd != std::string::npos)
                body.erase(0, headerEnd + 2);
        }

        resp << "HTTP/1.1 200 OK\r\n";
        resp << "Content-Length: " << body.size() << "\r\n";
        resp << "Content-Type: text/html\r\n";
        resp << "Connection: close\r\n";
        resp << "\r\n";
        resp << body;
    }

    _responses[clientFd] = resp.str();

    // nettoyer le CGI
    if (proc.inputFd != -1)
    {
        epoll_ctl(epfd, EPOLL_CTL_DEL, proc.inputFd, NULL);
        close(proc.inputFd);
        _cgiInputBuffers.erase(proc.inputFd);
    }

    epoll_ctl(epfd, EPOLL_CTL_DEL, pipeFd, NULL);
    close(pipeFd);
    _cgiProcesses.erase(pipeFd);

    // basculer le client en EPOLLOUT pour lui envoyer la réponse
    struct epoll_event ev;
    ev.events = EPOLLOUT;
    ev.data.fd = clientFd;
    epoll_ctl(epfd, EPOLL_CTL_MOD, clientFd, &ev);
}



void Server::checkCgiTimeouts(int epfd)
{
    std::map<int, CgiProcess>::iterator it = _cgiProcesses.begin();
    while (it != _cgiProcesses.end())
    {
        if (time(NULL) - it->second.startTime > 5)   // 5s dépassées
        {
            int pipeFd = it->first;
            int clientFd = it->second.clientFd;

            kill(it->second.pid, SIGKILL);           // tue le script
            waitpid(it->second.pid, NULL, 0);

            // réponse 504 (timeout)
            std::string resp = "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 0\r\n\r\n";
            _responses[clientFd] = resp;

            epoll_ctl(epfd, EPOLL_CTL_DEL, pipeFd, NULL);
            close(pipeFd);

            struct epoll_event ev;
            ev.events = EPOLLOUT;
            ev.data.fd = clientFd;
            epoll_ctl(epfd, EPOLL_CTL_MOD, clientFd, &ev);

            std::map<int, CgiProcess>::iterator toErase = it;
            ++it;
            _cgiProcesses.erase(toErase);   // retire le CGI tué
        }
        else
            ++it;
    }
}


void Server::writeCgiInput(int pipeFd, int epfd)
{
    std::map<int, std::string>::iterator it;
    it = _cgiInputBuffers.find(pipeFd);

    if (it == _cgiInputBuffers.end())
        return;

    std::string& body = it->second;
    int written = write(pipeFd, body.c_str(), body.size());

    if (written > 0)
        body.erase(0, written);

    if (written <= 0 || body.empty())
    {
        epoll_ctl(epfd, EPOLL_CTL_DEL, pipeFd, NULL);
        close(pipeFd);
        _cgiInputBuffers.erase(pipeFd);

        for (std::map<int, CgiProcess>::iterator process = _cgiProcesses.begin();
             process != _cgiProcesses.end(); ++process)
        {
            if (process->second.inputFd == pipeFd)
            {
                process->second.inputFd = -1;
                break;
            }
        }
    }
}



//----------------------------------------------------

void Server::run() // fonction principal qui lance l'ecoute
{
    int epfd = epollHold(); // init epoll avec le fd 
    if (epfd == -1)
        return ;
    struct epoll_event events[100]; // stock les fd prets (100 max)

    while (g_running) // debut boucle infini
    {
        int n = epoll_wait(epfd, events, 100, 1000);  // stocke les fd prets en info dans le tab
        int i = 0;
        while (i < n) // parcours tout les fd
        {
            int fd = events[i].data.fd; // recupere le fd
            if (_cgiInputBuffers.find(fd) != _cgiInputBuffers.end())
            {
                if (events[i].events & EPOLLOUT)
                    writeCgiInput(fd, epfd);
            }
            // pipe de sortie CGI prêt pour être lu
            else if (_cgiProcesses.find(fd) != _cgiProcesses.end())
            {
                readCgiOutput(fd, epfd);
            }
            else if (isServerFd(fd)) // si le fd est un port normal connu
                    acceptClient(fd, epfd);
            else if ((events[i].events & EPOLLOUT))// c un client deja connu
                    sendResponse(fd, epfd);
            else 
                readClient(fd, epfd); // c forcement un client 
            i++;
        }
        checkCgiTimeouts(epfd);
    }
    close (epfd);
}


void Server::sendResponse(int clientFd, int epfd)
{
    std::string &response = _responses[clientFd];
    int sent = send(clientFd, response.c_str(), response.size(), 0);

    if (sent <= 0)   // erreur (-1) ou connexion fermée (0) → RETIRER le client
    {
        close(clientFd);
        _responses.erase(clientFd);
        _readBuffers.erase(clientFd);
        _clientToServerFd.erase(clientFd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, clientFd, NULL);
        return;
    }

    response.erase(0, sent);   // enlève ce qui a été envoyé
    if (response.empty())      // tout envoyé ?
    {
        _responses.erase(clientFd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, clientFd, NULL);
        close(clientFd);
    }
}


// -------------------------------------------------

/*
    1) debuter un socket de connexion par rapport au port du server
    2) attacher le port au canal de communication 
    3) ensuite on lance une ecoute

    4) attendre une conexxion avec poll
        creation de epoll 
        affiliation des socket cibles
        boucle wait  
    5) accepter la reponses 
    6 ) close le socket (canal de co)


1) permettre la connexion
2) 


socket c quoi ? 
obj reseau 
il sert de canal de communication

ca renvoie a un numero de canal et on peux use les fonction comme ca 





problematique : 

c epoll et l'utilisation a repetition de fonction tant que c finis... 
*/