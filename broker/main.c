#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/survey.h>

#define DEFAULT_CLIENT_URL "tcp://*:1365"
#define DEFAULT_SERVER_URL "tcp://*:1365"

struct flux_server;
struct flux_server {
    char * id_str;
    size_t id_size;
    struct flux_server * next;
};

static struct flux_server * servers = NULL;
static struct flux_server * survey_response = NULL;

static char * id_response = NULL;
static size_t id_response_size = 0;

static void print_help(){
    printf("Usage: flux-broker [-s server_socket] [-c client_socket] [-v]\n"
           " -s server_socket  Set the nanomsg socket that servers connect to.\n"
           "                   Default: '" DEFAULT_SERVER_URL "'.\n"
           " -c client_socket  Set the nanomsg socket that clients connect to.\n"
           "                   Default: '" DEFAULT_CLIENT_URL "'.\n"
           " -h                Print this help message.\n"
           " -v                Verbose\n");
}

int main (int argc, char *argv [])
{
    char * server_url = DEFAULT_SERVER_URL;
    char * client_url = DEFAULT_CLIENT_URL;
    int verbose = 0;

    char ** arg_dest = NULL;
    while(--argc){
        argv++; // Chomp the next argument. We ignore the first argument (the program name)
        if(argv[0][0] == '-'){
            if(arg_dest){
                print_help();
                fprintf(stderr, "Expected argument value\n");
                return -1;
            }
            switch(argv[0][1]){
                case 's': arg_dest = &server_url; break;
                case 'c': arg_dest = &client_url; break;
                case 'v': verbose = 1; break;
                case 'h': print_help(); return 0;
                default:
                    print_help();
                    fprintf(stderr, "Invalid argument '%s'\n", *argv);
                    return -1;
            }
        }else{
            if(!arg_dest){
                print_help();
                fprintf(stderr, "Invalid argument '%s'\n", *argv);
                return -1;
            }
            *arg_dest = *argv;
            arg_dest = NULL;
        }
    }
    if(arg_dest){
        print_help();
        fprintf(stderr, "Expected argument value\n");
        return -1;
    }

    // Bind nanomsg sockets
    int rep_sock = nn_socket(AF_SP, NN_REP);
    assert(rep_sock >= 0);
    assert(nn_bind(rep_sock, client_url));

    int surv_sock = nn_socket(AF_SP, NN_SURVEYOR);
    assert(surv_sock >= 0);
    assert(nn_bind(surv_sock, server_url));

    struct nn_pollfd pollfds[2] = {
        {
            .fd = rep_sock,
            .events = NN_POLLIN,
            .revents = 0
        },
        {
            .fd = surv_sock,
            .events = NN_POLLIN,
            .revents = 0
        },
    };

    //  Get and pass messages back and forth forever (until interrupted)
    while (1) {
        int np = nn_poll(pollfds, 2, 1000);
        if(np < 0) break;

        if(pollfds[0].revents & NN_POLLIN){
            assert(pollfds[0].fd == rep_sock);
            void * msg;
            int r = nn_recv(rep_sock, &msg, NN_MSG, 0);
            if(r == 2){
                if(memcmp(msg, "ID", 2) == 0){
                    assert((int) id_response_size == nn_send(rep_sock, id_response, id_response_size, 0));
                    assert(2 == nn_send(surv_sock, "ID", 2, 0)); // TODO: only one survey at a time
                }
            }
        }

        if(pollfds[1].revents & NN_POLLIN){
            char * resp;
            assert(pollfds[1].fd == surv_sock);
            int resp_size = nn_recv(surv_sock, &resp, NN_MSG, 0);
            if(resp_size == ETIMEDOUT){

                char * iresp = NULL;
                char * iptr = NULL;
                size_t isize = 0;
                struct flux_server * serv = survey_response;
                while(serv){
                    isize += serv->id_size + 1;
                    serv = serv->next;
                }
                iresp = malloc(isize);
                assert(iresp);

                serv = survey_response;
                while(serv){
                    memcpy(iptr, serv->id_str, serv->id_size);
                    iptr += resp_size;
                    *iptr++ = '\n';
                    serv = serv->next;
                }

                assert(iptr - iresp == (int) isize);
                free(id_response);
                id_response = iresp;
                id_response_size = isize;

                free(servers);
                servers = survey_response; 
                survey_response = NULL;

                // Send next survey
                // assert(2 == nn_send(surv_sock, "ID", 2));
            }else if(resp_size < 0){
                if(verbose) fprintf(stderr, "Error from survey: %d\n", resp_size);
            }else{
                struct flux_server * serv = malloc(sizeof(serv));
                assert(serv);

                // Copy & store response
                serv->id_size = resp_size;
                serv->id_str = malloc(resp_size);
                assert(serv->id_str);
                memcpy(serv->id_str, resp, resp_size);
                nn_freemsg(resp);
                
                // Append to survey result list
                serv->next = survey_response;
                survey_response = serv->next;
            }
        }

    }

    nn_shutdown(rep_sock, 0);
    nn_shutdown(surv_sock, 0);

    return 0;
}
