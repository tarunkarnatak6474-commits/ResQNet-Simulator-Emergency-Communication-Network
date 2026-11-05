// ============================================================
// FailureDetectorTest.cpp  -  tests for FailureDetector.h
//
// Everything is deterministic: heartbeat outcomes are either
// scripted (recordHeartbeatSuccess / recordHeartbeatMiss) or
// come from the real Network node status (sendHeartbeat).
// No timers, threads or random numbers are used.
// ============================================================

#include <iostream>
#include <string>
#include "NetworkModel.h"
#include "FailureSimulator.h"
#include "FailureDetector.h"

static int passedCount = 0;
static int failedCount = 0;

static void check(bool condition, const std::string& name) {
    if (condition) {
        passedCount++;
    } else {
        failedCount++;
        std::cout << "  FAIL: " << name << '\n';
    }
}

static void section(const std::string& title) {
    std::cout << "[Test] " << title << '\n';
}

// Test network:  nodes 1..5, links 1-2, 2-3, 3-4, 4-5, 1-5
static void buildNetwork(Network& net) {
    for (int id = 1; id <= 5; id++)
        net.addNode(id);
    net.addLink(1, 2, 1.0);
    net.addLink(2, 3, 2.0);
    net.addLink(3, 4, 1.5);
    net.addLink(4, 5, 1.0);
    net.addLink(1, 5, 3.0);
}

// ------------------------------------------------------------
void testDefaultsAndConfig() {
    section("Defaults and configuration");
    Network net;
    buildNetwork(net);

    FailureDetector d(net);
    check(d.getMissThreshold() == 3, "default miss threshold is 3");
    check(d.getRecoveryThreshold() == 1, "default recovery threshold is 1");
    check(d.getMonitoredCount() == 0, "nothing monitored at start");
    check(d.getEventCount() == 0, "no events at start");

    FailureDetector d2(net, 5, 2);
    check(d2.getMissThreshold() == 5, "custom miss threshold");
    check(d2.getRecoveryThreshold() == 2, "custom recovery threshold");

    FailureDetector d3(net, 0, -4);
    check(d3.getMissThreshold() == 3, "invalid miss threshold -> default");
    check(d3.getRecoveryThreshold() == 1, "invalid recovery threshold -> default");

    check(d.setMissThreshold(4), "setMissThreshold(4) accepted");
    check(d.getMissThreshold() == 4, "miss threshold changed to 4");
    check(!d.setMissThreshold(0), "setMissThreshold(0) rejected");
    check(!d.setMissThreshold(-1), "setMissThreshold(-1) rejected");
    check(d.getMissThreshold() == 4, "threshold unchanged after rejection");
    check(d.setRecoveryThreshold(2), "setRecoveryThreshold(2) accepted");
    check(!d.setRecoveryThreshold(0), "setRecoveryThreshold(0) rejected");
    check(d.getRecoveryThreshold() == 2, "recovery threshold stays 2");
}

// ------------------------------------------------------------
void testRegistration() {
    section("Registering valid and invalid nodes");
    Network net;
    buildNetwork(net);
    FailureDetector d(net);

    check(d.registerNode(1) == RegisterResult::REGISTERED, "register node 1");
    check(d.isMonitored(1), "node 1 is monitored");
    check(d.getMonitoredCount() == 1, "monitored count is 1");
    check(d.getMissedCount(1) == 0, "new node has 0 missed");
    check(!d.isDeclaredFailed(1), "new node not declared failed");

    check(d.registerNode(1) == RegisterResult::ALREADY_MONITORED,
          "duplicate registration reported");
    check(d.getMonitoredCount() == 1, "duplicate did not add a second entry");

    check(d.registerNode(99) == RegisterResult::NOT_FOUND, "unknown id 99");
    check(d.registerNode(-1) == RegisterResult::NOT_FOUND, "negative id -1");
    check(d.registerNode(0) == RegisterResult::NOT_FOUND, "id 0 (not in network)");
    check(!d.isMonitored(99), "99 not monitored");
    check(d.getMonitoredCount() == 1, "invalid ids not counted");

    check(d.registerNode(3) == RegisterResult::REGISTERED, "register node 3");
    std::vector<int> ids = d.getMonitoredIds();
    check(ids.size() == 2 && ids[0] == 1 && ids[1] == 3,
          "monitored ids in registration order");

    check(d.registerAllNodes() == 3, "registerAllNodes adds the other 3");
    check(d.getMonitoredCount() == 5, "all 5 nodes monitored");
    check(d.registerAllNodes() == 0, "registerAllNodes again adds none");

    check(d.unregisterNode(3), "unregister node 3");
    check(!d.isMonitored(3), "node 3 no longer monitored");
    check(!d.unregisterNode(3), "unregister again returns false");
    check(!d.unregisterNode(99), "unregister unknown returns false");
    check(net.hasNode(3) && net.isNodeActive(3),
          "unregistering does not change the Network");

    // Registering a second time must keep existing heartbeat state
    FailureDetector d2(net);
    d2.registerNode(2);
    d2.recordHeartbeatMiss(2);
    d2.registerNode(2);
    check(d2.getMissedCount(2) == 1, "re-register keeps missed count");
}

