#include "tcp_server.h"
#include "common.h"
#include "elevator.h"
#include "manager.h"
#include "elev.h"       // obstruction kill

#include "tcp_client.h" // DEFAULT_PORT

#include <stdio.h>
#include <string.h>     // strlen
#include <stdlib.h>     // strlen
#include <sys/socket.h>
#include <arpa/inet.h>  // inet_addr
#include <unistd.h>     // write
#include <pthread.h>    // for threading

static int socket_desc , client_sock , c , *new_sock;
static struct sockaddr_in server , client;

pthread_mutex_t elevator_lock;

//move?
static int remote_elevator_count = 0;

//debug
void d_print_remote_elevators() {
    pthread_mutex_lock(&elevator_lock);
    printf("\n> remote_elevator_ count = %i\n", remote_elevator_count);
    for (int i = 0; i < MAX_ELEVATORS; i++) {
        printf("| remote_elevator[%2d]:\t", i);
        if (manager_get_remote_elevator(i)->active) {
            printf("rank:%3i\tip: %s", manager_get_remote_elevator(i)->rank, manager_get_remote_elevator(i)->ip);
        }
        else {
            printf("(inactive)");
        }
        printf("\n");
    }
    printf("\n");
    pthread_mutex_unlock(&elevator_lock);
}

void tcp_server_init()
{
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( DEFAULT_PORT );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("bind failed. Error");
        exit(1);
    }
    puts("bind done");
     
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
}

void *tcp_server_test() {
    for (int i = 0; i < MAX_ELEVATORS; i++)
        manager_get_remote_elevator(i)->active = 0;
	while((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) && !elev_get_obstruction_signal()) {
        puts("Connection accepted");
         
        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = client_sock;
         
        if( pthread_create( &sniffer_thread , NULL ,  elevator_connection_handler , (void*) new_sock) < 0) {
            perror("could not create thread");
            return (int*)1;
        }
        else { 
            puts("Created thread");
        }
         
        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( sniffer_thread , NULL);
        
        pthread_detach(sniffer_thread);
        printf("Handler assigned: %d\n\n", client_sock);
        remote_elevator_count++;
    }
     
    if (client_sock < 0) {
        perror("accept failed");
        return (int*)1;
    }
    return 0;
}
 
 
// This will handle connection for each client
void *elevator_connection_handler(void *socket_desc) {
    //Get the socket descriptor
    int sock = *(int*)socket_desc;


    int read_size;
    //char *message
    char client_message[255];
    
    // add elevator
    int elevator_id = add_remote_elevator(sock, inet_ntoa(client.sin_addr));

    //add sock to rank here? ----------------------------------------------------

    //get ip string
    //manager_get_remote_elevator(elevator_id)->ip = inet_ntoa(client.sin_addr);

    //DEBUG PRINT
    d_print_remote_elevators();

    //message = "Now type something and i shall repeat what you type \n";
    //write(sock , message , strlen(message));
     
    //Receive a message from client
    int direction;
    int floor;
    int ownership;
    while ((read_size = recv(sock , client_message , 255 , 0)) > 0 && !elev_get_obstruction_signal()) {
        //Send the message back to client
        //write(sock , client_message , strlen(client_message));
        /*switch (client_message[0]) {
            case 'b':
            if (client_message[1] == 'r') {
                printf("Elevator called ");
            }
            else if (client_message[1] == 'c') {
                printf("Elevator completed ");
            }
            switch(client_message[2]) {
                case 'u':
                printf("up");
                break;
                case 'd':
                printf("down");
                break;
            }
            printf(" on floor %c\n", client_message[3]);
            break;
        }*/

        //debug
        printf("Recieved C/S: %s\n", client_message);

        switch (client_message[0]) {
        	case 'b': // button assignment
        	direction = client_message[2] == 'd';
        	floor = client_message[3] - '0';
            if (client_message[1] == 'c')
        	    common_set_request(floor, direction, 0);
            else {
                ownership = manager_assign(floor, direction);
                //char ownership_str[3];
                common_set_request(floor, direction, ownership);
                sprintf(client_message, "%03d", ownership);
                //bzero(client_message);
                //for (int i = 0; i < 3; i++)

            }
        	break;

            case 'p': // polling
            switch (client_message[1]) {
                case 'r': // rank
                sprintf(client_message, "%03d", sock);
                break;
                case 'a': // assignments (common_request)
                bzero(client_message, 255);
                for (int flr = 0; flr < N_FLOORS; flr++) { // limits N_FLOORS to 127 because of message size 255
                    client_message[flr*2] = '0' + common_get_request(flr, 0);
                    client_message[flr*2 + 1] = '0' + common_get_request(flr, 1);

                    //debug
                    //printf("%i\t%i\n", common_get_request(flr, 0), common_get_request(flr, 1));

                }
                break;
                case 'n':
                bzero(client_message, 255);
                int next_master_remote_elevator_id = remote_elevator_count > 1;
                strncpy(client_message, manager_get_remote_elevator(next_master_remote_elevator_id)->ip, 16);
                break;
            }
            break;

            case 'u': // update
            switch (client_message[1]) {
                case 's': // status
                switch (client_message[2]) {
                    case 'i':
                    manager_get_remote_elevator(elevator_id)->state = 0;
                    break;
                    case 'm':
                    manager_get_remote_elevator(elevator_id)->state = 1;
                    break;
                    case 'd':
                    manager_get_remote_elevator(elevator_id)->state = 2;
                    break;
                    case 's':
                    manager_get_remote_elevator(elevator_id)->state = 3;
                    break;
                }
                manager_get_remote_elevator(elevator_id)->direction = client_message[3] == 'd';
                char floor_str[3];
                strncpy(floor_str, client_message + 4, 3);
                manager_get_remote_elevator(elevator_id)->floor = atoi(floor_str);
                break;
            }
            break;
        }

        //debug
        printf("Sending  S/C: %s\n", client_message);
        
        write(sock , client_message , strlen(client_message));
    }
     
    if(read_size == 0)
    {
        puts("Client disconnected");
        remote_elevator_count--;

        //debug
        //printf("-----------Elevator count: %i\n", remote_elevator_count);

        delete_remote_elevator(elevator_id);

        //debug
        d_print_remote_elevators();

        free(socket_desc);
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("recv failed");
    }
         
    //Free the socket pointer
    //delete_remote_elevator(elevator_id);
    //free(socket_desc);
    
    printf("Socket freed: %d\n", sock);
    
    return 0;
}

