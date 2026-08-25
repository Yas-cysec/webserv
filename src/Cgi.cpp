#include "Cgi.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>

Cgi::Cgi(const std::string& interpreter, const std::string& scriptPath, const std::string &method, 
    const std::string &queryString, const std::string &contentLen, const std::string &contentType,
    const std::string &pathInfo, const std::string &body): _interpreter(interpreter), _scriptPath(scriptPath), _methods(method), _queryString(queryString),
        _contentLen(contentLen), _contentType(contentType), _pathInfo(pathInfo), _body(body)
{

}


void Cgi::env_var(std::vector<std::string > &env)
{
    // methods
    env.push_back("REQUEST_METHOD=" + _methods);
    // tout ce qui y'a aores le ? 
    env.push_back("QUERY_STRING=" + _queryString);
    // CONTENT_LENGTH (taille du body, pour POST)
    env.push_back("CONTENT_LENGTH=" + _contentLen);
    // CONTENT_type 
    env.push_back("CONTENT_TYPE=" + _contentType);
    // path info
    env.push_back("PATH_INFO=" + _pathInfo);
    // script name
    env.push_back("SCRIPT_NAME=" + _pathInfo); // meme path..
}


bool Cgi::waitWithTimeout(pid_t pid)
{
    time_t start = time(NULL);
    int status;
    while (true)
    {
        pid_t result = waitpid(pid, &status, WNOHANG);// ne bloque pas
        if (result != 0)
            break;   // le script a fini
        if (time(NULL) - start > 5)   // 5 secondes écoulées ?
        {
            kill(pid, SIGKILL);        // tue le script
            waitpid(pid, &status, 0);  // nettoie le processus zombie
            return false;                 // timeout → erreur
        }
        usleep(1000); // pause
    }
    return true;
}












std::string Cgi::execute()
{
    int inpipefd[2]; // lire le contenu post pour enfant
    int outpipefd[2]; // ecrire post dans enfant.. 
    if (pipe(inpipefd) == -1 || pipe(outpipefd) == -1  )
        return "";

    char *args[] = {(char *)_interpreter.c_str(), (char *)_scriptPath.c_str(), NULL};
    std::vector<std::string > env;
    env_var(env);

    std::vector<char*> envp;
    for (std::size_t i = 0; i < env.size(); i++)
        envp.push_back((char*)env[i].c_str());
    envp.push_back(NULL); 
   
    pid_t pid = fork(); // creer l'enfant;
    if (pid == -1)
        return "";

    if (pid == 0) // dans l'enfant.. 
    {
        dup2(inpipefd[0], STDIN_FILENO);     // entrée = tube entrée
        dup2(outpipefd[1], STDOUT_FILENO); // sortie = tube sortie
       
        close(inpipefd[0]); 
        close(outpipefd[0]);
        close(inpipefd[1]);
        close(outpipefd[1]);

        execve(_interpreter.c_str(), args, &envp[0]);
        exit(1); // si ca echoue
    }
    else         // dans parent.. 
    {

        close(inpipefd[0]);
        close(outpipefd[1]);
         if (_methods == "POST")
            write(inpipefd[1], _body.c_str(), _body.size());
        close(inpipefd[1]); 

        char buffer[4096];
        int bytes;
        while ((bytes = read(outpipefd[0], buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes] = '\0';
            _contentHtml += buffer;
        }
        close (outpipefd[0]);
        if (!waitWithTimeout(pid))
            return "";
    }
    return (_contentHtml);
}


/*


ce que j'ai capter : 

en gros get n'a pas besoin d'info juste le script s'exec.. 
du coup on fait que stocker le fd[1] dans la sortie..
ensuite dans le parent on lis a partir du fd


dans enfant on config juste 
    inpipefd (il s'agis du contenu qu'on lis) (lecture est en entree..)
    dans parent on va ecrire dans inpipefd. 

    outpipe lui on lui dis que son ecriture est en sortie (car dois return le script)
    et dans le parent on lis a partir de output[0]... 




il y a un pb avec timeout et les loop.. 

le serveur lis avec read la sortie du sciot normalememnt quand read recois fini 
mais si y'a une boucle dans le script sans jamais finir le serv reste figer.. 
faut use epoll 
on rajoute simplement une obucle qui surveille 
*/