// ------------------------------------------------------------
void testSuccessfulHeartbeats() {
    section("Successful heartbeats");
    Network net;
    buildNetwork(net);
    FailureDetector d(net);
    d.registerNode(1);

    check(d.sendHeartbeat(1) == HeartbeatEffect::NONE,
          "heartbeat to active node: no effect");
    check(d.getMissedCount(1) == 0, "missed stays 0");
    check(d.getHeartbeatsSent(1) == 1, "1 heartbeat sent");
    check(d.getResponsesReceived(1) == 1, "1 response received");
    check(d.getConsecutiveSuccesses(1) == 1, "1 consecutive success");

    d.recordHeartbeatSuccess(1);
    d.sendHeartbeat(1);
    check(d.getHeartbeatsSent(1) == 3, "3 heartbeats sent");
    check(d.getResponsesReceived(1) == 3, "3 responses received");
    check(d.getEventCount() == 0, "healthy node creates no events");
    check(net.isNodeActive(1), "network node still ACTIVE");
    check(!d.isDeclaredFailed(1), "not declared failed");
}

// ------------------------------------------------------------
void testMissedHeartbeats() {
    section("One and multiple missed heartbeats (threshold 3)");
    Network net;
    buildNetwork(net);
    FailureDetector d(net);       // threshold 3
    d.registerNode(2);

    check(d.recordHeartbeatMiss(2) == HeartbeatEffect::NONE, "miss 1: no effect");
    check(d.getMissedCount(2) == 1, "missed count 1");
    check(!d.isDeclaredFailed(2), "not declared after 1 miss");
    check(net.isNodeActive(2), "network node 2 still ACTIVE after 1 miss");
    check(d.getEventCount() == 0, "no event after 1 miss");
    check(!d.hasReachedThreshold(2), "threshold not reached at 1");

    check(d.recordHeartbeatMiss(2) == HeartbeatEffect::NONE, "miss 2: no effect");
    check(d.getMissedCount(2) == 2, "missed count 2");
    check(!d.isDeclaredFailed(2), "not declared after 2 misses (no premature)");
    check(net.isNodeActive(2), "network node 2 still ACTIVE after 2 misses");
    check(d.getEventCount() == 0, "no event after 2 misses");
    check(!d.hasReachedThreshold(2), "threshold not reached at 2");
    check(d.getConsecutiveSuccesses(2) == 0, "no successes counted");
}

