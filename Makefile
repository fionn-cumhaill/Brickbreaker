all: sample2D

sample2D: brickbreaker.cpp glad.c
	g++ -o BrickBreaker brickbreaker.cpp glad.c -lao -lmpg123 -lm -lGL -lglfw -ldl

debug := CFLAGS= -g

clean:
	rm sample2D
