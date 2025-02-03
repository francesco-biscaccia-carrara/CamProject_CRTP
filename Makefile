all: Cclient Cserver

Cclient: cam_client.c ext_lib/render_sdl2.c
	${CC} -O0 -g3 -I/usr/include/SDL2/ $^ -o $@ -lm -lSDL2 -lSDL2_image -lGL

Cserver: cam_server.c	
	${CC} -O0 -g3 $^ -o $@

clean:
	rm -f Cclient Cserver
