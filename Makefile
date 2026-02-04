all:
	gcc swordle.c -L./Linux -lturtle -lglfw3 -ldl -lm -lX11 -lglad -lGL -lGLU -lpthread -DOS_LINUX -DDEBUGGING_FLAG -Wall -o swordle.o
rel:
	gcc swordle.c -L./Linux -lturtle -lglfw3 -ldl -lm -lX11 -lglad -lGL -lGLU -lpthread -DOS_LINUX -O3 -o swordle.o
lib:
	cp turtle.h turtlelib.c
	gcc turtlelib.c -c -DTURTLE_IMPLEMENTATION -DTURTLE_TEXT_DO_DYNAMIC_Y_CENTERING -DTURTLE_TEXT_PRETTY_PEN -DOS_LINUX -O3 -o Linux/libturtle.a
	rm turtlelib.c
win:
	gcc swordle.c -L./Windows -lturtle -lglfw3 -lopengl32 -lgdi32 -lglad -lole32 -luuid -DOS_WINDOWS -DDEBUGGING_FLAG -Wall -o swordle.exe
winrel:
	gcc swordle.c -L./Windows -lturtle -lglfw3 -lopengl32 -lgdi32 -lglad -lole32 -luuid -DOS_WINDOWS -O3 -o swordle.exe
winlib:
	cp turtle.h turtlelib.c
	gcc turtlelib.c -c -DTURTLE_IMPLEMENTATION -DTURTLE_TEXT_DO_DYNAMIC_Y_CENTERING -DTURTLE_TEXT_PRETTY_PEN -DOS_WINDOWS -O3 -o Windows/turtle.lib
	rm turtlelib.c