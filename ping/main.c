#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

typedef struct {
    unsigned char type;
    unsigned char code;
    unsigned short checksum;
    unsigned short id;
    unsigned short seq;
} ICMP_HEADER;

unsigned short calculate_checksum(void* b, int len) {
    unsigned short* buf = b;
    unsigned long sum = 0;

    for (; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char*)buf;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return ~sum;
}

void print_datagram(FILE* fp, const char* label, char* datagram, int len) {
    ICMP_HEADER* header = (ICMP_HEADER*)datagram;
    fprintf(fp, "%s\n", label);
    fprintf(fp, "Type: %d\n", header->type);
    fprintf(fp, "Code: %d\n", header->code);
    fprintf(fp, "Checksum: %04X\n", ntohs(header->checksum));
    fprintf(fp, "ID: %d\n", ntohs(header->id));
    fprintf(fp, "Sequence Number: %d\n", ntohs(header->seq));
    fprintf(fp, "Echo Data: ");
    for (int i = sizeof(ICMP_HEADER); i < len; ++i) {
        fprintf(fp, "%02X ", (unsigned char)datagram[i]);
    }
    fprintf(fp, "\n\n");
}

int main() {
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in dest, from;
    ICMP_HEADER icmp_hdr;
    char packet[32];
    char recvbuf[1024];
    int fromlen = sizeof(from);
    int bread, bwrote;
    DWORD startTime, endTime;
    FILE* fp = NULL;

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);

    errno_t err = fopen_s(&fp, "output.txt", "w");
    if (err != 0) {
        fprintf(stderr, "Failed to open file.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    fprintf(fp, "127.0.0.1 echoRequest AND echoReply:\n");
    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    icmp_hdr.type = 8;
    icmp_hdr.code = 0;
    icmp_hdr.id = htons(GetCurrentProcessId());
    icmp_hdr.seq = htons(0);
    memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));
    icmp_hdr.checksum = calculate_checksum(packet, sizeof(packet));
    memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));

    startTime = GetTickCount();
    bwrote = sendto(sock, packet, sizeof(packet), 0, (SOCKADDR*)&dest, sizeof(dest));
    if (bwrote == SOCKET_ERROR) {
        fprintf(fp, "ICMP Echo Request failed with error: %d\n", WSAGetLastError());
        fclose(fp);
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    print_datagram(fp, "Echo Request Datagram:", packet, sizeof(packet));
    bread = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (SOCKADDR*)&from, &fromlen);
    endTime = GetTickCount();
    if (bread == SOCKET_ERROR) {
        fprintf(fp, "Receive failed with error: %d\n", WSAGetLastError());
    }
    else {
        // Adjust pointer to skip IP header
        int ipHeaderLength = (recvbuf[0] & 0x0F) * 4;
        print_datagram(fp, "Echo Reply Datagram:", recvbuf + ipHeaderLength, bread - ipHeaderLength);
    }

    fclose(fp);
    closesocket(sock);
    WSACleanup();
    return 0;
}
