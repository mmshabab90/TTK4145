#include "elevator.h"
#include "common.h"
//#include "backup.h"
#include "timer.h"
#include "tcp_client.h"
#include "tcp_server.h"

#include <stdio.h> // printf 
#include <pthread.h> // POSIX threads
#include <semaphore.h> // for what?
#include <string.h>
#include <unistd.h> // sleep
#include <stdlib.h> // system 

// Debug
#include "debug.h"

static Elevator elevator;

void* button_monitor() {
    int floor_result;
    while (!elev_get_stop_signal()) {
        floor_result = elev_get_floor_sensor_signal();
        if (floor_result != -1 && floor_result != elevator.floor)
            elevator.floor = floor_result;
        common_set_lamps();
        elevator_set_lamps(&elevator);
        for (int flr = 0; flr < N_FLOORS; flr++){
            if (elev_get_button_signal(BUTTON_CALL_UP,flr) == 1 && flr != N_FLOORS -1){
                if (!common_get_request(flr, 0)) {
                    common_set_request(flr, 0, 1);
                    tcp_common_call('u', 'r', flr);
                }
            }
            if (elev_get_button_signal(BUTTON_CALL_DOWN,flr) == 1 && flr != 0){
                if (!common_get_request(flr, 1)) {
                    common_set_request(flr, 1, 1);
                    tcp_common_call('d', 'r', flr);
                }
            }
            if (elev_get_button_signal(BUTTON_COMMAND,flr) == 1){
                if (elevator.call[flr] != 1) {
                    elevator.call[flr] = 1;
                }
            }
        }
        if (elev_get_stop_signal()){
            elev_set_motor_direction(DIRN_STOP);
            elevator.state = STOPPED;
        }
    }
    return 0;
}

void initialize(){
    printf("Initializing... ");
    elev_init(ET_Comedi); // ET_Comedi or ET_Simulation
    elev_set_motor_direction(DIRN_STOP);
    elevator_initialize(&elevator);
    //common_initialize(common_request);
    printf("done\n");
}

void main_test() {
    initialize();

    // backup
    // char message[1024] = "Still alive!";
    // struct timeval message_timer;
    // timer_set(&message_timer, 100);
    tcp_server_init();
    pthread_t button_monitor_t;
    pthread_t tcp_server_test_t;
    pthread_create(&button_monitor_t,NULL,button_monitor,"Processing...");
    pthread_create(&tcp_server_test_t,NULL,tcp_server_test,"Processing...");
    //sleep(1);
    tcp_client_init();
    Elevator_State prev_state = elevator.state;
    int prev_floor = elevator.floor;
    int state_iterator = 1;
    //debug_print_state(&state_iterator, &elevator, 1);
    while (!elev_get_stop_signal()) {

        //backup
        // if (timer_timeout(&message_timer)) {
        //     sendMessage(message);
        //     timer_set(&message_timer, 100);
        // }

        if (elevator.state != prev_state) {
            //debug_print_state(&state_iterator, &elevator, 1);
            prev_state = elevator.state;
        }
        switch (elevator.state) {
            case IDLE:
            //current floor call or request in same direction
            if (common_get_request(elevator.floor, elevator.direction) == 2 || elevator.call[elevator.floor]) {
                elevator_door_open(&elevator);
                elevator.state = DOORS_OPEN;
                elevator.call[elevator.floor] = 0;
                common_set_request(elevator.floor, elevator.direction, 0);
                char button = elevator.direction ? 'd' : 'u';
                tcp_common_call(button, 'c', elevator.floor);
            }
            else if (elevator_should_advance(&elevator)) {
                prev_floor = elevator.floor;
                elevator_move(&elevator);
                elevator.state = MOVING;
            }
            else
                elevator.direction = !elevator.direction;
            break;
            case MOVING:
            if (elevator.floor != prev_floor && elevator_should_stop(&elevator)) {
                elevator_stop();
                if (common_get_request(elevator.floor, elevator.direction) == 2) {
                    elevator_door_open(&elevator);
                    elevator.state = DOORS_OPEN;
                    elevator.call[elevator.floor] = 0;
                    common_set_request(elevator.floor, elevator.direction, 0);
                    char button = elevator.direction ? 'd' : 'u';
                    tcp_common_call(button, 'c', elevator.floor);
                }
                else
                    elevator.state = IDLE;
            } 
            break;
            case DOORS_OPEN:
            if (elevator_door_closed(&elevator)) {
                if (elevator.call[elevator.floor] || common_get_request(elevator.floor, elevator.direction) == 2) {
                    elevator.call[elevator.floor] = 0;
                    common_set_request(elevator.floor, elevator.direction, 0);
                    char button = elevator.direction ? 'd' : 'u';
                    tcp_common_call(button, 'c', elevator.floor);
                    elevator_door_open(&elevator);
                }
                else if (elevator_should_advance(&elevator)) {
                    prev_floor = elevator.floor;
                    elevator_move(&elevator);
                    elevator.state = MOVING;
                }
                else
                    elevator.state = IDLE;
            }
            break;
            case STOPPED:
            break;
        }
    }
    tcp_client_kill();
    pthread_join(button_monitor_t,NULL); //Kill monitor
    pthread_join(tcp_server_test_t, NULL);
    elev_set_motor_direction(DIRN_STOP);
}

int main(int argc, char* argv[]){
    if (argc != 2) {
        printf("  -m\t\trun as master\n");
        printf("  -s\t\trun as slave\n");
    }
    else {
        if (argv[1][0] == '-') {
            switch (argv[1][1]) {
                case 'm':
                break;
                main_test();
                case 's':
                tcp_client_init();
                button_monitor();
                break;
                default:
                printf("  -m\t\trun as master\n");
                printf("  -s\t\trun as slave\n");
                break;
            }
        }
        else
            printHelp();
    }
    return 0;






    /*
    if (argc != 2) {
        printHelp();
    }
    else {
        if (argv[1][0] == '-') {
            switch (argv[1][1]) {
                case 'm':
                system("gnome-terminal -e \"./elevator -b\"");
                main_test();
                break;
                case 'b':
                runBackup();
                system("clear & gnome-terminal -e \"./elevator -b\"");
                main_test();
                //main_test();
                break;
                default:
                printHelp();
                break;
            }
        }
        else
            printHelp();
    }
    return 0;
    */
}
