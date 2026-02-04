# Swordle

Wordle solver

Run `swordle.exe`

You can also build the binary yourself with
```
gcc swordle.c -L./Windows -lturtle -lglfw3 -lopengl32 -lgdi32 -lglad -lole32 -luuid -lwsock32 -lWs2_32 -lMf -lMfplat -lmfreadwrite -lmfuuid -DOS_WINDOWS -O3 -o swordle.exe
```
on windows or
```
gcc swordle.c -L./Linux -lturtle -lglfw3 -ldl -lm -lX11 -lglad -lGL -lGLU -lpthread -DOS_LINUX -O3 -o swordle.o
```
on linux

Use the keyboard to enter words, click on letters to cycle through colors, press enter to calculate possible words and calculate a best next word

# TODO
- Optimize simulation to not have to redo work on non-changing top words
- Optimize the program so it runs faster
- Collect gambits
- Save and load files?

# Images

![example](images/example.jpg)
