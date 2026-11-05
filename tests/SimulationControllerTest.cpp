#include <iostream>
#include <cassert>

#include "SimulationController.h"

using namespace std;

// Build a small network with two possible routes:
//
//        2
//       / \
//      1   4
//       \ /
//        3
//
// Route 1: 1 -> 2 -> 4
// Route 2: 1 -> 3 -> 4

void buildTestNetwork(SimulationController& sim) {
    assert(sim.addNode(1));
    assert(sim.addNode(2));
    assert(sim.addNode(3));
    assert(sim.addNode(4));

    assert(sim.addLink(1, 2, 1));
    assert(sim.addLink(2, 4, 1));

    assert(sim.addLink(1, 3, 2));
    assert(sim.addLink(3, 4, 2));
}

// Test 1: Network setup
void testNetworkSetup() {
    SimulationController sim;
    buildTestNetwork(sim);

    assert(sim.getNetwork().getTotalNodes() == 4);
    assert(sim.getNetwork().getTotalLinks() == 4);

    cout << "Network setup test: PASS\n";
}

// Test 2: Initial packet delivery
void testInitialDelivery() {
    SimulationController sim;
    buildTestNetwork(sim);

    assert(sim.setEndpoints(1, 4));

    int delivered = sim.sendPackets(10);

    assert(delivered == 10);
    assert(sim.getPacketsSent() == 10);
    assert(sim.getPacketsDelivered() == 10);

    cout << "Initial packet delivery test: PASS\n";
}

// Test 3: Failure detection threshold
void testFailureDetection() {
    SimulationController sim;
    buildTestNetwork(sim);

    assert(sim.setEndpoints(1, 4));
    sim.setAdaptiveRouting(true);

    assert(sim.failNode(2) == ActionResult::SUCCESS);

    // First heartbeat round
    assert(sim.runHeartbeatRound() == 0);

    // Second heartbeat round
    assert(sim.runHeartbeatRound() == 0);

    // Third heartbeat round: failure detected
    assert(sim.runHeartbeatRound() == 1);

    assert(sim.getFailureDetector().isDeclaredFailed(2));

    cout << "Failure detection test: PASS\n";
}

// Test 4: Adaptive rerouting
void testAdaptiveRerouting() {
    SimulationController sim;
    buildTestNetwork(sim);

    assert(sim.setEndpoints(1, 4));
    sim.setAdaptiveRouting(true);

    // Initially, packets use route 1-2-4
    assert(sim.sendPackets(10) == 10);

    // Fail node 2
    assert(sim.failNode(2) == ActionResult::SUCCESS);

    // Old route is now unusable
    assert(sim.sendPackets(5) == 0);

    // Run 3 heartbeat rounds
    sim.runHeartbeatRound();
    sim.runHeartbeatRound();
    sim.runHeartbeatRound();

    // Adaptive routing should find route 1-3-4
    assert(sim.getRouting().isCurrentRouteUsable());

    // Packets can now be delivered
    assert(sim.sendPackets(10) == 10);

    assert(sim.getPacketsSent() == 25);
    assert(sim.getPacketsDelivered() == 20);
    assert(sim.getPacketsLost() == 5);

    cout << "Adaptive rerouting test: PASS\n";
}

// Test 5: Recovery detection
void testRecoveryDetection() {
    SimulationController sim;
    buildTestNetwork(sim);

    assert(sim.setEndpoints(1, 4));
    sim.setAdaptiveRouting(true);

    // Fail node 2 and detect the failure
    assert(sim.failNode(2) == ActionResult::SUCCESS);

    sim.runHeartbeatRound();
    sim.runHeartbeatRound();
    sim.runHeartbeatRound();

    assert(sim.getFailureDetector().isDeclaredFailed(2));

    // Restore node 2
    assert(sim.restoreNode(2) == ActionResult::SUCCESS);

    // One successful heartbeat detects recovery
    assert(sim.runHeartbeatRound() == 1);

    assert(!sim.getFailureDetector().isDeclaredFailed(2));

    cout << "Recovery detection test: PASS\n";
}

// Test 6: Metrics calculation
void testMetrics() {
    SimulationController sim;
    buildTestNetwork(sim);

    assert(sim.setEndpoints(1, 4));
    sim.setAdaptiveRouting(true);

    sim.sendPackets(10);

    assert(sim.failNode(2) == ActionResult::SUCCESS);

    sim.runHeartbeatRound();
    sim.runHeartbeatRound();
    sim.runHeartbeatRound();

    sim.sendPackets(10);

    MetricsResult result = sim.getMetrics("Adaptive Test");

    assert(result.valid);
    assert(result.packetsSent == 20);
    assert(result.packetsDelivered == 20);
    assert(result.packetsLost == 0);
    assert(result.failedNodes == 1);

    cout << "Metrics integration test: PASS\n";
}

// Test 7: JSON output
void testJsonOutput() {
    SimulationController sim;
    buildTestNetwork(sim);

    assert(sim.setEndpoints(1, 4));
    sim.sendPackets(10);

    string json = sim.getMetricsJson("JSON Test");

    assert(json.find("\"name\":\"JSON Test\"") != string::npos);
    assert(json.find("\"packetsSent\":10") != string::npos);
    assert(json.find("\"packetsDelivered\":10") != string::npos);

    cout << "JSON output test: PASS\n";
}

// Test 8: Invalid configuration
void testInvalidConfiguration() {
    SimulationController sim;
    buildTestNetwork(sim);

    assert(!sim.addNode(1));          // Duplicate node
    assert(!sim.setEndpoints(1, 99)); // Missing destination
    assert(sim.sendPackets(5) == 0);

    cout << "Invalid configuration test: PASS\n";
}

int main() {
    cout << "\n====================================\n";
    cout << " SIMULATION CONTROLLER TESTS\n";
    cout << "====================================\n\n";

    testNetworkSetup();
    testInitialDelivery();
    testFailureDetection();
    testAdaptiveRerouting();
    testRecoveryDetection();
    testMetrics();
    testJsonOutput();
    testInvalidConfiguration();

    cout << "\nAll Simulation Controller tests passed!\n";

    return 0;
}