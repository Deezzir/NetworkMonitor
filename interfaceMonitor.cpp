/**********************************************
Assignment 1

interfaceMonitor.cpp - interface information gatherer

Course: UNX511
Last Name: Kondrakov
First Name: Iurii
ID: 113202196
Section: NSA
This assignment represents my own work in accordance with Seneca Academic Policy.
Date: 11/19/2021
**********************************************/

#include <fcntl.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "params.h"

static void signal_handler(int signal); //siganl handler
void socket_setup(); //socket setuper
void get_statistics(char* data);  // statistics gatherer

char buffer[BUF_LEN];
char interface[IFNAMSIZ];

int client_fd;
bool is_running;

int main(int argc, char const *argv[]) {
    //The interface must be passed as an argument
    if (argc != 2) {
        std::cerr << "InterfaceMonitor: invalid number of arguments" << std::endl;
        exit(EXIT_FAILURE);
    }

    int entropy; //entropy counter
    bool permitted { true }; //permission flag

    int key_fd = open(key_file, O_RDONLY);
    if(ioctl(key_fd, RNDGETENTCNT, &entropy) < 0) { //check if called by parent
        std::cout << strerror(errno) << std::endl;
        permitted = false;
    } 
    if(entropy > 10) {
        std::cout << "InterfaceMonitor: Permission not granted" << std::endl;
        permitted = false;
    }
    close(key_fd);

    if(permitted) {
        char data[320];
        int ret, len;
        
        //Set up a signal handler to terminate the program gracefully
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = &signal_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;

        if(sigaction(SIGINT, &action, NULL) != 0) {
            print_error((char*)"Error while setting action for a signal", true);
        }

        strncpy(interface, argv[1], BUF_LEN); //The interface has been passed as an argument

        //Setup socket connection
        socket_setup();

        //Send "ready" message
        send(client_fd, buffer, "ready");
        //Receive "monitor" message
        receive(client_fd, buffer);


        //If message is "monitor" then start monitoring
        if(strcmp("monitor", buffer) == 0) {
            //Send "monitoring" message
            send(client_fd, buffer, "monitoring");    

            is_running = true;
            while(is_running) {
                get_statistics(data); //get interface statistics

                if(!is_link_up(interface)) { //check if link is down
                    send(client_fd, buffer, "link_down"); //notify networkMonitor
                    receive(client_fd, buffer); //wait for signal to set link up
                    if(strcmp("link_up", buffer) == 0) {
                        set_link_up(interface, 1); // set link up
                    }
                }

                send(client_fd, buffer, data); //send interface statistics
                sleep(1);
            }
        }

        //Send "done" message
        send(client_fd, buffer, "done");
        close(client_fd);
    }

    std::cout << "InterfaceMonitor(" << getpid() << "): finished" << std::endl;

    return 0;
}

/*Signal Handler is responsible for*/
/*handling SIGINT siganl */
static void signal_handler(int signal) {
    switch (signal) {
    case SIGINT:
        if(is_running) { //avoid double message if ctrl-c used which send SIGINT to the group
            std::cout << "InterfaceMonitor: SIGINT signal received" << std::endl;
            is_running = false;
        }
        break;
    
    default:
        std::cout << "InterfaceMonitor: undefined signal received" << std::endl;
        break;
    }
}

/*Socket Setup function is responsible for*/
/*connecting to the socket file in /tmp */
void socket_setup() {
    struct sockaddr_un client_addr;

    memset(&client_addr, 0, sizeof(client_addr));
    //Create Socket
    if((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        print_error((char*)"Error while creating the socket", true);
    }

    client_addr.sun_family = AF_UNIX;
    //Set the socket path to a local socket file
    strncpy(client_addr.sun_path, socket_path, sizeof(client_addr.sun_path)-1);
    #ifdef DEBUG
        std::cout << "Client: server_addr.sun_path - " << server_addr.sun_path << std::endl;
    #endif

    #ifdef DEBUG
        std::cout << "Client: connect()" << std::endl;
    #endif
    //Connect client to the server
    if (connect(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        close(client_fd);
        print_error((char*)"Error while connecting to a server", true);
    }
}

/*Get Statistics function is responsible for*/
/*gathering statistics from given inteface*/
/*putting information into data*/
void get_statistics(char* data) {
    char interface_path[BUF_LEN];

    std::string operstate;
    unsigned int carrier_up_count { 0 };
    unsigned int carrier_down_count { 0 };

    unsigned int tx_bytes { 0 };
    unsigned int rx_bytes { 0 };
    unsigned int tx_packets { 0 };
    unsigned int rx_packets { 0 };
    unsigned int tx_dropped { 0 };
    unsigned int rx_dropped { 0 };
    unsigned int tx_errors { 0 };            
    unsigned int rx_errors { 0 };

    std::ifstream infile;
    sprintf(interface_path, "/sys/class/net/%s/operstate", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> operstate;
        infile.close();
    }
    
    sprintf(interface_path, "/sys/class/net/%s/carrier_up_count", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> carrier_up_count;
        infile.close();
    }
    
    sprintf(interface_path, "/sys/class/net/%s/carrier_down_count", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> carrier_down_count;
        infile.close();
    }

    sprintf(interface_path, "/sys/class/net/%s/statistics/tx_bytes", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> tx_bytes;
        infile.close();
    }
    
    sprintf(interface_path, "/sys/class/net/%s/statistics/rx_bytes", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> rx_bytes;
        infile.close();
    }
    
    sprintf(interface_path, "/sys/class/net/%s/statistics/tx_packets", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> tx_packets;
        infile.close();
    }
    
    sprintf(interface_path, "/sys/class/net/%s/statistics/rx_packets", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> rx_packets;
        infile.close();
    }

    sprintf(interface_path, "/sys/class/net/%s/statistics/tx_dropped", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> tx_dropped;
        infile.close();
    }
    
    sprintf(interface_path, "/sys/class/net/%s/statistics/rx_dropped", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> rx_dropped;
        infile.close();
    }

    sprintf(interface_path, "/sys/class/net/%s/statistics/tx_errors", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> tx_errors;
        infile.close();
    }
    
    sprintf(interface_path, "/sys/class/net/%s/statistics/rx_errors", interface);
    infile.open(interface_path);
    if(infile.is_open()) {
        infile >> rx_errors;
        infile.close();
    }

    sprintf(data, "Interface:%s state:%s up_count:%d down_count:%d\n"
        "rx_bytes:%d rx_dropped:%d rx_errors:%d rx_packets:%d\n"
        "tx_bytes:%d tx_dropped:%d tx_errors:%d tx_packets:%d\n",
        interface, operstate.c_str(), carrier_up_count, carrier_down_count, rx_bytes,
        rx_dropped, rx_errors, rx_packets, tx_bytes, tx_dropped, tx_errors, tx_packets);
}