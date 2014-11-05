# TODO

* SDL_Flip
* GetTicks
* GetKeyState
* GetTime
* Clock_GetTime
* open
* Gamepad
* Mouse
* Desync detection (hashing draw calls)
* Multiple people

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

