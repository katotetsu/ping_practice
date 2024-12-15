#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h> // 分散計算用
#include <windows.h> // 高精度タイマー用

#pragma comment(lib, "ws2_32.lib")

#define NUM_PINGS 30 //Ping送信の回数を定義
//#define TARGET_IP "172.20.10.3"  // 実験用IPアドレスを定義
#define TARGET_IP "127.0.0.1"  // デバッグ用ループバックIPアドレスを定義

typedef struct {
    unsigned char type;
    unsigned char code;
    unsigned short checksum;
    unsigned short id;
    unsigned short seq;
} ICMP_HEADER;

// チェックサムの計算
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

// データグラムの内容をターミナルに出力
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
    char packet[sizeof(ICMP_HEADER) + 2];
    char recvbuf[1024];
    int fromlen = sizeof(from);
    int bread, bwrote;

    LARGE_INTEGER frequency, startTime, endTime;
    QueryPerformanceFrequency(&frequency); // 高精度タイマーの周波数を取得

    if (QueryPerformanceFrequency(&frequency)) {
        printf("Performance Counter Frequency: %lld Hz\n", frequency.QuadPart);
    }
    else {
        printf("Performance Counter not supported.\n");
    }

    double rtt, totalRTT = 0, totalRTTSquared = 0;
    int successCount = 0;
    int rttValues[NUM_PINGS]; // 配列のサイズをNUM_PINGSに変更

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, TARGET_IP, &dest.sin_addr);

    for (int i = 1; i <= NUM_PINGS; ++i) {
        memset(&icmp_hdr, 0, sizeof(icmp_hdr));
        icmp_hdr.type = 8; // Echo Request
        icmp_hdr.code = 0;
        icmp_hdr.id = htons(0);
        icmp_hdr.seq = htons(i);
        memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));

        packet[sizeof(ICMP_HEADER)] = 0xCC;
        packet[sizeof(ICMP_HEADER) + 1] = 0xCC;

        icmp_hdr.checksum = calculate_checksum(packet, sizeof(packet));
        memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));

        QueryPerformanceCounter(&startTime); // 開始時間を取得
        bwrote = sendto(sock, packet, sizeof(packet), 0, (SOCKADDR*)&dest, sizeof(dest));
        if (bwrote == SOCKET_ERROR) {
            fprintf(stderr, "ICMP Echo Request failed with error: %d\n", WSAGetLastError());
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        bread = recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (SOCKADDR*)&from, &fromlen);
        QueryPerformanceCounter(&endTime); // 終了時間を取得

        if (bread == SOCKET_ERROR) {
            fprintf(stderr, "Receive failed for Sequence %d with error: %d\n", i, WSAGetLastError());
        }
        else {
            int ipHeaderLength = (recvbuf[0] & 0x0F) * 4;
            ICMP_HEADER* recv_header = (ICMP_HEADER*)(recvbuf + ipHeaderLength);
            print_datagram("Echo Reply Datagram:", recvbuf + ipHeaderLength, bread - ipHeaderLength);

            // シーケンス番号をチェック
            if (ntohs(recv_header->seq) == i) {
                printf("Sequence number matches: %d\n", i);
            }
            else {
                printf("Sequence number mismatch: expected %d, got %d\n", i, ntohs(recv_header->seq));
                continue;
            }

            // RTT 計算 (マイクロ秒単位)
            rtt = (double)(endTime.QuadPart - startTime.QuadPart) * 1e6 / frequency.QuadPart;
            printf("RTT for Sequence %d: %.2f microseconds\n", i, rtt);

            rttValues[successCount] = (int)rtt;
            totalRTT += rtt;
            totalRTTSquared += rtt * rtt;
            ++successCount;
        }

        Sleep(1000); // 1秒待機
    }

    double meanRTT = (successCount > 0) ? (totalRTT / successCount) : 0;
    double varianceRTT = (successCount > 1) ? (totalRTTSquared / successCount - meanRTT * meanRTT) : 0;

    printf("\nPing Statistics:\n");
    printf("Packets: Sent = %d, Received = %d, Lost = %d (%.2f%% loss)\n", NUM_PINGS, successCount, NUM_PINGS - successCount, ((NUM_PINGS - successCount) / (double)NUM_PINGS) * 100);
    if (successCount > 0) {
        printf("Average RTT: %.2f microseconds\n", meanRTT);
        printf("Variance RTT: %.2f microseconds squared\n", varianceRTT);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
