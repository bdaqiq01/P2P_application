// Peer program for P2P Networking Project
// Basira Daqiq
// Steven Correa

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>

#define PUB_MSG_SIZE 1200             // adjusted from 2048 
#define SHARED_DIR "./SharedFiles" // Path to SharedFiles
#define MAX_NAME 100                  // max filename length

// Helper function to send all data in one request
// Carryover from previous project
int sendall(int s, const char *buf, int *len) {
  // this function is borrowed from Beej's guide to send all the data
  int total = 0;        // how many bytes we've sent
  int bytesleft = *len; // how many we have left to send
  int n;

  while (total < *len) {
    n = send(s, buf + total, bytesleft, 0);
    if (n == -1) {
      break;
    }
    total += n;
    bytesleft -= n;
  }

  *len = total; // return number actually sent here

  return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}


// Helper function to validate peer_id per handout instructions
// Instructions only require: "Select a positive number less than 2^32 - 1 as
// the ID"
static int validate_peer_id(const char *peer_id) {
  // check if peer_id is null
  if (!peer_id){
    printf("Invalid peer ID\n");
    return 0;
  }

  uint64_t id_value = 0; //changed fron unsigned long long to uint64_t
  const uint64_t MAX_PEER_ID = UINT32_MAX; // changed from hardcoded value to defined header 2^32 - 1

  if (sscanf(peer_id, "%" SCNu64, &id_value) != 1) {
    printf("Invalid peer ID format\n");
    return 0;
  }

  if (id_value == 0 || id_value > MAX_PEER_ID) {
    printf("Invalid peer ID range\n");
    return 0;
  }

  return 1; // Valid peer_id
}

int construct_publish_msg(uint8_t *pub_msg, size_t cap, size_t *filled_len) {
  if (!pub_msg || !filled_len || cap < 5)
    return -1; // invalid arguments

  pub_msg[0] = 0x01; // action code for publish  first byte

  
  uint32_t count = 0; // number of files to publish
  size_t offset = 5; // keeps track of how much is filled in onthe message buffer

  // Second pass: collect the filenames
  DIR *dir = opendir(SHARED_DIR);
  if (!dir) {
    perror("opendir SharedFiles");
    printf("Error: Failed to open shared files directory. Does it exist?\n");
    return -1;
  }
  struct dirent *de;
  while ((de = readdir(dir)) != NULL) {
    // full path for regular-file check

    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
      continue;

    if (de->d_type != DT_REG)
      continue; // skip if not a regular file

    size_t name_len = strlen(de->d_name) + 1; // include '\0'

    if (name_len > MAX_NAME)
    {
      printf("Skipping file with too long name: %s\n", de->d_name);
      continue; // skip if name too long
    }

    if (offset + name_len > cap)
      break; // check if there is enough space in the buffer

    
      // Copy filename into the message buffer
    memcpy(pub_msg + offset, de->d_name, name_len);
    offset += name_len;
    count++;
    

  }
  closedir(dir);

  if (count == 0) {
    // No files to publish
    printf("No files to publish in %s\n", SHARED_DIR);
    *filled_len = 5; // only action code and count
    uint32_t z = htonl(0);
    memcpy(pub_msg + 1, &z, 4);
    return 0;
  }
   // Set the count in network byte order at the correct position (bytes 1-4)
   uint32_t count_net = htonl(count);

   memcpy(pub_msg + 1, &count_net, 4);
  // maybe a close DIr here
  *filled_len = offset; // total length of the publish message

  return 0;
}

int lookup_and_connect(const char *host, const char *service) {
  // Carryover from previous project
  struct addrinfo hints = {0};
  struct addrinfo *rp, *result;
  int s;

  /* Translate host name into peer's IP address */
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  if ((s = getaddrinfo(host, service, &hints, &result)) != 0) {
    fprintf(stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror(s));
    return -1;
  }

  /* Iterate through the address list and try to connect */
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    if ((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
      continue;
    }

    if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    close(s);
  }
  if (rp == NULL) {
    perror("stream-talk-client: connect");
    return -1;
  }
  freeaddrinfo(result);

  return s;
}

