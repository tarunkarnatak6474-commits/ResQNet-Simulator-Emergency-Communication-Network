#include "httplib.h"
#include "nlohmann/json.hpp"
#include "SimulationController.h"

#include <iostream>

using json = nlohmann::json;

int main() {

    // Create the simulation controller
    SimulationController controller;

    // =========================================
    // INITIAL DEMO NETWORK
    // =========================================

    // Create six nodes: 0 to 5 (A to F)
    for (int i = 0; i < 6; i++) {
        controller.addNode(i);
    }

    // Create network links
 controller.addLink(0, 1, 2.0); // A-B
controller.addLink(1, 2, 2.0); // B-C
controller.addLink(2, 3, 2.0); // C-D

controller.addLink(0, 4, 4.0); // A-E
controller.addLink(4, 5, 2.0); // E-F
controller.addLink(5, 3, 2.0); // F-D
    // Configure initial source and destination
controller.setEndpoints(0, 5);

    // =========================================
    // CREATE HTTP SERVER
    // =========================================

    httplib::Server server;

    // Health check endpoint
    server.Get("/", [](const httplib::Request&,
                       httplib::Response& res) {

        res.set_content(
            "ResQNet C++ Backend is Running!",
            "text/plain"
        );
    });

    // =========================================
    // GET NETWORK DATA
    // =========================================

    server.Get("/api/network",
        [&controller](const httplib::Request&,
                      httplib::Response& res) {

        const Network& network = controller.getNetwork();

        json response;

        response["nodes"] = json::array();
        response["links"] = json::array();

        // Add nodes to JSON
        const std::vector<Node>& nodes = network.getNodes();

        for (size_t i = 0; i < nodes.size(); i++) {

            json node;

            node["id"] = nodes[i].id;
            node["active"] =
                (nodes[i].status == ElementStatus::ACTIVE);

            node["status"] = statusToString(nodes[i].status);

            response["nodes"].push_back(node);
        }

        // Add links to JSON
        const std::vector<Link>& links = network.getLinks();

        for (size_t i = 0; i < links.size(); i++) {

            json link;

            link["from"] = links[i].from;
            link["to"] = links[i].to;
            link["cost"] = links[i].cost;

            // Stored status of the link
            link["storedActive"] =
                (links[i].status == ElementStatus::ACTIVE);

            // Effective status: considers both endpoint nodes
            link["active"] = network.isLinkUsable(links[i]);

            response["links"].push_back(link);
        }

        // Network summary
        response["summary"] = {
            {"totalNodes", network.getTotalNodes()},
            {"activeNodes", network.getActiveNodeCount()},
            {"failedNodes", network.getFailedNodeCount()},
            {"totalLinks", network.getTotalLinks()},
            {"activeLinks", network.getActiveLinkCount()},
            {"failedLinks", network.getFailedLinkCount()}
        };

        // Send JSON response
        res.set_content(
            response.dump(4),
            "application/json"
        );
    });
    
    // =========================================
    // GET SIMULATION DATA
    // =========================================

    server.Get("/api/simulation",
        [&controller](const httplib::Request&,
                      httplib::Response& res) {

        json response;

        response["status"] = "IDLE";
        response["source"] = 0;
        response["destination"] = 5;
        response["adaptiveRouting"] =
            controller.isAdaptiveRoutingEnabled();

        response["packetsSent"] =
            controller.getPacketsSent();

        response["packetsDelivered"] =
            controller.getPacketsDelivered();

        response["packetsLost"] =
            controller.getPacketsLost();

        res.set_content(response.dump(4), "application/json");
    });


    // =========================================
    // GET METRICS
    // =========================================

    server.Get("/api/metrics",
        [&controller](const httplib::Request&,
                      httplib::Response& res) {

        std::string metrics =
            controller.getMetricsJson("Initial Network");

        res.set_content(metrics, "application/json");
    });


    // =========================================
    // GET ROUTE INFORMATION
    // =========================================

    server.Get("/api/routes",
        [&controller](const httplib::Request&,
                      httplib::Response& res) {

        const Routing& routing = controller.getRouting();

        json response;

        response["currentRoute"] =
            routing.getCurrentRoute();

        response["routeCost"] =
            routing.getRouteCost();

        response["routeChanges"] =
            routing.getRouteChanges();

        response["adaptiveMode"] =
            routing.isAdaptiveMode();

        response["routeUsable"] =
            routing.isCurrentRouteUsable();

        res.set_content(response.dump(4), "application/json");
    });


    // =========================================
    // START SERVER
    // =========================================

    std::cout << "\n====================================\n";
    std::cout << "  ResQNet C++ Backend Server\n";
    std::cout << "====================================\n";

    std::cout << "Network initialized successfully!\n";
    std::cout << "Nodes: "
              << controller.getNetwork().getTotalNodes() << "\n";

    std::cout << "Links: "
              << controller.getNetwork().getTotalLinks() << "\n";
              // GET /api/events
server.Get("/api/events", [&controller](const httplib::Request& req,
                                       httplib::Response& res) {

    json result;

    // Failure injection events
    json failureEvents = json::array();

    const auto& injectedEvents =
        controller.getFailureSimulator().getEventLog();

    for (const auto& event : injectedEvents) {
        json item;

        item["number"] = event.number;
        item["type"] = eventTypeToString(event.type);
        item["nodeId"] = event.nodeId;
        item["linkFrom"] = event.linkFrom;
        item["linkTo"] = event.linkTo;
        item["description"] = event.description;

        failureEvents.push_back(item);
    }

    // Heartbeat detection events
    json detectionEvents = json::array();

    const auto& detectedEvents =
        controller.getFailureDetector().getEventLog();

    for (const auto& event : detectedEvents) {
        json item;

        item["number"] = event.number;
        item["type"] = detectionEventTypeToString(event.type);
        item["nodeId"] = event.nodeId;
        item["description"] = event.description;

        detectionEvents.push_back(item);
    }

    result["injectedEvents"] = failureEvents;
    result["detectedEvents"] = detectionEvents;

    res.set_content(result.dump(4), "application/json");
});
// POST /api/fail-node
server.Post("/api/fail-node", [&controller](const httplib::Request& req,
                                             httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        if (!body.contains("id") || !body["id"].is_number_integer()) {
            res.status = 400;
            res.set_content(
                R"({"error":"Missing or invalid node id"})",
                "application/json"
            );
            return;
        }

        int id = body["id"];

        ActionResult result = controller.failNode(id);

        json response;
        response["result"] = actionResultToString(result);
        response["nodeId"] = id;

        res.set_content(response.dump(4), "application/json");
    }
    catch (...) {
        res.status = 400;
        res.set_content(
            R"({"error":"Invalid JSON request"})",
            "application/json"
        );
    }
});
// POST /api/restore-node
server.Post("/api/restore-node", [&controller](const httplib::Request& req,
                                               httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        if (!body.contains("id") || !body["id"].is_number_integer()) {
            res.status = 400;
            res.set_content(
                R"({"error":"Missing or invalid node id"})",
                "application/json"
            );
            return;
        }

        int id = body["id"];

        ActionResult result = controller.restoreNode(id);

        json response;
        response["result"] = actionResultToString(result);
        response["nodeId"] = id;

        res.set_content(response.dump(4), "application/json");
    }
    catch (...) {
        res.status = 400;
        res.set_content(
            R"({"error":"Invalid JSON request"})",
            "application/json"
        );
    }
});
// POST /api/fail-link
server.Post("/api/fail-link", [&controller](const httplib::Request& req,
                                             httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        if (!body.contains("from") || !body.contains("to") ||
            !body["from"].is_number_integer() ||
            !body["to"].is_number_integer()) {

            res.status = 400;
            res.set_content(
                R"({"error":"Missing or invalid link endpoints"})",
                "application/json"
            );
            return;
        }

        int from = body["from"];
        int to = body["to"];

        ActionResult result = controller.failLink(from, to);

        json response;
        response["result"] = actionResultToString(result);
        response["from"] = from;
        response["to"] = to;

        res.set_content(response.dump(4), "application/json");
    }
    catch (...) {
        res.status = 400;
        res.set_content(
            R"({"error":"Invalid JSON request"})",
            "application/json"
        );
    }
});

