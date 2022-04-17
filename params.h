#ifndef PARAMS_H
#define PARAMS_H

#include <iostream>
#include <sys/stat.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/random.h>

#define BUF_LEN 350 //Buffer Length
#define QUEUE 10 //Maximum number of connections

const char socket_path[] { "/tmp/networkMonitor" }; //path to the socket file
const char interface_monitor[] { "./interfaceMonitor" }; //interface monitor executable name
const char key_file[] { "/dev/urandom" }; //key file path 

/*Print Error function is responsible for*/
/*printing error messages and exiting on demand*/
void print_error(char* msg, bool with_exit = false) {
    perror(msg);
    if(with_exit) //exit if demanded
        exit(EXIT_FAILURE);
}

/*Clear stdin buffer*/
void clear() {
    while (getchar() != '\n') {
        ;
    }
}

/*Get Int function is responsible for*/
/*receiving int input from a user and validating it*/
int get_int() {
    bool done{false};
    int value;

    done = scanf("%d", &value);
    clear();

    while (done == false) {
        std::cout << "The value entered is not a number. Try again: ";
        done = scanf(" %d", &value);
        clear();
    }

    return value;
}

/*Get Int in Range function is responsible for*/
/*receiving int input in the range from the user and validating it*/
/*Function uses get_int() as a base*/
int get_int_in_range(int min, int max) {
    bool done{false};
    int value{-1};

    do {
        value = get_int();
        if (value > max || value < min) {
            std::cout << "You entered the wrong number. Please, enter the number between " << min << " and " << max << ": ";
        } else {
            done = !done;
        }
    } while (!done);

    return value;
}

//Inline function to check if file path is exists
inline bool file_exists (const char* pathname) {
    struct stat buffer;   
    return (stat (pathname, &buffer) == 0); 
}

/*Send function is a wrapper for the socket send */
void send(int fd, char* buffer, const char* msg) {
    int len, ret;
    len = sprintf(buffer, msg, NULL)+1;
    if((ret = send(fd, buffer, len, 0)) == -1) {
        print_error((char*)"Error while sending", false);
    }
    #ifdef DEBUG
	    std::cout << "Sent "<< ret <<" bytes" << std::sndl;
    #endif
}

/*Receive function is a wrapper for the socket receive */
void receive(int fd, char* buffer) {
    int ret;
    if((ret = recv(fd, buffer, BUF_LEN, 0)) == -1) {
        print_error((char*)"Error while receiving", false);
    }
    #ifdef DEBUG
	    std::cout<<"Received "<< ret <<" bytes" << std::sndl;
    #endif
}

/*Function is responsible for*/
/*checking if a given interface is up*/
bool is_link_up(const char* interface) {
    struct ifreq ifr;
    int socket_fd;
    if((socket_fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP)) < 0) {
        print_error((char*)"Error while creating the socket", false);
    }
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, interface);
    //check if up
    if (ioctl(socket_fd, SIOCGIFFLAGS, &ifr) < 0) {
        print_error((char*)"Error while IOCTL", false);
    }
    close(socket_fd);
    return !!(ifr.ifr_flags & IFF_UP);
}

/*Function is responsible for*/
/*setting the given interface's state to the given flag*/
void set_link_state(const char* interface, short flag) {
    struct ifreq ifr;
    int socket_fd;
    if((socket_fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP)) < 0) {
        print_error((char*)"Error while creating the socket", false);
    }
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, interface);
    ifr.ifr_flags = flag;
    if (ioctl(socket_fd, SIOCSIFFLAGS, &ifr) < 0) {
        print_error((char*)"Error while IOCTL", false);
    }
    close(socket_fd);
}

/*Function will set the given interface to the up state*/
void set_link_up(const char *interface, short flags) {
    set_link_state(interface, flags | IFF_UP);
}

/*Function will set the given interface to the down state*/
// void set_link_down(const char *interface, short flags) {
//     set_link_state(interface, flags & ~IFF_UP);
// }

#endif //PARAMS_H