// ------------------------------------------------------------
void testExactThreshold() {
    section("Failure detected at the exact threshold");
    Network net;
    buildNetwork(net);
    FailureDetector d(net);       // threshold 3
    d.registerNode(2);

    d.recordHeartbeatMiss(2);
    d.recordHeartbeatMiss(2);
    HeartbeatEffect e = d.recordHeartbeatMiss(2);

    check(e == HeartbeatEffect::FAILURE_DETECTED, "3rd miss detects failure");
    check(d.isDeclaredFailed(2), "node 2 declared failed");
    check(d.getMissedCount(2) == 3, "missed count is 3");
    check(d.hasReachedThreshold(2), "threshold reached");
    check(!net.isNodeActive(2), "Network node 2 updated to FAILED");
    check(d.getEventCount() == 1, "one detection event");
    check(d.getEventLog()[0].type == DetectionEventType::FAILURE_DETECTED,
          "event type is FAILURE_DETECTED");
    check(d.getEventLog()[0].nodeId == 2, "event node id is 2");
    check(d.getEventLog()[0].number == 1, "event number is 1");
    check(d.getDeclaredFailedCount() == 1, "1 node declared failed");

    // Repeated misses after detection: safe, no duplicate event
    check(d.recordHeartbeatMiss(2) == HeartbeatEffect::NONE,
          "extra miss after detection: no new effect");
    check(d.getEventCount() == 1, "still only one event");
    check(d.getMissedCount(2) == 4, "missed count keeps counting");
    check(!net.isNodeActive(2), "node stays FAILED");

    // Threshold 1: first miss detects
    Network net2;
    buildNetwork(net2);
    FailureDetector d1(net2, 1);
    d1.registerNode(4);
    check(d1.recordHeartbeatMiss(4) == HeartbeatEffect::FAILURE_DETECTED,
          "threshold 1: first miss detects");
    check(!net2.isNodeActive(4), "threshold 1: node failed in Network");
}

// ------------------------------------------------------------
void testNoPrematureAndConsecutive() {
    section("No premature detection; misses must be consecutive");
    Network net;
    buildNetwork(net);
    FailureDetector d(net, 5);    // threshold 5
    d.registerNode(3);

    for (int i = 0; i < 4; i++)
        d.recordHeartbeatMiss(3);
    check(d.getMissedCount(3) == 4, "4 misses counted");
    check(!d.isDeclaredFailed(3), "4 of 5: not declared");
    check(net.isNodeActive(3), "4 of 5: Network unchanged");

    d.recordHeartbeatSuccess(3);
    check(d.getMissedCount(3) == 0, "a response resets missed count");

    for (int i = 0; i < 4; i++)
        d.recordHeartbeatMiss(3);
    check(!d.isDeclaredFailed(3), "4 more misses (not consecutive with first 4)");
    check(d.getEventCount() == 0, "no events at all");

    check(d.recordHeartbeatMiss(3) == HeartbeatEffect::FAILURE_DETECTED,
          "5th consecutive miss detects");
    check(d.getMissedCount(3) == 5, "missed count 5");
}

// ------------------------------------------------------------
void testRecovery() {
    section("Recovery after responses resume");
    Network net;
    buildNetwork(net);
    FailureDetector d(net);       // miss 3, recovery 1
    d.registerNode(2);

    d.recordHeartbeatMiss(2);
    d.recordHeartbeatMiss(2);
    d.recordHeartbeatMiss(2);
    check(!net.isNodeActive(2), "node failed after detection");

    HeartbeatEffect e = d.recordHeartbeatSuccess(2);
    check(e == HeartbeatEffect::RECOVERY_DETECTED, "response -> recovery detected");
    check(!d.isDeclaredFailed(2), "no longer declared failed");
    check(d.getMissedCount(2) == 0, "missed count reset to 0");
    check(net.isNodeActive(2), "Network node 2 restored");
    check(d.getEventCount() == 2, "two events (failure + recovery)");
    check(d.getEventLog()[1].type == DetectionEventType::RECOVERY_DETECTED,
          "second event is RECOVERY_DETECTED");
    check(d.getEventLog()[1].number == 2, "second event number is 2");

    // Another response on a healthy node: nothing more happens
    check(d.recordHeartbeatSuccess(2) == HeartbeatEffect::NONE,
          "extra response: no new effect");
    check(d.getEventCount() == 2, "still 2 events");

    // Recovery threshold 2
    Network net2;
    buildNetwork(net2);
    FailureDetector d2(net2, 2, 2);
    d2.registerNode(1);
    d2.recordHeartbeatMiss(1);
    d2.recordHeartbeatMiss(1);
    check(d2.isDeclaredFailed(1), "d2: failure detected at 2 misses");
    check(d2.recordHeartbeatSuccess(1) == HeartbeatEffect::NONE,
          "d2: first response, not recovered yet");
    check(d2.isDeclaredFailed(1), "d2: still declared failed");
    check(!net2.isNodeActive(1), "d2: Network still FAILED");

    d2.recordHeartbeatMiss(1);   // a miss breaks the streak
    check(d2.getConsecutiveSuccesses(1) == 0, "d2: miss resets success streak");
    d2.recordHeartbeatSuccess(1);
    check(d2.isDeclaredFailed(1), "d2: 1 response after streak reset: no recovery");
    check(d2.recordHeartbeatSuccess(1) == HeartbeatEffect::RECOVERY_DETECTED,
          "d2: second response in a row recovers");
    check(net2.isNodeActive(1), "d2: Network node restored");
    check(d2.getEventCount() == 2, "d2: exactly 2 events");
}

