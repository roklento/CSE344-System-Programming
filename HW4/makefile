all: server client
	
server: server.c
	gcc -o server server.c -lpthread -lrt
	
client: client.c	
	gcc -o client client.c -lpthread -lrt

clean:
	rm -f server
	rm -f client