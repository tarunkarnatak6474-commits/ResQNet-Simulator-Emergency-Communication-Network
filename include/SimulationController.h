#ifndef RESQNET_SIMULATION_CONTROLLER_H
#define RESQNET_SIMULATION_CONTROLLER_H

#include <string>

#include "NetworkModel.h"
#include "FailureSimulator.h"
#include "FailureDetector.h"
#include "Routing.h"
#include "Metrics.h"

class SimulationController {
private:
    // One shared network
    Network network;

    // All modules use the same Network object
    FailureSimulator failureSimulator;
    FailureDetector failureDetector;
    Routing routing;

    int source;
    int destination;

    bool endpointsConfigured;
    bool adaptiveMode;

    int packetsSent;
    int packetsDelivered;

public:
    // Constructor
    SimulationController(unsigned int seed = 1)
        : failureSimulator(network, seed),
          failureDetector(network),
          routing(network) {

        source = -1;
        destination = -1;

        endpointsConfigured = false;
        adaptiveMode = false;

        packetsSent = 0;
        packetsDelivered = 0;
    }

    // Prevent copying because the modules hold references
    SimulationController(const SimulationController&) = delete;
    SimulationController& operator=(const SimulationController&) = delete;

    // ================= NETWORK SETUP =================

    bool addNode(int id) {
        if (!network.addNode(id))
            return false;

        failureDetector.registerNode(id);
        return true;
    }

    bool addLink(int a, int b, double cost = 1.0) {
        return network.addLink(a, b, cost);
    }

    // ================= ROUTING CONFIGURATION =================

    bool setEndpoints(int src, int dest) {
        if (!network.hasNode(src) || !network.hasNode(dest))
            return false;

        source = src;
        destination = dest;
        endpointsConfigured = true;

        if (adaptiveMode)
            routing.setAdaptiveRoute(source, destination);
        else
            routing.setStaticRoute(source, destination);

        return true;
    }

    void setAdaptiveRouting(bool enabled) {
        adaptiveMode = enabled;

        if (!endpointsConfigured)
            return;

        if (adaptiveMode)
            routing.setAdaptiveRoute(source, destination);
        else
            routing.setStaticRoute(source, destination);
    }

    bool isAdaptiveRoutingEnabled() const {
        return adaptiveMode;
    }

    // ================= FAILURE INJECTION =================

    ActionResult failNode(int id) {
        return failureSimulator.failNode(id);
    }

    ActionResult restoreNode(int id) {
        return failureSimulator.restoreNode(id);
    }

    ActionResult failLink(int a, int b) {
        return failureSimulator.failLink(a, b);
    }

    ActionResult restoreLink(int a, int b) {
        return failureSimulator.restoreLink(a, b);
    }
    int restoreAll() {
    return failureSimulator.restoreAll();
}
// Run a predefined failure scenario
ScenarioResult runScenario(const Scenario& scenario) {
    ScenarioResult result = failureSimulator.runScenario(scenario);

    // Recalculate route automatically in adaptive mode
    if (adaptiveMode && endpointsConfigured) {
        routing.updateRoute(source, destination);
    }

    return result;
}

    // ================= FAILURE DETECTION =================

    // One call represents one heartbeat round
    int runHeartbeatRound() {
        int detectedEvents = failureDetector.runHeartbeatRound();

        // Recalculate only after a detection/recovery event
        if (adaptiveMode &&
            endpointsConfigured &&
            detectedEvents > 0) {

            routing.updateRoute(source, destination);
        }

        return detectedEvents;
    }

    // ================= PACKET SIMULATION =================

    // Simulates sending a batch of packets over the current route
    int sendPackets(int count) {
        if (count < 0)
            return 0;

        packetsSent += count;

        if (!endpointsConfigured || count == 0)
            return 0;

        // A packet is delivered only if the complete route is usable
        if (routing.isCurrentRouteUsable()) {
            packetsDelivered += count;
            return count;
        }

        return 0;
    }

    // ================= METRICS =================

    NetworkScenario buildScenario(const std::string& name) const {
        return NetworkScenario(
            name,
            packetsSent,
            packetsDelivered,
            network.getTotalNodes(),
            network.getFailedNodeCount(),
            network.getActiveLinkCount(),
            network.getFailedLinkCount(),
            routing.getRouteChanges(),
            network.getTotalLinks()
        );
    }

    MetricsResult getMetrics(const std::string& name) const {
        NetworkScenario scenario = buildScenario(name);
        return calculateMetrics(scenario);
    }

    std::string getMetricsJson(const std::string& name) const {
        MetricsResult result = getMetrics(name);
        return toJson(result);
    }

    void displayMetrics(const std::string& name) const {
        NetworkScenario scenario = buildScenario(name);
        displayReport(scenario);
    }
// ================= RESET SIMULATION =================

void resetSimulation() {

    // Restore all failed nodes and links
    failureSimulator.restoreAll();

    // Clear old failure injection events
    failureSimulator.clearEventLog();

    // Reset heartbeat counters and detection history
    failureDetector.resetAll();
    failureDetector.clearEventLog();

    // Reset packet counters
    packetsSent = 0;
    packetsDelivered = 0;

    // Clear old route
    routing.clearRoute();

    // Recalculate the original route
    if (endpointsConfigured) {
        if (adaptiveMode)
            routing.setAdaptiveRoute(source, destination);
        else
            routing.setStaticRoute(source, destination);
    }

    // Reset route-change counter
    routing.resetRouteChanges();
}
    // ================= STATUS / ACCESS =================

    int getPacketsSent() const {
        return packetsSent;
    }

    int getPacketsDelivered() const {
        return packetsDelivered;
    }

    int getPacketsLost() const {
        return packetsSent - packetsDelivered;
    }

    const Network& getNetwork() const {
        return network;
    }

    const FailureSimulator& getFailureSimulator() const {
        return failureSimulator;
    }

    const FailureDetector& getFailureDetector() const {
        return failureDetector;
    }

    const Routing& getRouting() const {
        return routing;
    }
};

#endif