// ------------------------------------------------------------
void testRepeatedCycles() {
    section("Repeated failure and recovery");
    Network net;
    buildNetwork(net);
    FailureDetector d(net, 2, 1);
    d.registerNode(4);

    for (int cycle = 0; cycle < 3; cycle++) {
        d.recordHeartbeatMiss(4);
        check(d.recordHeartbeatMiss(4) == HeartbeatEffect::FAILURE_DETECTED,
              "cycle failure detected");
        check(!net.isNodeActive(4), "cycle: node failed in Network");
        check(d.recordHeartbeatSuccess(4) == HeartbeatEffect::RECOVERY_DETECTED,
              "cycle recovery detected");
        check(net.isNodeActive(4), "cycle: node restored in Network");
    }
    check(d.getEventCount() == 6, "6 events after 3 cycles");
    check(d.countEvents(DetectionEventType::FAILURE_DETECTED) == 3, "3 failure events");
    check(d.countEvents(DetectionEventType::RECOVERY_DETECTED) == 3, "3 recovery events");

    bool numbersOk = true;
    for (int i = 0; i < d.getEventCount(); i++) {
        if (d.getEventLog()[i].number != i + 1)
            numbersOk = false;
    }
    check(numbersOk, "event numbers are 1..6 in order");
    check(d.getEventLog()[0].type == DetectionEventType::FAILURE_DETECTED &&
          d.getEventLog()[1].type == DetectionEventType::RECOVERY_DETECTED,
          "events alternate failure / recovery");
}

// ------------------------------------------------------------
void testMultipleNodes() {
    section("Multiple monitored nodes");
    Network net;
    buildNetwork(net);
    FailureDetector d(net);       // threshold 3
    d.registerNode(1);
    d.registerNode(2);
    d.registerNode(3);

    // Only node 2 stops responding (scripted)
    for (int i = 0; i < 3; i++) {
        d.recordHeartbeatSuccess(1);
        d.recordHeartbeatMiss(2);
        d.recordHeartbeatSuccess(3);
    }
    check(d.isDeclaredFailed(2), "node 2 declared failed");
    check(!d.isDeclaredFailed(1), "node 1 not declared failed");
    check(!d.isDeclaredFailed(3), "node 3 not declared failed");
    check(net.isNodeActive(1) && net.isNodeActive(3), "nodes 1 and 3 still ACTIVE");
    check(!net.isNodeActive(2), "node 2 FAILED in Network");
    check(d.getMissedCount(1) == 0 && d.getMissedCount(3) == 0,
          "healthy nodes have 0 missed");
    check(d.getMissedCount(2) == 3, "node 2 has 3 missed");
    check(d.getEventCount() == 1, "exactly one event");
    check(d.getDeclaredFailedCount() == 1, "1 node declared failed");

    // Counters are independent per node
    d.recordHeartbeatMiss(1);
    check(d.getMissedCount(1) == 1 && d.getMissedCount(3) == 0,
          "miss on node 1 does not affect node 3");

    // Two nodes failing in the same tick pattern
    d.recordHeartbeatMiss(1);
    d.recordHeartbeatMiss(1);
    check(d.isDeclaredFailed(1) && d.isDeclaredFailed(2), "nodes 1 and 2 both failed");
    check(d.getDeclaredFailedCount() == 2, "2 nodes declared failed");
    check(d.getEventCount() == 2, "two events");
    check(d.getEventLog()[1].nodeId == 1, "second event is for node 1");
}