// POST /api/restore-link
server.Post("/api/restore-link", [&controller](const httplib::Request& req,
                                                httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        if (!body.contains("from") || !body.contains("to") ||
            !body["from"].is_number_integer() ||
            !body["to"].is_number_integer()) {

            res.status = 400;
            res.set_content(
                R"({"error":"Missing or invalid link endpoints"})",
                "application/json"
            );
            return;
        }

        int from = body["from"];
        int to = body["to"];

        ActionResult result = controller.restoreLink(from, to);

        json response;
        response["result"] = actionResultToString(result);
        response["from"] = from;
        response["to"] = to;

        res.set_content(response.dump(4), "application/json");
    }
    catch (...) {
        res.status = 400;
        res.set_content(
            R"({"error":"Invalid JSON request"})",
            "application/json"
        );
    }
});

// POST /api/packets
server.Post("/api/packets", [&controller](const httplib::Request& req,
                                          httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        if (!body.contains("count") ||
            !body["count"].is_number_integer() ||
            body["count"].get<int>() < 0) {

            res.status = 400;
            res.set_content(
                R"({"error":"Missing or invalid packet count"})",
                "application/json"
            );
            return;
        }

        int count = body["count"];

        int delivered = controller.sendPackets(count);

        json response;
        response["sent"] = count;
        response["delivered"] = delivered;
        response["lost"] = count - delivered;

        res.set_content(response.dump(4), "application/json");
    }
    catch (...) {
        res.status = 400;
        res.set_content(
            R"({"error":"Invalid JSON request"})",
            "application/json"
        );
    }
});

