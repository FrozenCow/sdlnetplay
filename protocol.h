#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <vector>
#include <queue>

#include <stdint.h>
#include <SDL/SDL.h>

#include "utils.h"

namespace sdlnetplay {

extern int fd;
extern bool is_server;

typedef struct {
    uint8_t scancode;
    uint32_t sym;
    uint32_t mod;
    uint16_t unicode;
} KeySym;

typedef struct {
    uint8_t type;
    uint8_t state;
    KeySym keysym;
} KeyboardEvent;

typedef union {
    uint8_t type;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    KeyboardEvent key;
} NPEvent;

typedef struct {
    uint32_t framenumber;
    std::vector<NPEvent> events;
} SyncPacket;

typedef struct {
    char name[1024];
} FSyncPacket;



typedef uint8_t ChannelId;

class ChannelQueue {
public:
    ChannelId id;
    virtual void read_and_queue_packet() = 0;
};

extern std::vector<ChannelQueue*> channels;


template <typename T>
class Channel : public ChannelQueue {
public:
    std::queue<T> pending;
    virtual void read_packet(T*) = 0;
    virtual void read_and_queue_packet() = 0;
    virtual void write_packet(T &packet) = 0;
};

class SyncChannel : public Channel<SyncPacket> {
    void read_packet(SyncPacket *packet);
    void read_and_queue_packet();
    void write_packet(SyncPacket &packet);
};

class FSyncChannel : public Channel<FSyncPacket> {
    void read_packet(FSyncPacket *packet);
    void read_and_queue_packet();
    void write_packet(FSyncPacket &packet);
};

template <typename T>
void read_from_channel(Channel<T> * channel, T *packet) {
    if (!channel->pending.empty()) {
        *packet = channel->pending.front();
        channel->pending.pop();
        return;
    }
    while(true) {
        ChannelId channelIndex;
        if(read_until(fd, &channelIndex, sizeof(channelIndex)) < 0) {
            die("Failed to read channel id");
        }
        if(channelIndex >= channels.size()) {
            fprintf(stderr, "Received channel id: %d\n", channelIndex);
            die("Received invalid channelId");
        }
        if (channelIndex != channel->id) {
            channels[channelIndex]->read_and_queue_packet();
        } else {
            channel->read_packet(packet);
            return;
        }
    }
}

template<typename T>
void write_to_channel(Channel<T> * channel, T & packet) {
    if(write_until(fd, &channel->id, sizeof(channel->id)) < 0) {
        die("Failed to write channel id");
    }
    channel->write_packet(packet);
}


SDL_Event toSDLEvent(const NPEvent *event);
NPEvent fromSDLEvent(const SDL_Event *event);

extern SyncChannel *syncChannel;
extern FSyncChannel *fsyncChannel;

void netplay_listen();
void netplay_connect();

}
#endif