// ------------------------------------------------------------
void testInvalidIds() {
    section("Invalid and unmonitored node IDs");
    Network net;
    buildNetwork(net);
    FailureDetector d(net);
    d.registerNode(1);

    check(d.sendHeartbeat(99) == HeartbeatEffect::NOT_MONITORED, "sendHeartbeat(99)");
    check(d.sendHeartbeat(-1) == HeartbeatEffect::NOT_MONITORED, "sendHeartbeat(-1)");
    check(d.recordHeartbeatSuccess(99) == HeartbeatEffect::NOT_MONITORED, "success(99)");
    check(d.recordHeartbeatMiss(99) == HeartbeatEffect::NOT_MONITORED, "miss(99)");
    check(d.checkTimeout(99) == HeartbeatEffect::NOT_MONITORED, "checkTimeout(99)");
    check(d.getMissedCount(99) == -1, "getMissedCount(99) == -1");
    check(d.getConsecutiveSuccesses(99) == -1, "getConsecutiveSuccesses(99) == -1");
    check(d.getHeartbeatsSent(99) == -1, "getHeartbeatsSent(99) == -1");
    check(d.getResponsesReceived(99) == -1, "getResponsesReceived(99) == -1");
    check(!d.isDeclaredFailed(99), "isDeclaredFailed(99) false");
    check(!d.hasReachedThreshold(99), "hasReachedThreshold(99) false");
    check(!d.resetNode(99), "resetNode(99) false");

    // Real node that exists in the Network but is not registered
    check(d.sendHeartbeat(5) == HeartbeatEffect::NOT_MONITORED,
          "node 5 exists but is not monitored");
    check(d.recordHeartbeatMiss(5) == HeartbeatEffect::NOT_MONITORED,
          "miss on unmonitored node ignored");
    check(net.isNodeActive(5), "unmonitored node untouched in Network");

    check(d.getEventCount() == 0, "invalid calls created no events");
    check(d.getMissedCount(1) == 0, "monitored node unaffected by invalid calls");
    check(net.getFailedNodeCount() == 0, "Network unchanged by invalid calls");
}

