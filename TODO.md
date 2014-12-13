# Games

* Hotline Miami
* Crayon Physics Deluxe
* Case Story+
> Braid
* Aquaria
* McPixel
* Monaco
* Penumbra
* Shank (local coop)
* Snapshot
* VVVVVV
* Voxatron
* World of Goo
* Super Meat Boy

# TODO
* Waren bezig met aquaria
* open
* Gamepad
* Desync detection (hashing draw calls)
* Multiple people
* De tijd synchroniseren bij sync

# DONE
* time
* gettimeofday
* GetTime
* Clock_GetTime
* GetKeyState
* Mouse
* SDL_Flip
* GetTicks
* Experiment with disabling address space layout randomization (does this create consistent address layouts on different machines?) (It doesn't)

## ASLR (Address Space Layout Randomization)
Disabling ASLR via setarch `uname -m` -R <program> could eliminate an annoying source of desynchronization. Example:
    ldd /usr/bin/ls
vs
    setarch `uname -m` -R ldd /usr/bin/ls

## SDL_Flip
same as SDL_GL_SwapBuffers ?

## SDL_GetTicks / GetTime / Clock_GetTime
? Not sure 

## GetKeyState
fill an array from events that also go into the circbuffer now, either in SDL_PollEvent or in SDL_Flip/SDL_GL_SwapBuffers

## open
    _open()
    read file
    hash file
    send hash
    recv hash
    hashes differ:
      send file    |    recv  file
    else
      done
