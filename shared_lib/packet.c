#include "packet.h"

#include <unistd.h>
#include <stdlib.h>

#define CHECK_PACKET2(p) if(p == (void*)0) return 0
#define CHECK_PACKET(p) if(p == (void*)0) return

int read_packet_from_fd(int fd, packet_t* p)
{
    CHECK_PACKET(p);

}

void destroy_packet(packet_t* p)
{
    CHECK_PACKET(p);
    free(p->body.content);
    free(p);
}