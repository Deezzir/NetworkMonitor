#include <iostream>
#include <string>
#include <signal.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <fstream>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "params.h"

static void signal_handler(int sig);
void get_interfaces();
void socket_setup();
void network_monitor();
void exit_handler(int ev, void *arg);

char** interfaces { nullptr }; //2d char array to store interfaces got from a user
pid_t* child_pids { nullptr }; //array to store children PIDs
int* child_fds { nullptr }; //array to store children FDs
size_t num_child { 0 }; //number of children spawned
    
char buffer[BUF_LEN];
bool is_running;
bool is_parent;
int master_fd;

int main() {
    if(getuid()) {
        std::cerr << "NetworkMonitor: the program must be run with root privileges" << std::endl;
        exit(EXIT_FAILURE);
    }
    //Setup exit handler
    on_exit(exit_handler, NULL);

    //Set up a signal handler to terminate the program gracefully
    struct sigaction action;
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if(sigaction(SIGINT, &action, NULL) < 0) {
        print_error((char*)"Error while setting action for a signal", true);
    }

    //Get interfaces from the user
    get_interfaces();

    //Setup socket
    socket_setup();

    is_running = true;  
    is_parent = true;
    child_pids = new pid_t[num_child]; //allocate memory
    int key_fd = open(key_file, O_RDONLY); //open keyfile

    for (size_t i = 0; i < num_child && is_parent; i++) {
        if(ioctl(key_fd, RNDZAPENTCNT, 0) < 0) { //Set entropy to zero
            std::cout << strerror(errno) << std::endl;
        }
        child_pids[i] = fork(); //Fork child
        if(child_pids[i] == 0) {
            #ifdef DEBUG
                std::cout << "NetworkMonitor child: PID - " << getpid() << std::endl;
            #endif
            is_parent = false;
            close(master_fd); //close copied fd
            close(key_fd); //close copied fd
            execlp(interface_monitor, interface_monitor, interfaces[i], NULL); //execute file
            print_error((char*)"Error while executing child file", false); //should not get here
        }
    }
    if(is_parent) {
        network_monitor(); //actual monitoring 
    }
    
    close(key_fd);

    std::cout << "NetworkMonitor(" << getpid() << "): finished" << std::endl;

    return 0;
}

/*Signal Handler is responsible for*/
/*handling SIGINT siganl */
static void signal_handler(int sig) {
    switch (sig) {
    case SIGINT:
        std::cout << "NetworkMonitor: SIGINT signal received" << std::endl;
        if(is_running) { //Check if monitoring was started
            is_running = false;
        } else {
            exit(EXIT_FAILURE); //Exit if SIGINT was sent during user input
        }
        break;
    
    default:
        std::cout << "NetworkMonitor: undefined signal received" << std::endl;
        break;
    }
}

/*Get Interfaces function is responsible for*/
/*taking and validating user input related to interfaces*/
void get_interfaces() {
    std::string intf;
    char interface_path[BUF_LEN];

    std::cout << "How many interfaces do you want to monitor: ";
    size_t num_interfaces = get_int_in_range(1, QUEUE); //get number of interface ranging from 1 to QUEUE

    interfaces = new char*[num_interfaces]{ nullptr }; //Allocate memory for array
    num_child = num_interfaces; //set global var

    for (size_t i = 0; i < num_interfaces; i++) {
        std::cout << "Enter interface " << i+1 << ": ";
        while (true) {
            std::cin >> intf;

            memset(interface_path, 0, sizeof(interface_path));
            sprintf(interface_path, "/sys/class/net/%s", intf.c_str()); //Get interface path 
            #ifdef DEBUG
                std::cout << i+1 << "interface_path: " << interface_path << std::endl;
            #endif
            if(!file_exists(interface_path)) { //Check if filepath exists
                std::cout << "The interface entered does not exist. Try again: ";
            } else {
                interfaces[i] = new char[intf.length() + 1]; //Allocate memory for interface
                strcpy(interfaces[i], intf.c_str());
                #ifdef DEBUG
                    std::cout << "interfaces[" << i << "] " << interfaces[i] << std::endl;
                #endif
                break;
            }
        }   
    }
}

