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

class Server
{
    private :
        ServerConfig _config;
        std::vector<int> _serverFds;
        std::vector<int> _clientFds;
        std::map<int, std::string> _responses;   // clientFd → sa réponse à envoyer
            std::map<int, std::string> _readBuffers;   // fd → ce qu'on a accumulé

    public : 
        Server(const ServerConfig &config);
        bool start(int port); // debut init
        int epollHold(); // init de epoll
        void run(); // gestion du programme
        void sendResponse(int clientfd, int epfd);

    private : 
        bool isServerFd(int fd);
        void acceptClient(int serverFd, int epfd);
        void readClient(int clientFd, int epfd);
        bool is_complete(const std::string& buffer);
};


#endif