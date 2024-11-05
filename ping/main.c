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

// �`�F�b�N�T���̌v�Z�i�l�b�g���[�N�o�C�g�I�[�_�[�̂܂܌v�Z�j
unsigned short calculate_checksum(void* b, int len, FILE* fp) {
    unsigned short* buf = b;
    unsigned long sum = 0;

    fprintf(fp, "Checksum Calculation Start:\n");

    for (; len > 1; len -= 2) {
        unsigned short word = *buf++;
        fprintf(fp, "Step %d: %04X + %04X = ", (len / 2), (unsigned short)sum, word);
        sum += word;
        fprintf(fp, "%04X\n", (unsigned short)sum);

        // �L�����I�[�o�[����
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
            fprintf(fp, "Carry Adjustment: %04X\n", (unsigned short)sum);
        }
    }

    if (len == 1) {
        unsigned short last_byte = *(unsigned char*)buf;
        sum += last_byte;
        fprintf(fp, "Remaining Byte: + %02X = %04X\n", last_byte, (unsigned short)sum);
    }

    unsigned short checksum = ~sum;
    fprintf(fp, "Final Checksum (1's complement): %04X\n", checksum);

    return checksum;
}

// �f�[�^�O�����̓��e���o��
void print_datagram(FILE* fp, const char* label, char* datagram, int len) {
    ICMP_HEADER* header = (ICMP_HEADER*)datagram;
    fprintf(fp, "%s\n", label);
    fprintf(fp, "Type: %d\n", header->type);
    fprintf(fp, "Code: %d\n", header->code);
    fprintf(fp, "Checksum: %04X\n", ntohs(header->checksum));  // �r�b�O�G���f�B�A���o��
    fprintf(fp, "ID: %d\n", ntohs(header->id));                // �r�b�O�G���f�B�A���o��
    fprintf(fp, "Sequence Number: %d\n", ntohs(header->seq));  // �r�b�O�G���f�B�A���o��
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
    icmp_hdr.id = htons(GetCurrentProcessId());  // ID���r�b�O�G���f�B�A���Őݒ�
    icmp_hdr.seq = htons(0);                      // �V�[�P���X�ԍ����r�b�O�G���f�B�A���Őݒ�
    memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));

    // Echo Data ��ǉ��i��Ƃ��� 0xCC 0xCC ��2�o�C�g�Ŗ��߂�j
    packet[sizeof(ICMP_HEADER)] = 0xCC;
    packet[sizeof(ICMP_HEADER) + 1] = 0xCC;

    // �`�F�b�N�T���̌v�Z�Ɛݒ�
    icmp_hdr.checksum = calculate_checksum(packet, sizeof(packet), fp);
    memcpy(packet, &icmp_hdr, sizeof(icmp_hdr));

    // �o��: ���M�O�̃p�P�b�g���e
    fprintf(fp, "Packet data before sending: ");
    for (int i = 0; i < sizeof(packet); ++i) {
        fprintf(fp, "%02X ", (unsigned char)packet[i]);
    }
    fprintf(fp, "\n");

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
        int ipHeaderLength = (recvbuf[0] & 0x0F) * 4;
        print_datagram(fp, "Echo Reply Datagram:", recvbuf + ipHeaderLength, bread - ipHeaderLength);
        fprintf(fp, "Round-trip time: %ld ms\n", endTime - startTime);
    }

    fclose(fp);
    closesocket(sock);
    WSACleanup();
    return 0;
}
