#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include "protocol.h"
#include "utils.h"

namespace sdlnetplay {

int fd;
bool is_server = false;

SDL_Event toSDLEvent(const NPEvent *event) {
    SDL_Event result;
    result.type = event->type;
    switch(event->type) {
    case SDL_KEYUP:
    case SDL_KEYDOWN:
        result.key.state = event->key.state;
        result.key.keysym.scancode = event->key.keysym.scancode;
        result.key.keysym.sym = (SDLKey)event->key.keysym.sym;
        result.key.keysym.mod = (SDLMod)event->key.keysym.mod;
        result.key.keysym.unicode = event->key.keysym.unicode;
        break;
    case SDL_MOUSEMOTION:
        result.motion = event->motion;
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        result.button = event->button;
        break;
    default:
        break;
    }
    return result;
}

NPEvent fromSDLEvent(const SDL_Event *event) {
    NPEvent result;
    result.type = event->type;
    switch(event->type) {
        case SDL_KEYUP:
        case SDL_KEYDOWN:
            result.key.state = event->key.state;
            result.key.keysym.scancode = event->key.keysym.scancode;
            result.key.keysym.sym = event->key.keysym.sym;
            result.key.keysym.mod = event->key.keysym.mod;
            result.key.keysym.unicode = event->key.keysym.unicode;
            break;
        case SDL_MOUSEMOTION:
            result.motion = event->motion;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            result.button = event->button;
            break;
        default:
            break;
    }
    return result;
}

void netplay_listen() {
    is_server = true;
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        die("Error creating socket");
    }
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8008);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    int optval = 1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    if (bind(serverfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        die("Error binding socket");
    }
    if (listen(serverfd,1) < 0) {
        die("Error listening on socket");
    }
    


    fprintf(stdout,"Accepting clients...\n");
    fd = accept(serverfd, (struct sockaddr *) NULL, NULL);
    if (fd < 0) {
        die("Error accepting socket");
    }

    { // Disable Nagle buffering algorithm.
        int flag = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    }
    
    fprintf(stdout,"Accepted client.\n");
}

void netplay_connect() {
    is_server = false;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("Error creating socket");
    }
    struct hostent *server = gethostbyname(getenv("SDLNETPLAY_HOSTNAME"));
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(8008);
    fprintf(stdout,"Connecting to server...\n");
    if (connect(fd,(const sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
        die("Error connecting to address");
    }
    fprintf(stdout,"Connected\n");
    

    { // Disable Nagle buffering algorithm.
        int flag = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    }
}

void write_event(int fd, NPEvent *event) {
    write_until(fd, event, sizeof(NPEvent));  
}

}
