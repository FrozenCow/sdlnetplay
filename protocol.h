#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <SDL/SDL.h>

namespace sdlnetplay {
enum { PACKET_SYNC, PACKET_EVENT };
typedef uint8_t PacketType;

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

SDL_Event toSDLEvent(const NPEvent *event);
NPEvent fromSDLEvent(const SDL_Event *event);

extern int fd;
extern bool is_server;

void netplay_listen();
void netplay_connect();

void write_event(int fd, NPEvent *event);
}

#endif
