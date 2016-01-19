 # Arquivo: MakeFile
 # Autores: 
 # André Matheus Nicoletti - 12643797
 # Hugo Marques Casarini - 12014742
 # Thamer El Ayssami - 12133302
 # Yago Elias - 12146007
 #
CC=gcc
CFLAGS = -Iconfig/ -Ilink_layer/ -Inet_layer/ -Inet_layer/frag/ -Inet_layer/list/
CFLAGS += -Itransport_layer/ -Itest_layer -Inet_layer_test -Itransport_layer_test
CFLAGS += -I. -Itransport_layer/ack/ -Itransport_layer/segment/ -Ichecksum/ -Iapplication_layer/
CFLAGS += -Igarbler/ -Ilogging/ -lpthread

all: main

debug: CFLAGS += -DDEBUG -g
debug: main


lost: CFLAGS += -DDEBUG -DLOST -g
lost: main

corrupt: CFLAGS += -DDEBUG -DDILMA -g
corrupt: main

duplicate: CFLAGS += -DDEBUG -DDUBLE -g
duplicate: main

yolo: CFLAGS += -DDEBUG -DYOLO -g
yolo: main

list: net_layer/list/net_list.c
	$(CC) -c net_layer/list/net_list.c -o net_list.o $(CFLAGS)

frag: net_layer/frag/frag.c
	$(CC) -c net_layer/frag/frag.c -o frag.o $(CFLAGS)

net: net_layer/net_layer.c
	$(CC) -c net_layer/net_layer.c -o net_layer.o $(CFLAGS)

log: logging/logger.c
	$(CC) -c logging/logger.c -o logger.o $(CFLAGS)

checksum: checksum/checksum.c
	$(CC) -c checksum/checksum.c -o checksum.o $(CFLAGS)

config: config/config.c
	$(CC) -c config/config.c -o config.o $(CFLAGS)

garbler: garbler/garbler.c
	$(CC) -c garbler/garbler.c -o garbler.o $(CFLAGS)
	
link: link_layer/link.c
	$(CC) -c link_layer/link.c -o link.o $(CFLAGS)

test_layer: test_layer/test_layer.c
	$(CC) -c test_layer/test_layer.c -o test_layer.o $(CFLAGS)
	
net_layer_test: net_layer_test/net_layer_test.c
	$(CC) -c net_layer_test/net_layer_test.c -o net_layer_test.o $(CFLAGS)
	
transport: transport_layer/transport_layer.c
	$(CC) -c transport_layer/transport_layer.c -o transport.o $(CFLAGS)
	
ack: transport_layer/ack/ack_lst.c
	$(CC) -c transport_layer/ack/ack_lst.c -o ack_lst.o $(CFLAGS)
	
segment: transport_layer/segment/seg_lst.c
	$(CC) -c transport_layer/segment/seg_lst.c -o seg_lst.o $(CFLAGS)

transport_layer_test: transport_layer_test/transport_layer_test.c
	$(CC) -c transport_layer_test/transport_layer_test.c -o transport_layer_test.o $(CFLAGS)
	
application_layer: application_layer/application_layer.c
	$(CC) -c application_layer/application_layer.c -o application_layer.o $(CFLAGS)

project: project.c
	$(CC) -c project.c -o project.o $(CFLAGS)

main: list frag log config checksum garbler link project net test_layer net_layer_test transport ack segment transport_layer_test application_layer
	$(CC) net_list.o frag.o logger.o config.o checksum.o garbler.o link.o test_layer.o net_layer_test.o net_layer.o transport.o ack_lst.o seg_lst.o transport_layer_test.o application_layer.o project.o -o project $(CFLAGS)

clean:
	rm *.o

