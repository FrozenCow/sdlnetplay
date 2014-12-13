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
SyncChannel *syncChannel;
FSyncChannel *fsyncChannel;
std::vector<ChannelQueue*> channels;

void write_event(int fd, NPEvent *event) {
    write_until(fd, event, sizeof(NPEvent));  
}

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

bool inited = false;
void netplay_init() {
    if(inited) {
        die("netplay was already initialized");
    }
    channels.push_back(syncChannel = new SyncChannel());
    channels.push_back(fsyncChannel = new FSyncChannel());
    channels[0]->id = 0;
    channels[1]->id = 1;
    inited = true;
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

    netplay_init();
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

    netplay_init();
}


void SyncChannel::read_packet(SyncPacket *packet) {
    if(read_until(fd, &packet->framenumber, sizeof(packet->framenumber)) < 0) {
        die("Failed to read framenumber");
    }
    uint16_t count;
    if(read_until(fd, &count, sizeof(count)) < 0) {
        die("Failed to read event count");
    }
    //packet->events.resize(count);
    while(packet->events.size() < count) {
        NPEvent event;
        if(read_until(fd, &event, sizeof(event)) < 0) {
            die("Failed to read event");
        }
        packet->events.push_back(event);
    }
}

void SyncChannel::write_packet(SyncPacket &packet) {
    if(write_until(fd, &packet.framenumber, sizeof(packet.framenumber)) < 0) {
        die("Failed to write frame number");
    }
    uint16_t count = (uint16_t)packet.events.size();
    if(write_until(fd, &count, sizeof(count)) < 0) {
        die("Failed to write event count");
    }
    for(size_t i=0;i<packet.events.size();i++) {
        if(write_until(fd, &(packet.events[i]), sizeof(NPEvent)) < 0) {
            die("Failed to write event");
        }
    }
}

void SyncChannel::read_and_queue_packet() {
    SyncPacket packet;
    read_packet(&packet);
    pending.push(packet);
}

void FSyncChannel::read_packet(FSyncPacket *packet) {
    uint32_t len;
    if (read_until(fd, &len, sizeof(len)) < 0) {
        die("Failed to read FSync packet len");
    }
    packet->mName.resize(len);
    if(read_until(fd, packet->ptr(), (size_t)len) < 0) {
        die("Failed to read FSync packet name");
    }
    
}

void FSyncChannel::write_packet(FSyncPacket &packet) {
    uint32_t len = packet.mName.size();
    if (write_until(fd, &len, sizeof(len)) < 0) {
        die("Failed to write FSyncPacket");
    }
    if (write_until(fd, packet.ptr(), sizeof(char)*len) < 0) {
        die("Failed to write FSyncPacket");
    }
}

void FSyncChannel::read_and_queue_packet() {
    FSyncPacket msg;
    read_packet(&msg);
    pending.push(msg);
}

void set_cork() {
    // Cork TCP
    int state = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
}

void unset_cork() {
    // Uncork TCP
    int state = 0;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
}

}