/*Socket Setup function is responsible for*/
/*creating socket and linking it to the file in /tmp */
void socket_setup() {
    struct sockaddr_un master_addr;

    //Create the socket
    memset(&master_addr, 0, sizeof(master_addr));
    if((master_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) { //setup global fd
        print_error((char*)"Error while creating the socket", true);
    }

    master_addr.sun_family = AF_UNIX;
    //Set the socket path to a local socket file
    strncpy(master_addr.sun_path, socket_path, sizeof(master_addr.sun_path)-1);
    #ifdef DEBUG
        std::cout << "NetworkMonitor: server_addr.sun_path - " << server_addr.sun_path << endl;
    #endif

    #ifdef DEBUG
        std::cout << "NetworkMonitor: bind()" << std::endl;
    #endif
    //Bind socket 
    if(bind(master_fd, (struct sockaddr*)&master_addr, sizeof(master_addr)) < 0) {
        print_error((char*)"Error while binding the socket", true);
    }

    #ifdef DEBUG
        std::cout << "NetworkMonitor: listen()" << std::endl;
    #endif
    std::cout << "NetworkMonitor(" << getpid() << "): waiting for the interfaces..." << std::endl;
    //Start listening for a new connection
    if(listen(master_fd, QUEUE) == -1) {
        print_error((char*)"Error while listening", true);
    }
}

/*Network Monitor is responsible for*/
/*accepting and managing connection on the socket*/
void network_monitor() {
    fd_set active_fd_set; //set for the active FDs
    fd_set read_fd_set; //set for ready to be read FDs
    int counter { 0 }, ret, len; //Local counter 

    FD_ZERO(&read_fd_set); //zeroth the set
    FD_ZERO(&active_fd_set); //zeroth the set
    FD_SET(master_fd, &active_fd_set); //Add the master_fd to the socket set

    int max_fd = master_fd; //Sockets will be selected from max-fd + 1
    child_fds = new int[num_child];

    while(is_running) {
        //Block until an input arrives on one or more sockets
        read_fd_set = active_fd_set;
        if(select(max_fd+1, &read_fd_set, NULL, NULL, NULL) >= 0) { //select connection
            //Service all the sockets with input pending
            if(FD_ISSET(master_fd, &read_fd_set) && counter <= num_child) { //Connection request on the master socket
                if((child_fds[counter] = accept(master_fd, NULL, 0)) >= 0) {
                    std::cout << "NetworkMonitor: starting the monitor for the interface " << interfaces[counter] << std::endl;
                    std::cout << "NetworkMonitor: incoming connection " << child_fds[counter] << std::endl; 
                    FD_SET(child_fds[counter], &active_fd_set);

                    receive(child_fds[counter], buffer);
                    #ifdef DEBUG
                        std::cout << "NetworkMonitor: received '" << buffer << "'" << std::endl;
                    #endif

                    if(strcmp(buffer, "ready") == 0) {
                        send(child_fds[counter], buffer, "monitor"); //start interface monitor
                        receive(child_fds[counter], buffer);
                        #ifdef DEBUG
                            std::cout << "NetworkMonitor: received '" << buffer << "'" << std::endl;
                        #endif
                    }

                    if(max_fd < child_fds[counter]) max_fd = child_fds[counter];
                    ++counter;
                } else {
                    print_error((char*)"Error while accepting connection on the socket", false);        
                }
            } else {
                for (int i = 0; i < num_child; i++) {//Find which client sent the data
                    if (FD_ISSET(child_fds[i], &read_fd_set) && is_running) {
                        memset(buffer, 0, BUF_LEN);
                        receive(child_fds[i], buffer);

                        if(strcmp(buffer, "done") == 0) { //close connection if client is done
                            FD_CLR(child_fds[i], &active_fd_set);
                            close(child_fds[i]);
                        } else if(strcmp(buffer, "link_down") == 0) { //check if link is down
                            send(child_fds[i], buffer, "link_up"); //set up link 
                        } else {
                            std::cout << buffer << std::endl;
                        }
                
                    }
                }
            }
        }
        sleep(1);
    }

    //kill children
    for (size_t i = 0; i < num_child; i++) {
        kill(child_pids[i], SIGINT);
        sleep(1);
        FD_CLR(child_fds[i], &active_fd_set);
        close(child_fds[i]);
    }    
}

/* Exit Handler that is responsible for releasing locks */
/* and dynamically allocated memory */
void exit_handler(int ev, void *arg) {
    #ifdef DEBUG
        std::cout << "closing file descriptors" << std::endl;
    #endif
    //Close file descriptors
    close(master_fd);
    //Remove the socket file from /tmp
    unlink(socket_path);

    //Release dynamically allocated memory
    if(interfaces != nullptr) {
        #ifdef DEBUG
            std::cout << "deleting interfaces" << std::endl;
        #endif
        for (size_t i = 0; i < num_child; i++)
            if(interfaces[i] != nullptr)
                delete[] interfaces[i];
        delete[] interfaces;
    } else {
        #ifdef DEBUG
            std::cout << "interfaces already deallocated" << std::endl;
        #endif
    }

    if(child_pids != nullptr) {
        #ifdef DEBUG
            std::cout << "deleting child_pids" << std::endl;
        #endif
        delete[] child_pids;
    } else {
        #ifdef DEBUG
            std::cout << "child_pids already deallocated" << std::endl;
        #endif
    }

    if(child_fds != nullptr) {
        #ifdef DEBUG
            std::cout << "deleting child_fds" << std::endl;
        #endif
        delete[] child_fds;
    } else {
        #ifdef DEBUG
            std::cout << "child_fds already deallocated" << std::endl;
        #endif
    }
}