// ------------------------------------------------------------
void testSimulatorCompatibility() {
    section("Compatibility with FailureSimulator (injected failures)");
    Network net;
    buildNetwork(net);
    FailureSimulator sim(net);
    FailureDetector d(net);       // threshold 3
    d.registerAllNodes();

    // Inject a failure. The detector must NOT know yet.
    check(sim.failNode(2) == ActionResult::SUCCESS, "simulator fails node 2");
    check(!net.isNodeActive(2), "Network shows node 2 FAILED");
    check(!d.isDeclaredFailed(2), "detector has not detected it yet");
    check(d.getMissedCount(2) == 0, "no missed heartbeats before any are sent");
    check(d.getEventCount() == 0, "detector log empty");

    check(d.runHeartbeatRound() == 0, "round 1: no detection");
    check(d.getMissedCount(2) == 1, "round 1: node 2 missed 1");
    check(!d.isDeclaredFailed(2), "round 1: not declared");
    check(d.runHeartbeatRound() == 0, "round 2: no detection");
    check(d.getMissedCount(2) == 2, "round 2: node 2 missed 2");
    check(!d.isDeclaredFailed(2), "round 2: not declared (no premature)");
    check(d.getMissedCount(1) == 0 && d.getMissedCount(3) == 0,
          "healthy nodes never missed");

    check(d.runHeartbeatRound() == 1, "round 3: one failure detected");
    check(d.isDeclaredFailed(2), "round 3: node 2 declared failed");
    check(!net.isNodeActive(2), "node 2 still FAILED in Network");
    check(d.getEventCount() == 1, "detector logged 1 event");
    check(sim.getEventCount() == 1, "simulator log still has only its own 1 event");
    check(sim.getEventLog()[0].type == EventType::NODE_FAILED,
          "simulator log: NODE_FAILED");
    check(d.getEventLog()[0].type == DetectionEventType::FAILURE_DETECTED,
          "detector log: FAILURE_DETECTED");

    // Simulator restores the node; the detector notices on the next heartbeat
    check(d.getEventCount() == 1, "restore not noticed before a heartbeat");
    check(sim.restoreNode(2) == ActionResult::SUCCESS, "simulator restores node 2");
    check(d.isDeclaredFailed(2), "detector still believes node 2 failed");
    check(d.runHeartbeatRound() == 1, "next round: recovery detected");
    check(!d.isDeclaredFailed(2), "detector now believes node 2 alive");
    check(net.isNodeActive(2), "node 2 ACTIVE in Network");
    check(sim.getEventCount() == 2, "simulator log: fail + restore only");
    check(d.getEventCount() == 2, "detector log: detect + recover");

    // Detector's own network updates are NOT in the simulator's log
    Network net2;
    buildNetwork(net2);
    FailureSimulator sim2(net2);
    FailureDetector d2(net2, 2);
    d2.registerNode(3);
    d2.recordHeartbeatMiss(3);
    d2.recordHeartbeatMiss(3);
    check(!net2.isNodeActive(3), "detector failed node 3 (scripted misses)");
    check(sim2.getEventCount() == 0, "simulator log untouched by detector");

    // Simulator can restore a node the detector failed
    check(sim2.restoreNode(3) == ActionResult::SUCCESS,
          "simulator can restore detector-failed node");
    check(d2.sendHeartbeat(3) == HeartbeatEffect::RECOVERY_DETECTED,
          "detector sees recovery after simulator restore");
    check(d2.getEventCount() == 2, "d2: failure + recovery logged");

    // Random failures are detected only after enough heartbeats
    Network net3;
    buildNetwork(net3);
    FailureSimulator sim3(net3, 42);
    FailureDetector d3(net3, 2);
    d3.registerAllNodes();
    std::vector<int> hit = sim3.failRandomNodes(2);
    check(hit.size() == 2, "simulator failed 2 random nodes");
    check(d3.getEventCount() == 0, "random failures not auto-detected");
    d3.runHeartbeatRound();
    check(d3.getEventCount() == 0, "round 1 of threshold 2: still nothing");
    check(d3.runHeartbeatRound() == 2, "round 2: both random failures detected");
    check(d3.getDeclaredFailedCount() == 2, "2 nodes declared failed");

    // Scenario compatibility: node 2 fails, link 3-4 fails, node 2 restored
    Network net4;
    buildNetwork(net4);
    FailureSimulator sim4(net4);
    FailureDetector d4(net4);
    d4.registerAllNodes();
    sim4.runScenario(createNodeAndLinkScenario(2, 3, 4));
    d4.runHeartbeatRound();
    d4.runHeartbeatRound();
    d4.runHeartbeatRound();
    check(d4.getEventCount() == 0, "scenario ended with node 2 active: nothing to detect");
    check(d4.getMissedCount(2) == 0, "node 2 never missed a heartbeat");
    check(!net4.isLinkActive(3, 4), "link 3-4 failure is not a node failure");
}

// ------------------------------------------------------------
void testLinkStateSafety() {
    section("Recovery does not overwrite independently failed links");
    Network net;
    buildNetwork(net);
    FailureSimulator sim(net);
    FailureDetector d(net);       // miss 3, recovery 1
    d.registerAllNodes();

    sim.failLink(1, 2);           // link 1-2 fails on its own
    sim.failNode(1);              // then node 1 fails
    d.runHeartbeatRound();
    d.runHeartbeatRound();
    d.runHeartbeatRound();
    check(d.isDeclaredFailed(1), "node 1 failure detected");
    check(net.getStoredFailedLinkCount() == 1, "only link 1-2 stored as failed");

    sim.restoreNode(1);
    d.runHeartbeatRound();
    check(!d.isDeclaredFailed(1), "node 1 recovery detected");
    check(net.isNodeActive(1), "node 1 ACTIVE");
    check(!net.isLinkActive(1, 2), "link 1-2 STILL failed (not overwritten)");
    check(net.isLinkActive(1, 5), "link 1-5 still active");
    check(net.getStoredFailedLinkCount() == 1, "stored failed links still 1");

    // Detector-driven fail and restore of node 3 with link 3-4 failed
    Network net2;
    buildNetwork(net2);
    FailureDetector d2(net2, 2);
    d2.registerNode(3);
    net2.failLink(3, 4);
    d2.recordHeartbeatMiss(3);
    d2.recordHeartbeatMiss(3);
    check(!net2.isNodeActive(3), "d2: node 3 failed by detector");
    check(net2.getNodeBlockedLinkCount() == 1, "d2: link 2-3 blocked by failed node");
    d2.recordHeartbeatSuccess(3);
    check(net2.isNodeActive(3), "d2: node 3 restored by detector");
    check(!net2.isLinkActive(3, 4), "d2: link 3-4 still failed after recovery");
    check(net2.isLinkUsable(2, 3), "d2: link 2-3 usable again");
    check(net2.getNodeBlockedLinkCount() == 0, "d2: no links blocked any more");
    check(net2.getStoredFailedLinkCount() == 1, "d2: stored failed links still 1");

    // Detector never changes link status by itself
    Network net3;
    buildNetwork(net3);
    FailureDetector d3(net3, 1);
    d3.registerNode(2);
    d3.recordHeartbeatMiss(2);
    check(net3.getStoredFailedLinkCount() == 0, "d3: node failure leaves link status alone");
    check(net3.getFailedLinkCount() == 2, "d3: links 1-2 and 2-3 unusable (effective)");
}