int main(int argc, char *argv[]) {
  char *reg_host;  // host of the registry server
  char *reg_port;  // port of the registry server
  char *peer_id;   // unique ID of this peer
  int sockfd = -1; // persistent socket connection
  int joined = 0;  // flag to track if we've joined the network

  char buffer[1024]; // buffer to hold data before sending the send request
  uint8_t publish_mg[PUB_MSG_SIZE]; // buffer to hold the publish message


  if (argc == 4) {
    reg_host = argv[1];
    reg_port = argv[2];
    peer_id = argv[3];

    // Validate peer_id per handout: "Select a positive number less than
    // 2^32 - 1 as the ID"
    if (!validate_peer_id(peer_id)) {
      fprintf(stderr,
              "Invalid peer Id. usage: %s <registry_host> <registry_port> <my_peer_id>\n",
              argv[0]);
      exit(1);
    }
  } else {
    // Invalid number of arguments passed in
    fprintf(stderr, "usage: %s <registry_host> <registry_port> <my_peer_id>\n",
            argv[0]);
    exit(1);
  }

  while (1) {

    // Get input from user
    char command[32];
    printf("Enter a command: ");
    fflush(stdout); // ensure prompt is displayed

    if (fgets(command, sizeof(command), stdin) == NULL)
      break; // some checking

    // Remove trailing newline if present
    size_t cmd_len = strlen(command);
    if (cmd_len > 0 && command[cmd_len - 1] == '\n') {
      command[cmd_len - 1] = '\0';
    }

    if (strcmp(command, "EXIT") == 0) {
      if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
      }
      break; // exit if command is exit
    }

    else if (strcmp(command, "JOIN") == 0) {
      // Close existing connection if any
      if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
        joined = 0;
      }

      // Create a socket and connect to the registry server
      if ((sockfd = lookup_and_connect(reg_host, reg_port)) < 0) {
        perror("failed to connect to registry");
        exit(1);
      }

      // Prepare JOIN message: action code 0 + 4-byte peer_id in network byte
      // order
      buffer[0] = 0; // action code for JOIN
      uint32_t peer_id_num = (uint32_t)strtoul(peer_id, NULL, 10);
      uint32_t peer_id_net = htonl(peer_id_num);
      memcpy(buffer + 1, &peer_id_net, 4);
      int len = 5; // 1 byte action + 4 bytes peer_id


      // Send the join request to the registry server
      if (sendall(sockfd, buffer, &len) != 0) {
        perror("failed to send join request");
        close(sockfd);
        sockfd = -1;
        continue;
      }

      joined = 1; // Mark as joined, keep socket open
      continue;   // prompt again
    } else if (strcmp(command, "PUBLISH") == 0) {
      if (!joined) {
        printf("Error: Please JOIN the network before issuing a PUBLISH\n");
        continue;
      }

      // send publish request
      // sends a request to the registry server to publish a file
      publish_mg[0] = 1; // action code for PUBLISH

      size_t pub_msg_len = 0;
      if (construct_publish_msg((uint8_t *)publish_mg, PUB_MSG_SIZE,
                                &pub_msg_len) != 0) {
        fprintf(stderr, "failed to construct publish message\n");
        continue; // return to prompt
      }

      // send publish request using existing connection
      int len = pub_msg_len;
      if (sendall(sockfd, (const char *)publish_mg, &len) != 0) {
        perror("failed to send publish request");
        close(sockfd);
        sockfd = -1;
        joined = 0;
        continue; // return to prompt
      }
      continue; // prompt again
    } else if (strcmp(command, "SEARCH") == 0) {
      if (!joined) {
        printf("Error: Must JOIN the network before SEARCH\n");
        continue;
      }

      // send search request
      char filename[256];
      printf("Enter filename: ");

      if (fgets(filename, sizeof(filename), stdin) == NULL) {
        printf("No filename provided, Please try again with a filename that exists.\n");
        continue;
      }

      // Remove trailing newline if present
      size_t filename_len = strlen(filename);
      if (filename_len > 0 && filename[filename_len - 1] == '\n') {
        filename[filename_len - 1] = '\0';
        filename_len--;
      }

      buffer[0] = 2;                // action code for SEARCH
      strcpy(buffer + 1, filename); // copy filename after action code
      int len = filename_len + 2; // +1 for action code + 1 for null terminator

      // send the search request using existing connection
      if (sendall(sockfd, buffer, &len) != 0) {
        perror("failed to send search request");
        close(sockfd);
        sockfd = -1;
        joined = 0;
        continue;
      }

      // handle the response from the registry server
      char response[10]; // 4 bytes peer_id + 4 bytes IP + 2 bytes port
      int total_received = 0;
      int expected_bytes = 10;

      // Receive all 10 bytes of the response
      while (total_received < expected_bytes) {
        int numbytes = recv(sockfd, response + total_received,
                            expected_bytes - total_received, 0);
        if (numbytes == -1) {
          perror("error on recv");
          printf("Error: Failed to receive search response from registry.\n");
          close(sockfd);
          break; // break out of recv loop, not continue to outer loop
        }
        if (numbytes == 0) {
          printf("Connection closed by registry\n");
          close(sockfd);
          break;
        }
        total_received += numbytes;
      }

      if (total_received == 10) {
        // Parse the response
        uint32_t peer_id_resp, ip_addr;
        uint16_t port_num;

        memcpy(&peer_id_resp, response, 4);
        memcpy(&ip_addr, response + 4, 4);
        memcpy(&port_num, response + 8, 2);

        peer_id_resp = ntohl(peer_id_resp);
        ip_addr = ntohl(ip_addr);
        port_num = ntohs(port_num);


        if (peer_id_resp == 0 && ip_addr == 0 && port_num == 0) {
          printf("File not indexed by registry\n");
        } else {
          // Convert IP address to string
          struct in_addr addr;
          addr.s_addr = htonl(ip_addr);
          char ip_str[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);

          printf("File found at\n");
          printf("Peer %u\n", peer_id_resp);
          printf("%s:%u\n", ip_str, port_num);
        }
      } else if (total_received > 0) {
        printf("Bad response from registry (%d bytes)\n", total_received);
      }
      continue; // prompt again
    } else if (strcmp(command, "FETCH") == 0) {
      if (!joined) {
        printf("Error: Must JOIN the network before FETCH\n");
        continue;
      }

      // Read filename from user
      char filename[256];
      printf("Enter filename: ");

      if (fgets(filename, sizeof(filename), stdin) == NULL) {
        printf("No filename provided.\n");
        continue;
      }

      // Remove trailing newline if present
      size_t filename_len = strlen(filename);
      if (filename_len > 0 && filename[filename_len - 1] == '\n') {
        filename[filename_len - 1] = '\0';
        filename_len--;
      }

      // Validate filename length (max 100 bytes including NULL per handout)
      if (filename_len >= MAX_NAME) {
        printf("Error: Filename too long (max %d bytes)\n", MAX_NAME - 1);
        continue;
      }

      // Step 1: Send SEARCH request to registry to find the file
      buffer[0] = 2;                // action code for SEARCH
      memcpy(buffer + 1, filename, filename_len + 1); // copy filename after action code
      int len = (int)(1 + filename_len + 1);   // +1 for action code + 1 for null terminator

      // Send the search request using existing connection
      if (sendall(sockfd, buffer, &len) != 0) {
        perror("failed to send search request");
        close(sockfd);
        sockfd = -1;
        joined = 0;
        continue;
      }

      // Step 2: Receive SEARCH response from registry
      char search_response[10]; // 4 bytes peer_id + 4 bytes IP + 2 bytes port
      int total_received = 0;
      int expected_bytes = 10;

      // Receive all 10 bytes of the response
      while (total_received < expected_bytes) {
        int numbytes = recv(sockfd, search_response + total_received,
                            expected_bytes - total_received, 0);
        if (numbytes == -1) {
          perror("error on recv");
          close(sockfd);
          sockfd = -1;
          joined = 0;
          break;
        }
        if (numbytes == 0) {
          printf("Connection closed by registry\n");
          close(sockfd);
          sockfd = -1;
          joined = 0;
          break;
        }
        total_received += numbytes;
      }

      if (total_received != 10) {
        printf("Bad response from registry (%d bytes)\n", total_received);
        continue;
      }

      // Step 3: Parse the SEARCH response
      uint32_t peer_id_resp, ip_addr;
      uint16_t port_num;

      memcpy(&peer_id_resp, search_response, 4);
      memcpy(&ip_addr, search_response + 4, 4);
      memcpy(&port_num, search_response + 8, 2);

      peer_id_resp = ntohl(peer_id_resp);
      ip_addr = ntohl(ip_addr);
      port_num = ntohs(port_num);

      // Check if file was not found
      if (peer_id_resp == 0 && ip_addr == 0 && port_num == 0) {
        printf("File not indexed by registry\n");
        continue;
      }

      // Convert IP address to string for connection
      struct in_addr addr;
      addr.s_addr = htonl(ip_addr);
      char ip_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);

      // Convert port to string for lookup_and_connect
      char port_str[10];
      snprintf(port_str, sizeof(port_str), "%u", port_num);

      // Step 4: Connect to the peer that has the file
      int peer_sockfd = lookup_and_connect(ip_str, port_str);
      if (peer_sockfd < 0) {
        fprintf(stderr, "Failed to connect to peer %u at %s:%u\n", 
                peer_id_resp, ip_str, port_num);
        continue;
      }

      // Step 5: Send FETCH request to peer (action=3 + filename)
      buffer[0] = 3;                // action code for FETCH
      memcpy(buffer + 1, filename, filename_len + 1); // copy filename after action code
      len = (int)(1 + filename_len + 1);       // +1 for action code + 1 for null terminator

      if (sendall(peer_sockfd, buffer, &len) != 0) {
        perror("failed to send fetch request");
        close(peer_sockfd);
        continue;
      }

      // Step 6: Receive FETCH response code (1 byte)
      uint8_t response_code;
      int numbytes = recv(peer_sockfd, &response_code, 1, 0);
      if (numbytes != 1) {
        fprintf(stderr, "Failed to receive response code from peer\n");
        close(peer_sockfd);
        continue;
      }

      if (response_code != 0) {
        printf("Peer returned error code: %u\n", response_code);
        close(peer_sockfd);
        continue;
      }

      // Step 7: Receive file data (response code was 0, so file data follows)
      // Open file for writing
      FILE *fp = fopen(filename, "wb");
      if (!fp) {
        perror("failed to open file for writing");
        close(peer_sockfd);
        continue;
      }

      // Receive file data in chunks until peer closes connection
      char file_buffer[4096];
      size_t total_bytes_received = 0;

      while (1) {
        numbytes = recv(peer_sockfd, file_buffer, sizeof(file_buffer), 0);
        if (numbytes == -1) {
          perror("error receiving file data");
          fclose(fp);
          close(peer_sockfd);
          break;
        }
        if (numbytes == 0) {
          // Peer closed connection, file transfer complete
          break;
        }

        // Write received data to file
        size_t written = fwrite(file_buffer, 1, numbytes, fp);
        if (written != (size_t)numbytes) {
          fprintf(stderr, "Error writing to file\n");
          fclose(fp);
          close(peer_sockfd);
          break;
        }

        total_bytes_received += numbytes;
      }

      fclose(fp);
      close(peer_sockfd);

      printf("File transfer complete: %zu bytes received\n", total_bytes_received);
      continue; // prompt again
    } else {
      printf("Invalid command. Please enter JOIN, PUBLISH, SEARCH, FETCH, or EXIT.\n");
      continue; // prompt again
    }
  }

  // Close socket if still open
  close(sockfd);

  return 0;
}