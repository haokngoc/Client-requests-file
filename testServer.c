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

void send_file(int socket, const char* filename) {
    // Gửi file có tên 'filename' cho client thông qua kết nối socket 'socket'

    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Failed to open file: %s\n", filename);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    int bytes_sent;
    long bytes_total = 0;
    char buffer[MAX_BUFFER_SIZE];

    while (bytes_total < file_size) {
        int bytes_to_send = (file_size - bytes_total < MAX_BUFFER_SIZE) ? file_size - bytes_total : MAX_BUFFER_SIZE;
        fread(buffer, 1, bytes_to_send, file);

        bytes_sent = send(socket, buffer, bytes_to_send, 0);
        if (bytes_sent == -1) {
            printf("Failed to send file data\n");
            fclose(file);
            return;
        }

        bytes_total += bytes_sent;
    }

    fclose(file);
    printf("File sent successfully\n");
}

int main() {
    // Tạo socket server
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        printf("Failed to create socket\n");
        return 1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT_INFO);

    // Liên kết socket server với địa chỉ và cổng
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        printf("Failed to bind socket\n");
        close(server_socket);
        return 1;
    }

    // Lắng nghe kết nối từ client
    if (listen(server_socket, 1) == -1) {
        printf("Failed to listen for connections\n");
        close(server_socket);
        return 1;
    }

    printf("Server started and listening on port %d\n", PORT_INFO);

    while (1) {
        // Chấp nhận kết nối từ client
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_size);
        if (client_socket == -1) {
            printf("Failed to accept client connection\n");
            continue;
        }

        printf("Client connected from %s: %d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        // Gửi thông tin về file cho client

        
        const char* filename = "file.txt";
        FILE* file = fopen(filename, "rb");
        if (file == NULL) {
            printf("Failed to open file: %s\n", filename);
            close(client_socket);
            continue;
        }

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char fileinfo[MAX_BUFFER_SIZE];
        sprintf(fileinfo, "%s;%ld;", filename, file_size);

        // Tính toán giá trị hash MD5 của file
        MD5_CTX mdContext;
        MD5_Init(&mdContext);
            int bytes_read;
        char hash_buffer[MAX_BUFFER_SIZE];
        while ((bytes_read = fread(hash_buffer, 1, MAX_BUFFER_SIZE, file)) != 0) {
            MD5_Update(&mdContext, hash_buffer, bytes_read);
        }
        unsigned char hash_value[MD5_DIGEST_LENGTH];
        MD5_Final(hash_value, &mdContext);

        char hash_str[MD5_DIGEST_LENGTH * 2 + 1];
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            sprintf(&hash_str[i * 2], "%02x", (unsigned int)hash_value[i]);
        }

        strcat(fileinfo, hash_str);

        fclose(file);

        // Gửi thông tin file cho client
        int bytes_sent = send(client_socket, fileinfo, strlen(fileinfo), 0);
        if (bytes_sent == -1) {
            printf("Failed to send file info\n");
            close(client_socket);
            continue;
        }

        printf("File info sent successfully\n");

        // Chờ đợi phản hồi từ client
        char response[MAX_BUFFER_SIZE];
        int bytes_received = recv(client_socket, response, MAX_BUFFER_SIZE, 0);
        if (bytes_received == -1) {
            printf("Failed to receive response from client\n");
            close(client_socket);
            continue;
        }

        response[bytes_received] = '\0';

        if (strcmp(response, "ACK") != 0) {
            printf("Invalid response from client: %s\n", response);
            close(client_socket);
            continue;
        }

        printf("Client ACK received\n");

        // Gửi file cho client
        send_file(client_socket, filename);

        close(client_socket);
    }

    close(server_socket);

    return 0;
}
