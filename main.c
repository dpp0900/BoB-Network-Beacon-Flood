#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <linux/wireless.h>
#include <netinet/ether.h>
#include <pthread.h>

#include "struct.h"

void create_beacon_frame(Beacon_Frame *frame, const char *ssid) {
    frame->header.frame_control = 0x0080;
    frame->header.duration_id = 0;
    memcpy(frame->header.da, "\xff\xff\xff\xff\xff\xff", 6);
    memcpy(frame->header.sa, "\x00\x11\x22\x33\x44\x55", 6); // 임의의 MAC 주소
    memcpy(frame->header.bss_id, "\x00\x11\x22\x33\x44\x55", 6); // 임의의 MAC 주소
    frame->header.sequence_control = 0;

    frame->body.timestamp = time(NULL);
    frame->body.beacon_interval = 100;
    frame->body.capacity_info = 0;

    frame->body.tag_name = 0;
    frame->body.tag_len = strlen(ssid);
    memcpy(frame->body.data, ssid, frame->body.tag_len);
}

int create_socket(const char *interface) {
    int sock;
    struct ifreq ifr;
    struct sockaddr_ll sll;

    sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    strncpy(ifr.ifr_name, interface, IFNAMSIZ);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl");
        close(sock);
        exit(EXIT_FAILURE);
    }

    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(sock, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }

    return sock;
}

void set_channel(const char *interface, int channel) {
    int sock;
    struct iwreq req;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, interface, IFNAMSIZ);

    req.u.freq.m = channel;
    req.u.freq.e = 0;
    req.u.freq.flags = IW_FREQ_FIXED;

    if (ioctl(sock, SIOCSIWFREQ, &req) < 0) {
        perror("Failed to set channel");
        close(sock);
        exit(EXIT_FAILURE);
    }

    close(sock);

    usleep(200000); // 0.2s
}

typedef struct {
    int sock;
    size_t packet_size;
    uint8_t *packet;
    char ssid[256];
} thread_data_t;

void *send_beacon_frame(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int frame_count = 0;

    while (1) {
        if (send(data->sock, data->packet, data->packet_size, 0) < 0) {
            perror("send");
            free(data->packet);
            close(data->sock);
            free(data);
            pthread_exit(NULL);
        }

        frame_count++;
        printf("Beacon frame %d sent with SSID: %s\n", frame_count, data->ssid);
        usleep(100000); // 100ms 대기
    }

    free(data->packet);
    close(data->sock);
    free(data);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <interface> <channel> <ssid_list_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *interface = argv[1];
    int channel = atoi(argv[2]);
    const char *ssid_list_file = argv[3];

    FILE *file = fopen(ssid_list_file, "r");
    if (!file) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    set_channel(interface, channel);

    char ssid[1024];
    while (fgets(ssid, sizeof(ssid), file)) {
        ssid[strcspn(ssid, "\n")] = '\0';

        Beacon_Frame beacon_frame;
        create_beacon_frame(&beacon_frame, ssid);

        Radio_tap_header rt_header;
        rt_header.version = 0;
        rt_header.pad = 0;
        rt_header.len = sizeof(Radio_tap_header);
        rt_header.present = 0;

        size_t packet_size = sizeof(Radio_tap_header) + sizeof(beacon_frame.header) + sizeof(beacon_frame.body) - sizeof(beacon_frame.body.data) + beacon_frame.body.tag_len;
        uint8_t *packet = malloc(packet_size);

        memcpy(packet, &rt_header, sizeof(Radio_tap_header));
        memcpy(packet + sizeof(Radio_tap_header), &beacon_frame, sizeof(beacon_frame.header) + sizeof(beacon_frame.body) - sizeof(beacon_frame.body.data) + beacon_frame.body.tag_len);

        thread_data_t *data = malloc(sizeof(thread_data_t));
        data->sock = create_socket(interface);
        data->packet_size = packet_size;
        data->packet = packet;
        strncpy(data->ssid, ssid, sizeof(data->ssid) - 1);
        data->ssid[sizeof(data->ssid) - 1] = '\0';

        pthread_t thread;
        if (pthread_create(&thread, NULL, send_beacon_frame, data) != 0) {
            perror("pthread_create");
            free(packet);
            free(data);
            close(data->sock);
            fclose(file);
            return EXIT_FAILURE;
        }

        pthread_detach(thread);
    }

    fclose(file);

    while (1) {
        sleep(1);
    }

    return EXIT_SUCCESS;
}