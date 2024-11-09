#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

typedef struct {
    unsigned char type;
    unsigned char code;
    unsigned short checksum;
    unsigned short id;
    unsigned short seq;
} ICMP_HEADER;

// �`�F�b�N�T���̌v�Z
unsigned short calculate_checksum(void* b, int len) {
    unsigned short* buf = b;
    unsigned long sum = 0;

    for (; len > 1; len -= 2) {
        sum += *buf++;
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    if (len == 1) {
        sum += *(unsigned char*)buf;
    }
    return ~sum;
}

// �f�[�^�O�����̓��e���^�[�~�i���ɏo��
void print_datagram(const char* label, char* datagram, int len) {
    ICMP_HEADER* header = (ICMP_HEADER*)datagram;
    printf("%s\n", label);
    printf("Type: %d\n", header->type);
    printf("Code: %d\n", header->code);
    printf("Checksum: %04X\n", ntohs(header->checksum)); 
    printf("ID: %d\n", ntohs(header->id));                
    printf("Sequence Number: %d\n", ntohs(header->seq));  
    printf("Echo Data: ");
    for (int i = sizeof(ICMP_HEADER); i < len; ++i) {
        printf("%02X ", (unsigned char)datagram[i]);
    }
    printf("\n\n");
}

int main() {
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in dest, from;
    ICMP_HEADER icmp_hdr;
    char packet[sizeof(ICMP_HEADER) + 2]; // �p�P�b�g�T�C�Y�� ICMP_HEADER + 2 �o�C�g�ɐݒ�
    char recvbuf[1024];
    int fromlen = sizeof(from);
    int bread, bwrote;

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, "8.8.8.8", &dest.sin_addr);

    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    icmp_hdr.type = 8;
    icmp_hdr.code = 0;
    icmp_hdr.id = htons(0);  
    icmp_hdr.seq = htons(0); 
    memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));

    // Echo Data ��ǉ��i��Ƃ��� 0xCC 0xCC ��2�o�C�g�Ŗ��߂�j
    packet[sizeof(ICMP_HEADER)] = 0xCC;
    packet[sizeof(ICMP_HEADER) + 1] = 0xCC;

    // �`�F�b�N�T���̌v�Z�Ɛݒ�
    icmp_hdr.checksum = calculate_checksum(packet, sizeof(packet));
    memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));

    // ���M�O�̃p�P�b�g���e���o��
    printf("Packet data before sending: ");
    for (int i = 0; i < sizeof(packet); ++i) {
        printf("%02X ", (unsigned char)packet[i]);
    }
    printf("\n");

    // Echo Request ���M
    bwrote = sendto(sock, packet, sizeof(packet), 0, (SOCKADDR*)&dest, sizeof(dest));
    if (bwrote == SOCKET_ERROR) {
        fprintf(stderr, "ICMP Echo Request failed with error: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Echo Request �f�[�^�O������\��
    print_datagram("Echo Request Datagram:", packet, sizeof(packet));

    // Echo Reply �̎�M
    bread = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (SOCKADDR*)&from, &fromlen);
    if (bread == SOCKET_ERROR) {
        fprintf(stderr, "Receive failed with error: %d\n", WSAGetLastError());
    }
    else {
        int ipHeaderLength = (recvbuf[0] & 0x0F) * 4;
        print_datagram("Echo Reply Datagram:", recvbuf + ipHeaderLength, bread - ipHeaderLength);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
