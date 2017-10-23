# sdlnetplay

sdlnetplay allows one to share a game session with another person. This will allow hotseat multiplayer games to be played over the internet.

Both players start the same game and sdlnetplay makes sure the games are simulated deterministically, passes input to eachother and synchronizes frame.

sdlnetplay sits between the game and SDL, OpenGL and the system. It does so by overriding calls to these APIs. For instance, it uses `SDL_GL_SwapBuffers` and `glBegin` to synchronize frames. It makes sure the `time`, `clock` and `gettimeofday` functions always return the same value for both systems. It also makes sure `rand` returns a pseudo-random value that is the same for each client.

It uses `LD_PRELOAD` to override the API functions.

This project goes with a disclaimer that it is **highly experimental**. Most games will not work due to non-deteministic behavior that `sdlnetplay` cannot handle, usually caused by the game-logic relying on different threads. In addition, `sdlnetplay` currently has a nasty way to detect desyncs by storing screenshots to disk, hashing the file and send over and compare the hash of the other system.

Also note that because most Linux games are compiled to 32-bit binaries, this library also compiles to 32-bit by default, otherwise it would be incompatible with the game and will not be able to preload.

## Games

Games that have shown to work:

* Gish

Games that have issues:

* Aquaria
  * Desyncs after audio dialogs, which runs on a separate thread

Games that break down quickly:

* FTL

## Requirements

* OpenGL
* SDL 1.2
* libc6-dev-i386
* imagemagick

## Building

```
make
```

## Usage

On one system host a game:

```
SDLNETPLAY_LISTEN=1 LD_PRELOAD=/path/to/sdlnetplay.so /path/to/game_executable
```

On another system you need to connect to the host:

```
SDLNETPLAY_CONNECT=1 SDLNETPLAY_HOSTNAME=hostname.of.other.player LD_PRELOAD/path/to/sdlnetplay.so /path/to/game_executable
```

The protocol uses TCP port `8008`, so make sure the client can reach the host on this port.

