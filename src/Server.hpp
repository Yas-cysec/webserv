#ifndef _SERVER_HPP_
# define _SERVER_HPP_

#include "Config.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <vector>
#include <sys/epoll.h>
#include <unistd.h>     // close
#include <map>          // std::map
#include <fcntl.h>
#include "Request.hpp"
#include <cstdlib>
#include <sys/wait.h>
#include <csignal>
#include <ctime>

class Server
{
    private :
        std::map<int, std::vector<ServerConfig> > _fdToConfigs; // permet dávoir plus config differente selon server a stocker
        std::vector<int> _serverFds;
        std::vector<int> _clientFds;
        std::map<int, std::string> _responses;   // clientFd → sa réponse à envoyer
        std::map<int, std::string> _readBuffers;   // fd → ce qu'on a accumulé
        std::map<int, int> _clientToServerFd;   // clientFd → la porte d'où il vient
        std::map<int, CgiProcess> _cgiProcesses;


    public :
        Server();
        bool start(int port, const ServerConfig& config); // debut init
        int epollHold(); // init de epoll
        void run(); // gestion du programme
        void sendResponse(int clientfd, int epfd);
        bool isServerFd(int fd);
        void acceptClient(int serverFd, int epfd);
        void readClient(int clientFd, int epfd);
        bool is_complete(const std::string& buffer);
        ServerConfig choose_config(const std::vector<ServerConfig>& configs, Request& req);

        // cgi
        bool isCgiPipe(int fd);
        void readCgiOutput(int fd, int epfd);
        void startCgi(Request& req, int clientFd, int epfd);
        void checkCgiTimeouts(int epfd)
};


#endif