int add_remote_elevator(int socket, const char* ip) {
    int elevator_id = -1;
    pthread_mutex_lock(&elevator_lock);
    for (int i = 0; i < MAX_ELEVATORS; i++) {

        if (!manager_get_remote_elevator(i)->active) {
            manager_get_remote_elevator(i)->active = 1;
            manager_get_remote_elevator(i)->rank = socket;
            manager_get_remote_elevator(i)->ip = ip;
            elevator_id = i;
            break;
        }
    }
    pthread_mutex_unlock(&elevator_lock);
    return elevator_id;
}

void delete_remote_elevator(int elevator_id){
    pthread_mutex_lock(&elevator_lock);
    manager_get_remote_elevator(elevator_id)->active = 0;
    int i = elevator_id + 1;
    if (manager_get_remote_elevator(i)->active)
        while(manager_get_remote_elevator(i + 1)->active && i < MAX_ELEVATORS - 1)
            i++;
    *manager_get_remote_elevator(elevator_id) = *manager_get_remote_elevator(i);
    manager_get_remote_elevator(i)->active = 0;
    pthread_mutex_unlock(&elevator_lock);

    // Reassign all elevator requests
    for (int flr = 0; flr < N_FLOORS; flr++) {
        for (int dir = 0; dir < 2; dir++) {
            if (common_get_request(flr, dir) > 0) {
                printf("common_get_request(%d, %d) = %d;\n", flr, dir, common_get_request(flr, dir));
                common_set_request(flr, dir, manager_assign(flr, dir));
            }
        }
    }
}