// ------------------------------------------------------------
void testThresholdChecks() {
    section("Timeout checks after changing the threshold");
    Network net;
    buildNetwork(net);
    FailureDetector d(net);       // threshold 3
    d.registerNode(1);
    d.registerNode(2);
    d.registerNode(3);

    d.recordHeartbeatMiss(1);
    d.recordHeartbeatMiss(1);
    d.recordHeartbeatMiss(2);
    check(d.getEventCount() == 0, "no detection yet");

    check(d.setMissThreshold(2), "lower threshold to 2");
    check(d.getEventCount() == 0, "lowering threshold does not detect by itself");
    check(d.hasReachedThreshold(1), "node 1 now at/above threshold");
    check(!d.hasReachedThreshold(2), "node 2 (1 miss) below threshold");
    check(d.checkTimeout(2) == HeartbeatEffect::NONE, "checkTimeout(2): nothing");
    check(d.checkAllTimeouts() == 1, "checkAllTimeouts finds node 1");
    check(d.isDeclaredFailed(1), "node 1 declared failed");
    check(!net.isNodeActive(1), "node 1 FAILED in Network");
    check(d.checkTimeout(1) == HeartbeatEffect::NONE, "checkTimeout again: no repeat");
    check(d.checkAllTimeouts() == 0, "checkAllTimeouts again: nothing new");
    check(d.getEventCount() == 1, "still only 1 event");
}

// ------------------------------------------------------------
void testSendHeartbeatUsesNetworkState() {
    section("sendHeartbeat follows the real Network state");
    Network net;
    buildNetwork(net);
    FailureDetector d(net, 2);
    d.registerNode(5);

    net.failNode(5);                       // changed directly on Network
    check(d.sendHeartbeat(5) == HeartbeatEffect::NONE, "heartbeat 1 to dead node: miss");
    check(d.getMissedCount(5) == 1, "1 miss recorded");
    check(d.getResponsesReceived(5) == 0, "no response received");
    check(d.sendHeartbeat(5) == HeartbeatEffect::FAILURE_DETECTED,
          "heartbeat 2 to dead node: detected");
    check(!net.isNodeActive(5), "Network node stays FAILED (no double change)");
    check(d.getEventCount() == 1, "one event");

    net.restoreNode(5);
    check(d.sendHeartbeat(5) == HeartbeatEffect::RECOVERY_DETECTED,
          "heartbeat to restored node: recovery");
    check(d.getHeartbeatsSent(5) == 3, "3 heartbeats sent in total");
    check(d.getResponsesReceived(5) == 1, "1 response in total");
}

