compile:
	gcc -Wall -g3 -fsanitize=address -pthread server.cpp -o server
	gcc -Wall -g3 -fsanitize=address -pthread client.cpp -o client