// POST /api/routing
server.Post("/api/routing", [&controller](const httplib::Request& req,
                                          httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        if (!body.contains("adaptive") ||
            !body["adaptive"].is_boolean()) {

            res.status = 400;
            res.set_content(
                R"({"error":"Missing or invalid adaptive value"})",
                "application/json"
            );
            return;
        }

        bool adaptive = body["adaptive"];

        controller.setAdaptiveRouting(adaptive);

        json response;
        response["result"] = "SUCCESS";
        response["adaptiveRouting"] = controller.isAdaptiveRoutingEnabled();
        response["mode"] = adaptive ? "ADAPTIVE" : "STATIC";

        res.set_content(response.dump(4), "application/json");
    }
    catch (...) {
        res.status = 400;
        res.set_content(
            R"({"error":"Invalid JSON request"})",
            "application/json"
        );
    }
});

// POST /api/heartbeat
server.Post("/api/heartbeat", [&controller](const httplib::Request& req,
                                            httplib::Response& res) {
    int detectedEvents = controller.runHeartbeatRound();

    json response;
    response["result"] = "SUCCESS";
    response["detectedEvents"] = detectedEvents;

    response["currentRoute"] = controller.getRouting().getCurrentRoute();
    response["routeUsable"] = controller.getRouting().isCurrentRouteUsable();

    res.set_content(response.dump(4), "application/json");
});

// POST /api/restore-all
server.Post("/api/restore-all", [&controller](const httplib::Request& req,
                                              httplib::Response& res) {
    int restored = controller.restoreAll();

    json response;
    response["result"] = "SUCCESS";
    response["restoredElements"] = restored;

    res.set_content(response.dump(4), "application/json");
});


// POST /api/reset
server.Post("/api/reset", [&controller](const httplib::Request& req,
                                        httplib::Response& res) {
    controller.resetSimulation();

    json response;
    response["result"] = "SUCCESS";
    response["message"] = "Simulation reset successfully";

    res.set_content(response.dump(4), "application/json");
});

// POST /api/scenario
server.Post("/api/scenario", [&controller](const httplib::Request& req,
                                           httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        if (!body.contains("steps") || !body["steps"].is_array()) {
            res.status = 400;
            res.set_content(
                R"({"error":"Missing or invalid steps array"})",
                "application/json"
            );
            return;
        }

        std::string name = "Custom Scenario";

        if (body.contains("name") && body["name"].is_string())
            name = body["name"];

        Scenario scenario(name);

        for (const auto& step : body["steps"]) {

            if (!step.is_object() ||
                !step.contains("type") ||
                !step["type"].is_string()) {

                res.status = 400;
                res.set_content(
                    R"({"error":"Invalid scenario step"})",
                    "application/json"
                );
                return;
            }

            std::string type = step["type"];

            if (type == "failNode" || type == "restoreNode") {

                if (!step.contains("id") ||
                    !step["id"].is_number_integer()) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"Node step requires integer id"})",
                        "application/json"
                    );
                    return;
                }

                int id = step["id"];

                if (type == "failNode")
                    scenario.addFailNode(id);
                else
                    scenario.addRestoreNode(id);
            }

            else if (type == "failLink" || type == "restoreLink") {

                if (!step.contains("from") ||
                    !step.contains("to") ||
                    !step["from"].is_number_integer() ||
                    !step["to"].is_number_integer()) {

                    res.status = 400;
                    res.set_content(
                        R"({"error":"Link step requires integer from and to"})",
                        "application/json"
                    );
                    return;
                }

                int from = step["from"];
                int to = step["to"];

                if (type == "failLink")
                    scenario.addFailLink(from, to);
                else
                    scenario.addRestoreLink(from, to);
            }

            else if (type == "randomNodeFailures" ||
                     type == "randomLinkFailures") {

                if (!step.contains("count") ||
                    !step["count"].is_number_integer() ||
                    step["count"].get<int>() < 0) {

                    res.status = 400;
                    res.set_content(
                        R"({"error":"Random failure step requires non-negative count"})",
                        "application/json"
                    );
                    return;
                }

                int count = step["count"];

                if (type == "randomNodeFailures")
                    scenario.addRandomNodeFailures(count);
                else
                    scenario.addRandomLinkFailures(count);
            }

            else {
                res.status = 400;
                res.set_content(
                    R"({"error":"Unknown scenario step type"})",
                    "application/json"
                );
                return;
            }
        }

        ScenarioResult result = controller.runScenario(scenario);

        json response;
        response["result"] = "SUCCESS";
        response["scenario"] = name;
        response["totalSteps"] = result.totalSteps;
        response["changedSteps"] = result.changedSteps;
        response["noChangeSteps"] = result.noChangeSteps;
        response["invalidSteps"] = result.invalidSteps;

        res.set_content(response.dump(4), "application/json");
    }
    catch (...) {
        res.status = 400;
        res.set_content(
            R"({"error":"Invalid JSON request"})",
            "application/json"
        );
    }
});


    std::cout << "\nServer: http://localhost:8080\n";
    std::cout << "Network API: http://localhost:8080/api/network\n";
    std::cout << "\nPress Ctrl+C to stop the server.\n";

    if (!server.listen("localhost", 8080)) {
        std::cerr << "ERROR: Could not start server on port 8080.\n";
        return 1;
    }

    return 0;
}