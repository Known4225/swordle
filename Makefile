all:
	gcc swordle.c -L./Linux -lturtle -lglfw3 -ldl -lm -lX11 -lglad -lGL -lGLU -lpthread -DOS_LINUX -DDEBUGGING_FLAG -Wall -o swordle.o
rel:
	gcc swordle.c -L./Linux -lturtle -lglfw3 -ldl -lm -lX11 -lglad -lGL -lGLU -lpthread -DOS_LINUX -O3 -o swordle.o
lib:
	cp turtle.h turtlelib.c
	gcc turtlelib.c -c -DTURTLE_IMPLEMENTATION -DTURTLE_TEXT_PRETTY_PEN -DOS_LINUX -O3 -o Linux/libturtle.a
	rm turtlelib.c
win:
	gcc swordle.c -L./Windows -lturtle -lglfw3 -lopengl32 -lgdi32 -lglad -lole32 -luuid -DOS_WINDOWS -DDEBUGGING_FLAG -Wall -o swordle.exe
winrel:
	gcc swordle.c -L./Windows -lturtle -lglfw3 -lopengl32 -lgdi32 -lglad -lole32 -luuid -DOS_WINDOWS -O3 -o swordle.exe
winlib:
	cp turtle.h turtlelib.c
	gcc turtlelib.c -c -DTURTLE_IMPLEMENTATION -DTURTLE_TEXT_PRETTY_PEN -DOS_WINDOWS -O3 -o Windows/turtle.lib
	rm turtlelib.c
html:
	emcc swordleWeb.c --shell-file config/turtle_shell.html -sUSE_GLFW=3 -sMAX_WEBGL_VERSION=2 -sASYNCIFY -sINITIAL_MEMORY=1073741824 -sWASM=0 -DTURTLE_IMPLEMENTATION -DTURTLE_ENABLE_TEXTURES -DOS_BROWSER -Oz -o swordle.html --embed-file wordle-answers-alphabetical.txt --embed-file wordle-past-words-06.02.26.txt --embed-file wordle-valid-words.txt
	gcc config/combine.c -o combine.exe
	./combine.exe swordle.html
	rm combine.exe
htmlserver:
	emcc swordleWeb.c --shell-file config/turtle_shell.html -sUSE_GLFW=3 -sMAX_WEBGL_VERSION=2 -sASYNCIFY -sINITIAL_MEMORY=1073741824 -DTURTLE_IMPLEMENTATION -DTURTLE_ENABLE_TEXTURES -DOS_BROWSER -O3 -o swordle.html --embed-file wordle-answers-alphabetical.txt --embed-file wordle-past-words-06.02.26.txt --embed-file wordle-valid-words.txt
runserver:
	emrun swordle.html