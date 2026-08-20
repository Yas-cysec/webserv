#include "Cgi.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>

Cgi::Cgi(const std::string& interpreter, const std::string& scriptPath)
    : _interpreter(interpreter), _scriptPath(scriptPath) 
{

}

std::string Cgi::execute()
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
        return "";

    pid_t pid = fork(); // creer l'enfant;
    if (pid == -1)
        return "";

    if (pid == 0) // dans l'enfant.. 
    {
        close(pipefd[0]); // enfant pas besoin de lire
        dup2(pipefd[1], STDOUT_FILENO); // sortie c le tuyau..
        close(pipefd[1]);

        char *args[] = {(char *)_interpreter.c_str(), (char *)_scriptPath.c_str(), NULL};
        execve(_interpreter.c_str(), args, NULL);
        exit(1); // si ca echoue
    }

    else         // dans parent.. 
    {
        close(pipefd[1]); // parent n'ecrit pas il doit lire.. 
        char buffer[4096];
        int bytes;
        while ((bytes = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes] = '\0';
            _contentHtml += buffer;
        }
        close (pipefd[0]);
        waitpid(pid, NULL, 0);
    }
    return (_contentHtml);
}


