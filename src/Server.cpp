#include "Server.hpp"
#include <sys/socket.h>

Server::Server(const ServerConfig& config) : _config(config)
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

bool Server::start(int port)
{
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
    struct sockaddr_in addr = makeaddr(port);

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
    while (i < _serverFds.size()) // pour chaques socket 
    {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = _serverFds[i];

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, _serverFds[i], &ev) == -1) // pour chaques socket on lui precise qui surveiller 
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
    int contentLength = atoi(buffer.c_str() + clStart); // taille du cl
    size_t bodyStart = pos + 4; // debut body apres \r etc.. 
    size_t bodyReceived = buffer.size() - bodyStart; 
    return bodyReceived >= (size_t)contentLength;   // body complet ?

}




// -------------------------------------------------

void Server::readClient(int clientFd, int epfd)
{
    char buffer[4096]; // stock  la requete car ecriture
    int bytes = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) // 0 = client parti,  -1 = erreur
    {
        close (clientFd);
        _readBuffers.erase(clientFd);   // nettoie le buffer du client parti
        _responses.erase(clientFd);  // reponse en attente 
        epoll_ctl(epfd, EPOLL_CTL_DEL, clientFd, NULL);  // arret de surveiller...  
        return;
    }
    buffer[bytes] = '\0';
    _readBuffers[clientFd] += buffer;   // ← ACCUMULE au lieu de traiter direct

     if (!is_complete(_readBuffers[clientFd]))   // requête pas complète ?
        return;  // attend prochain 

    // Parser requetes 
    Request req;
    if (!req.InitRequestParser(_readBuffers[clientFd]))   // parsing échoue ?
        {
            req.setError(400);                // → prépare une 400
        }
    else
        {
            req.act_request();                // sinon, traite normalement
        }         
    // stock reponse pour client 
    _responses[clientFd] = req.build_response();
    _readBuffers[clientFd].clear();            // vide pour la prochaine requête

    // basculuer ce client en EPOLLOUT (pret a ecrire)
    struct epoll_event ev;
    ev.events = EPOLLOUT;
    ev.data.fd = clientFd;
    epoll_ctl(epfd, EPOLL_CTL_MOD, clientFd, &ev);
}



// -------------------------------------------------

void Server::run() // fonction principal qui lance l'ecoute
{
    int epfd = epollHold(); // init epoll avec le fd 
    if (epfd == -1)
        return ;
    struct epoll_event events[64];

    while (true) // debut boucle infini
    {
        int n = epoll_wait(epfd, events, 64, -1); // recupere le nb
        int i = 0;
        while (i < n) // parcours tout les fd
        {
            int fd = events[i].data.fd; // recupere le fd
            if (isServerFd(fd)) // si le fd est un port normal connu
                {
                    acceptClient(fd, epfd); // accept la co (car c un futur client)
                }
            else if ((events[i].events & EPOLLOUT))// c un client deja connu
                {
                    sendResponse(fd, epfd);
                }
            else 
                readClient(fd, epfd);
            i++;
        }
    }

}

void Server::sendResponse(int clientFd, int epfd)
{
    std::string &response = _responses[clientFd];
    int sent = send(clientFd, response.c_str(), response.size(), 0);
    if (sent <= 0) // erreur
        return; //reesaie prochain tour..
    response.erase(0, sent);   // enlève ce qui a deja été envoyé
    if (response.empty()) // tout envoyer ? 
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