#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/md5.h>

#define PORT_INFO 4587
#define PORT_FILE 4588
#define MAX_BUFFER_SIZE 1024

void receive_file(int socket, const char* filename, long file_size, const char* expected_md5sum) {
    // Nhận file từ server và lưu vào đĩa cứng
    // Sau đó tính toán md5sum của file và so sánh với giá trị expected_md5sum

    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Failed to create file: %s\n", filename);
        return;
    }

    int bytes_received;
    long bytes_total = 0;
    char buffer[MAX_BUFFER_SIZE];
    MD5_CTX md5_context;
    MD5_Init(&md5_context);

    while (bytes_total < file_size) {
        int bytes_to_receive = (file_size - bytes_total < MAX_BUFFER_SIZE) ? file_size - bytes_total : MAX_BUFFER_SIZE;

        bytes_received = recv(socket, buffer, bytes_to_receive, 0);
        if (bytes_received == -1) {
            printf("Failed to receive file data\n");
            fclose(file);
            return;
        }

        fwrite(buffer, 1, bytes_received, file);
        MD5_Update(&md5_context, buffer, bytes_received);

        bytes_total += bytes_received;
    }

    fclose(file);

    unsigned char md5sum[MD5_DIGEST_LENGTH];
    MD5_Final(md5sum, &md5_context);

    char md5sum_string[MD5_DIGEST_LENGTH*2+1];
    int i;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&md5sum_string[i*2], "%02x", (unsigned int)md5sum[i]);
    }
    md5sum_string[MD5_DIGEST_LENGTH*2] = '\0';

    if (strcmp(md5sum_string, expected_md5sum) == 0) {
        printf("File received successfully and MD5 checksum is correct\n");
    } else {
        printf("File received successfully but MD5 checksum is incorrect\n");
    }
}

int main() {
    // Tạo socket client
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        printf("Failed to create socket\n");
        return 1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(PORT_INFO);

    // Kết nối tới server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
    printf("Connection failed\n");
    return 1;
    }
    // Nhận thông tin về file
    char filename[100];
    long file_size;
    char expected_md5sum[MD5_DIGEST_LENGTH*2+1];
    memset(expected_md5sum, 0, sizeof(expected_md5sum));

    int bytes_received = recv(client_socket, filename, sizeof(filename), 0);
    if (bytes_received == -1) {
        printf("Failed to receive filename\n");
        return 1;
    }
    filename[bytes_received] = '\0';

    bytes_received = recv(client_socket, &file_size, sizeof(file_size), 0);
    if (bytes_received == -1) {
        printf("Failed to receive file size\n");
        return 1;
    }

    bytes_received = recv(client_socket, expected_md5sum, sizeof(expected_md5sum), 0);
    if (bytes_received == -1) {
        printf("Failed to receive MD5 checksum\n");
        return 1;
    }

    printf("Receiving file %s (%ld bytes)\n", filename, file_size);
    // Mở kết nối mới để nhận file
    close(client_socket);
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        printf("Failed to create socket\n");
        return 1;
    }

    server_address.sin_port = htons(PORT_FILE);

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("Connection failed\n");
        return 1;
    }

    // Nhận và lưu file
    receive_file(client_socket, filename, file_size, expected_md5sum);
    close(client_socket);
    return 0;
}
