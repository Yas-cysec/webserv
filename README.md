*This project has been created as part of the 42 curriculum by ybouzekr and insriji.*

# Webserv
 
## Description

Webserv is an HTTP/1.1 server written from scratch in C++98. It handles multiple clients simultaneously through a single non-blocking event loop (epoll), and serves static websites, file uploads, and dynamic content via CGI scripts.

The server is configured through a configuration file (inspired by the NGINX `server` block syntax), which defines the ports to listen on, the routes, the allowed methods, error pages, body-size limits, redirections, directory listing, and CGI execution.

Main features:
- Non-blocking I/O driven by a single `epoll` instance (sockets and CGI pipes)
- GET, POST and DELETE methods
- Accurate HTTP status codes and default/custom error pages
- Static file serving and file uploads
- Directory listing (autoindex) and default index files
- HTTP redirections
- Per-route configuration (allowed methods, root, upload location, CGI)
- Client body-size limit
- CGI execution (Python) for GET and POST, with error and timeout handling
- Listening on multiple ports to serve different content

## Instructions
 
### Compilation

```
make
```
 
The Makefile provides the standard rules: `all`, `clean`, `fclean`, `re`. The code compiles with `-Wall -Wextra -Werror -std=c++98`.

### Execution
 
```
./webserv conf/config.conf
```


The server takes a single argument: the path to a configuration file.
 
### Configuration
 
The configuration file defines one or more `server` blocks. Example:
```
server {
    listen 8080;
    server_name localhost;
    root www;
    index index.html;
    client_max_body_size 1000000;
    error_page 404 /erreurs/404.html;
 
    location /files {
        allow_methods GET;
        autoindex on;
    }
 
    location /upload {
        allow_methods POST DELETE;
        upload uploads;
    }
 
    location /cgi-bin {
        cgi .py /usr/bin/python3;
    }
}
```

Supported directives:
- `listen` — port to listen on
- `server_name` — server name
- `root` — root directory for files
- `index` — default file served for a directory
- `client_max_body_size` — maximum request body size in bytes
- `error_page <code> <path>` — custom error page
- `location <path> { ... }` — per-route rules: `allow_methods`, `autoindex on|off`, `return <url>`, `root`, `index`, `upload <dir>`, `cgi <ext> <interpreter>`


### Testing
 
```
curl http://localhost:8080/
curl http://localhost:8080/cgi-bin/test.py
curl -X POST http://localhost:8080/cgi-bin/post.py -d "name=value"
siege -b -t10s http://localhost:8080/
```
 
## Resources

The resources below are the ones that actually helped while building this
project, grouped by topic.

### Sockets & non-blocking I/O

The networking foundation of the server: how to open sockets, bind, listen,
accept connections, and handle many clients at once with a single event loop.

- https://nginx.org/en/docs/beginners_guide.html (parsing nginx style)
- Beej's Guide to Network Programming — the classic, beginner-friendly guide
  to sockets in C: https://beej.us/guide/bgnet/
- Video explaining I/O multiplexing with `select()` (the same idea applies to
  `poll` and `epoll`): https://www.youtube.com/watch?v=Y6pFtgRdUts
- `man` pages: `epoll`, `socket`, `bind`, `listen`, `accept`, `fcntl`, `recv`,
  `send`

### CGI

How the server runs an external program (Python) to generate dynamic content,
and how information about the request is passed to it through environment
variables.

- CGI environment variables (IBM docs):
  https://www.ibm.com/docs/fr/netcoolomnibus/8.1.0?topic=scripts-environment-variables-in-cgi-script
- CGI environment variables (Wikipedia):
  https://fr.wikipedia.org/wiki/Variables_d%27environnement_CGI
- RFC 3875 — the CGI specification:
  https://datatracker.ietf.org/doc/html/rfc3875
- `man` pages: `fork`, `execve`, `pipe`, `dup2`, `waitpid`

### HTTP protocol

Understanding request/response format, methods, status codes and headers.

- MDN — HTTP overview (clear and practical):
  https://developer.mozilla.org/en-US/docs/Web/HTTP/Overview
- MDN — HTTP response status codes:
  https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
- RFC 7230 / RFC 7231 — HTTP/1.1 message syntax and semantics

### Configuration file

The config syntax is inspired by the NGINX `server` block structure.

- NGINX documentation — server and location blocks:
  https://nginx.org/en/docs/http/ngx_http_core_module.html

### Use of AI

AI was used as a learning and pair-programming aid during development, mainly
to explain concepts (non-blocking I/O with epoll, the fork/exec/pipe mechanism
behind CGI) and to help structure and debug specific parts of the code.
