#include "my_redis_cli.h"

/*
 A simple Redis CLI in C++ (plain TCP), demonstrating:
   1) Command-line argument parsing for -h (host) and -p (port)
   2) Two modes of operation:
      - REPL (interactive) if no extra arguments are given
      - One-shot execution if extra arguments are provided
   3) Network Communication:
       Connects to Redis using sockets
       Sends commands formatted in RESP
       Parses responses according to RESP
   4) Minimal error handling and forward-thinking design
*/

/*
The main entry: parse arguments, connect, then either run REPL or single command.
*/
int main(int argc, char *argv[]) {
    // Defaults
    std::string host = "127.0.0.1";
    // int port = 6379;
    int port = 6378;

    // Check for -h host and -p port
    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else {
            // A single Redis command
            break;
        }
        i++;
    }

    Client myClient(host, port);

    // Connect to redis
    int sockfd = myClient.connectToServer();
    if (sockfd < 0) { return 1; }

    // If i < argc, we have some leftover arguments => single command mode
    
    if (i < argc) {
        // Collect the command and its arguments from argv.
        std::vector<std::string> args;

        while (i < argc) {
            args.emplace_back(argv[i++]);
        }

        // Build the RESP request.
        std::string cmd = myClient.buildRESPCommand(args);

        // Send the complete command, handling partial sends.
        size_t totalSent = 0;

        while (totalSent < cmd.size()) {
            ssize_t n = send(
                sockfd,
                cmd.data() + totalSent,
                cmd.size() - totalSent,
                0
            );

            if (n <= 0) {
                std::cerr << "(Error) Failed to send command.\n";
                close(sockfd);
                return 1;
            }

            totalSent += static_cast<size_t>(n);
        }

        // Read and print the server's response.
        bool success = myClient.parseAndPrintRedisReply(sockfd);

        close(sockfd);
        return success ? 0 : 1;
    }

    // Otherwise, REPL mode
    while (true) {
        // Print prompt
        std::cout << host << ":" << port << "> ";
        std::fflush(stdout);

        // Read a line of input
        std::string line;
        if (!std::getline(std::cin, line)) {
            // End of file or error
            break;
        }
        line = Utils::trim(line);
        if (line.empty()) {
            continue;
        }
        // If user typed "quit", break
        if (line == "quit") {
            std::cout << "Goodbye.\n";
            break;
        }

        // Split into arguments
        auto args = myClient.splitArgs(line);
        if (args.empty()) {
            continue;
        }

        // Show built-in help
        if (args[0] == "help") {
            Utils::printHelp();
            continue;
        }

        // Build the RESP command
        std::string cmd = myClient.buildRESPCommand(args);

        // Send to Redis
        if (send(sockfd, cmd.c_str(), cmd.size(), 0) < 0) {
            std::cerr << "(Error) Failed to send command.\n";
            break;
        }

        // Parse the response
        if (!myClient.parseAndPrintRedisReply(sockfd)) {
            // If we fail to parse, let's exit to avoid a partial/inconsistent state
            break;
        }
    }

    close(sockfd);
    return 0;    
}