// ------------------------------------------------------------
void testResetAndClear() {
    section("Resetting monitoring state");
    Network net;
    buildNetwork(net);
    FailureDetector d(net);
    d.registerNode(1);
    d.registerNode(2);

    d.recordHeartbeatMiss(1);
    d.recordHeartbeatMiss(1);
    d.recordHeartbeatMiss(1);
    d.recordHeartbeatMiss(2);
    check(d.isDeclaredFailed(1), "node 1 failed before reset");

    check(d.resetNode(1), "resetNode(1) ok");
    check(d.getMissedCount(1) == 0, "node 1 missed count cleared");
    check(!d.isDeclaredFailed(1), "node 1 belief cleared");
    check(d.getHeartbeatsSent(1) == 0, "node 1 counters cleared");
    check(d.isMonitored(1), "node 1 still monitored");
    check(d.getMissedCount(2) == 1, "node 2 untouched by resetNode(1)");
    check(!net.isNodeActive(1), "reset does not change the Network");
    check(d.getEventCount() == 1, "reset does not erase the event log");

    check(d.resetAll() == 2, "resetAll resets 2 nodes");
    check(d.getMissedCount(2) == 0, "node 2 cleared by resetAll");
    check(d.getDeclaredFailedCount() == 0, "no beliefs left");
    check(d.getMonitoredCount() == 2, "both still monitored");

    d.clearEventLog();
    check(d.getEventCount() == 0, "event log cleared");

    d.clearMonitoring();
    check(d.getMonitoredCount() == 0, "clearMonitoring: nothing monitored");
    check(!d.isMonitored(1), "node 1 not monitored after clear");
    check(d.sendHeartbeat(1) == HeartbeatEffect::NOT_MONITORED,
          "heartbeat after clear ignored");
    check(d.registerNode(1) == RegisterResult::REGISTERED, "can register again");
    check(d.runHeartbeatRound() == 0, "round with healthy node: no events");

    // Empty detector is safe
    FailureDetector empty(net);
    check(empty.runHeartbeatRound() == 0, "round with no monitored nodes");
    check(empty.checkAllTimeouts() == 0, "timeouts with no monitored nodes");
    check(empty.resetAll() == 0, "resetAll with no monitored nodes");
}

// ------------------------------------------------------------
void testSharedNetwork() {
    section("Detector shares the Network by reference");
    Network net;
    buildNetwork(net);
    FailureDetector d(net, 1);
    d.registerNode(4);
    d.recordHeartbeatMiss(4);
    check(!net.isNodeActive(4), "caller's Network object sees the detector's change");
    check(net.getFailedNodeCount() == 1, "failed node count is 1");
    net.restoreNode(4);
    check(d.sendHeartbeat(4) == HeartbeatEffect::RECOVERY_DETECTED,
          "detector sees change made directly on the Network");
}

// ------------------------------------------------------------
void testDisplayAndStrings() {
    section("Display and string helpers");
    Network net;
    buildNetwork(net);
    FailureDetector d(net, 2);
    d.registerNode(1);
    d.registerNode(2);
    d.recordHeartbeatMiss(2);
    d.recordHeartbeatMiss(2);

    check(registerResultToString(RegisterResult::REGISTERED) == "REGISTERED", "RegisterResult string");
    check(registerResultToString(RegisterResult::NOT_FOUND) == "NOT_FOUND", "RegisterResult NOT_FOUND string");
    check(heartbeatEffectToString(HeartbeatEffect::FAILURE_DETECTED) == "FAILURE_DETECTED", "effect string");
    check(heartbeatEffectToString(HeartbeatEffect::NOT_MONITORED) == "NOT_MONITORED", "effect NOT_MONITORED string");
    check(detectionEventTypeToString(DetectionEventType::RECOVERY_DETECTED) == "RECOVERY_DETECTED", "event type string");
    check(d.getEventLog()[0].description.find("Node 2") != std::string::npos,
          "event description names the node");

    std::cout << "  (sample display output below)";
    d.displayStatus();
    d.displayEventLog();
    check(true, "display functions run without crashing");
}

// ------------------------------------------------------------
int main() {
    std::cout << "=== FailureDetector tests ===\n";

    testDefaultsAndConfig();
    testRegistration();
    testSuccessfulHeartbeats();
    testMissedHeartbeats();
    testExactThreshold();
    testNoPrematureAndConsecutive();
    testRecovery();
    testRepeatedCycles();
    testMultipleNodes();
    testInvalidIds();
    testSimulatorCompatibility();
    testLinkStateSafety();
    testThresholdChecks();
    testSendHeartbeatUsesNetworkState();
    testResetAndClear();
    testSharedNetwork();
    testDisplayAndStrings();

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Passed: " << passedCount << '\n';
    std::cout << "Failed: " << failedCount << '\n';
    if (failedCount == 0)
        std::cout << "ALL TESTS PASSED\n";
    else
        std::cout << "SOME TESTS FAILED\n";

    return (failedCount == 0) ? 0 : 1;
}