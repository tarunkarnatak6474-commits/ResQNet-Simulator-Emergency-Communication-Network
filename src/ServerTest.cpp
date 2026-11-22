
#include "httplib.h"
#include <iostream>

int main() {
    httplib::Server server;

    server.Get("/", [](const httplib::Request& req,
                       httplib::Response& res) {
        res.set_content(
            "ResQNet Backend is Running!",
            "text/plain"
        );
    });

    std::cout << "Server running at http://localhost:8080\n";

    server.listen("localhost", 8080);

    return 0;
}
