// FailureSimulatorTest.cpp - tests for FailureSimulator.h
//
// Network 1 ("sample"), used for manual tests and scenarios:
//   A -- B -- C        A=0, B=1, C=2, D=3
//   A -- D -- C        links: A-B, B-C, A-D, D-C
//
// Network 2 ("ring"), used for random tests:
//   6 nodes (0..5), 8 links:
//   0-1, 1-2, 2-3, 3-4, 4-5, 5-0, 0-3, 1-4
//
// All random tests use fixed seeds. The random generator is
// written by hand in FailureSimulator.h, so the same seed gives
// the same results on every compiler and computer.

#include <iostream>
#include <string>
#include <vector>
#include "NetworkModel.h"
#include "FailureSimulator.h"

const int A = 0;
const int B = 1;
const int C = 2;
const int D = 3;

int passedCount = 0;
int failedCount = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "  [PASS] " << description << '\n';
        passedCount++;
    } else {
        std::cout << "  [FAIL] " << description << '\n';
        failedCount++;
    }
}

void buildSampleNetwork(Network& net) {
    for (int i = 0; i < 4; i++)
        net.addNode(i);
    net.addLink(A, B, 1);
    net.addLink(B, C, 1);
    net.addLink(A, D, 2);
    net.addLink(D, C, 1);
}

void buildRingNetwork(Network& net) {
    for (int i = 0; i < 6; i++)
        net.addNode(i);
    net.addLink(0, 1);
    net.addLink(1, 2);
    net.addLink(2, 3);
    net.addLink(3, 4);
    net.addLink(4, 5);
    net.addLink(5, 0);
    net.addLink(0, 3);
    net.addLink(1, 4);
}

// true if the list contains the given id more than once
bool hasDuplicateNodes(const std::vector<int>& ids) {
    for (size_t i = 0; i < ids.size(); i++)
        for (size_t j = i + 1; j < ids.size(); j++)
            if (ids[i] == ids[j])
                return true;
    return false;
}

// true if the same link appears twice (either direction)
bool hasDuplicateLinks(const std::vector<LinkId>& links) {
    for (size_t i = 0; i < links.size(); i++) {
        for (size_t j = i + 1; j < links.size(); j++) {
            bool same = (links[i].from == links[j].from &&
                         links[i].to == links[j].to);
            bool reverse = (links[i].from == links[j].to &&
                            links[i].to == links[j].from);
            if (same || reverse)
                return true;
        }
    }
    return false;
}

// Test 1: manual node failure and recovery
void testManualNode() {
    std::cout << "\nTest 1: Manual node failure and recovery\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net);

    check(sim.failNode(B) == ActionResult::SUCCESS, "failNode(B) returns SUCCESS");
    check(!net.isNodeActive(B), "B is FAILED in the network");
    check(!net.isLinkUsable(A, B) && !net.isLinkUsable(B, C),
          "A-B and B-C are unusable");
    check(net.isLinkUsable(A, D) && net.isLinkUsable(D, C),
          "A-D and D-C are still usable");
    check(net.getFailedNodeCount() == 1, "failed nodes = 1");
    check(net.getActiveLinkCount() == 2, "effective active links = 2");
    check(net.getFailedLinkCount() == 2, "effective failed links = 2");
    check(net.getStoredFailedLinkCount() == 0, "stored failed links = 0");

    check(sim.restoreNode(B) == ActionResult::SUCCESS, "restoreNode(B) returns SUCCESS");
    check(net.isNodeActive(B), "B is ACTIVE again");
    check(net.getActiveLinkCount() == 4, "effective active links = 4");
    check(net.getFailedNodeCount() == 0, "failed nodes = 0");
}

// Test 2: manual link failure and recovery
void testManualLink() {
    std::cout << "\nTest 2: Manual link failure and recovery\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net);

    check(sim.failLink(A, B) == ActionResult::SUCCESS, "failLink(A,B) returns SUCCESS");
    check(net.isNodeActive(A) && net.isNodeActive(B), "A and B stay ACTIVE");
    check(!net.isLinkActive(A, B), "stored status of A-B is FAILED");
    check(!net.isLinkUsable(A, B), "A-B is unusable");
    check(net.isLinkUsable(B, C), "B-C is still usable");
    check(net.getStoredFailedLinkCount() == 1, "stored failed links = 1");
    check(net.getFailedLinkCount() == 1, "effective failed links = 1");
    check(net.getNodeBlockedLinkCount() == 0, "links blocked by node = 0");

    check(sim.restoreLink(B, A) == ActionResult::SUCCESS,
          "restoreLink(B,A) returns SUCCESS (either order)");
    check(net.isLinkUsable(A, B), "A-B is usable again");
    check(net.getActiveLinkCount() == 4, "effective active links = 4");
}

// Test 3: invalid node and link IDs
void testInvalidIds() {
    std::cout << "\nTest 3: Invalid node and link IDs\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net);

    check(sim.failNode(99) == ActionResult::NOT_FOUND, "failNode(99) is NOT_FOUND");
    check(sim.restoreNode(99) == ActionResult::NOT_FOUND, "restoreNode(99) is NOT_FOUND");
    check(sim.failNode(-1) == ActionResult::NOT_FOUND, "failNode(-1) is NOT_FOUND");
    check(sim.failLink(A, C) == ActionResult::NOT_FOUND, "failLink(A,C) is NOT_FOUND (no such link)");
    check(sim.failLink(A, 99) == ActionResult::NOT_FOUND, "failLink(A,99) is NOT_FOUND");
    check(sim.restoreLink(A, C) == ActionResult::NOT_FOUND, "restoreLink(A,C) is NOT_FOUND");
    check(sim.restoreLink(99, 98) == ActionResult::NOT_FOUND, "restoreLink(99,98) is NOT_FOUND");
    check(sim.getEventCount() == 0, "invalid requests are not logged");
    check(net.getFailedNodeCount() == 0 && net.getFailedLinkCount() == 0,
          "network is unchanged");
}

// Test 4: repeated failure and recovery requests
void testRepeatedRequests() {
    std::cout << "\nTest 4: Repeated failure and recovery requests\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net);

    check(sim.restoreNode(B) == ActionResult::NO_CHANGE, "restoring an active node is NO_CHANGE");
    check(sim.restoreLink(A, B) == ActionResult::NO_CHANGE, "restoring an active link is NO_CHANGE");

    check(sim.failNode(B) == ActionResult::SUCCESS, "first failNode(B) is SUCCESS");
    check(sim.failNode(B) == ActionResult::NO_CHANGE, "second failNode(B) is NO_CHANGE");
    check(sim.failLink(A, D) == ActionResult::SUCCESS, "first failLink(A,D) is SUCCESS");
    check(sim.failLink(D, A) == ActionResult::NO_CHANGE, "second failLink(D,A) is NO_CHANGE");

    check(net.getFailedNodeCount() == 1, "failed nodes still 1");
    check(net.getStoredFailedLinkCount() == 1, "stored failed links still 1");
    check(sim.getEventCount() == 2, "only the 2 real changes are logged");

    check(sim.restoreNode(B) == ActionResult::SUCCESS, "first restoreNode(B) is SUCCESS");
    check(sim.restoreNode(B) == ActionResult::NO_CHANGE, "second restoreNode(B) is NO_CHANGE");
    check(sim.restoreLink(A, D) == ActionResult::SUCCESS, "first restoreLink(A,D) is SUCCESS");
    check(sim.restoreLink(A, D) == ActionResult::NO_CHANGE, "second restoreLink(A,D) is NO_CHANGE");
    check(sim.getEventCount() == 4, "4 real changes logged in total");
    check(net.getActiveLinkCount() == 4, "all links usable again");
}

// Test 5: node and link failures stay independent
void testIndependence() {
    std::cout << "\nTest 5: Node and link failures stay independent\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net);

    sim.failLink(A, B);
    sim.failNode(B);
    sim.restoreNode(B);

    check(net.isNodeActive(B), "B is ACTIVE after restore");
    check(!net.isLinkUsable(A, B), "A-B is still NOT usable (independent failure)");
    check(net.isLinkUsable(B, C), "B-C is usable again");

    // failing a link whose end node is already failed
    sim.failNode(C);
    check(sim.failLink(D, C) == ActionResult::SUCCESS,
          "failLink(D,C) works while C is failed");
    sim.restoreNode(C);
    check(!net.isLinkUsable(D, C), "D-C stays unusable after C is restored");
    check(sim.restoreLink(D, C) == ActionResult::SUCCESS, "restoreLink(D,C) is SUCCESS");
    check(net.isLinkUsable(D, C), "D-C usable after restoreLink");
}

// Test 6: random node failures with a fixed seed
void testRandomNodes() {
    std::cout << "\nTest 6: Random node failures (fixed seed)\n";
    Network net;
    buildRingNetwork(net);
    FailureSimulator sim(net, 1);

    std::vector<int> first = sim.failRandomNodes(2);
    check(first.size() == 2, "2 nodes were failed");
    check(!hasDuplicateNodes(first), "the 2 nodes are different");
    check(!net.isNodeActive(first[0]) && !net.isNodeActive(first[1]),
          "both chosen nodes are FAILED in the network");
    check(net.getFailedNodeCount() == 2, "failed nodes = 2");
    check(sim.getEventCount() == 2, "2 events logged");

    std::vector<int> second = sim.failRandomNodes(10);
    check(second.size() == 4, "asking for 10 fails only the 4 remaining active nodes");
    check(net.getFailedNodeCount() == 6, "failed nodes = 6");
    check(sim.getEventCount() == 6, "6 events logged");

    check(sim.failRandomNodes(3).empty(), "no active nodes left: nothing is failed");
    check(sim.failRandomNodes(0).empty(), "count 0 fails nothing");
    check(sim.failRandomNodes(-2).empty(), "negative count fails nothing");

    // already failed nodes must not be chosen
    Network net2;
    buildRingNetwork(net2);
    FailureSimulator sim2(net2, 99);
    sim2.failNode(0);
    std::vector<int> picked = sim2.failRandomNodes(5);
    bool zeroPicked = false;
    for (size_t i = 0; i < picked.size(); i++)
        if (picked[i] == 0)
            zeroPicked = true;
    check(picked.size() == 5, "5 more nodes failed");
    check(!zeroPicked, "already failed node 0 was not chosen again");
    check(sim2.getEventCount() == 6, "no duplicate events logged");
}

// Test 7: random link failures with a fixed seed
void testRandomLinks() {
    std::cout << "\nTest 7: Random link failures (fixed seed)\n";
    Network net;
    buildRingNetwork(net);
    FailureSimulator sim(net, 3);

    std::vector<LinkId> picked = sim.failRandomLinks(2);
    check(picked.size() == 2, "2 links were failed");
    check(!hasDuplicateLinks(picked), "the 2 links are different");
    check(!net.isLinkActive(picked[0].from, picked[0].to) &&
          !net.isLinkActive(picked[1].from, picked[1].to),
          "both chosen links are FAILED");
    check(net.getFailedNodeCount() == 0, "no node was failed");
    check(net.getStoredFailedLinkCount() == 2, "stored failed links = 2");
    check(net.getFailedLinkCount() == 2, "effective failed links = 2");
    check(net.getNodeBlockedLinkCount() == 0, "links blocked by node = 0");
    check(sim.getEventCount() == 2, "2 events logged");

    // links cut off by a failed node are not "usable", so not chosen
    Network net2;
    buildRingNetwork(net2);
    FailureSimulator sim2(net2, 5);
    sim2.failNode(0);                    // cuts links 0-1, 5-0, 0-3
    std::vector<LinkId> all = sim2.failRandomLinks(100);

    bool touchesNodeZero = false;
    for (size_t i = 0; i < all.size(); i++)
        if (all[i].from == 0 || all[i].to == 0)
            touchesNodeZero = true;

    check(all.size() == 5, "only the 5 usable links were failed");
    check(!touchesNodeZero, "links of failed node 0 were not chosen");
    check(net2.getStoredFailedLinkCount() == 5, "stored failed links = 5");
    check(net2.getNodeBlockedLinkCount() == 3, "links blocked by node = 3");
    check(net2.getActiveLinkCount() == 0, "no usable links left");
    check(net2.getFailedLinkCount() == 8, "effective failed links = 8");
    check(sim2.getEventCount() == 6, "6 events logged (1 node + 5 links)");
    check(sim2.failRandomLinks(1).empty(), "no usable links left: nothing is failed");
}

// Test 8: reproducibility with seeds
void testSeedReproducibility() {
    std::cout << "\nTest 8: Same seed gives same random failures\n";

    // two separate runs, same seed
    Network netX;
    Network netY;
    buildRingNetwork(netX);
    buildRingNetwork(netY);
    FailureSimulator simX(netX, 123);
    FailureSimulator simY(netY, 123);

    std::vector<int> nodesX = simX.failRandomNodes(3);
    std::vector<int> nodesY = simY.failRandomNodes(3);
    check(nodesX == nodesY, "same seed: same 3 failed nodes");

    std::vector<LinkId> linksX = simX.failRandomLinks(2);
    std::vector<LinkId> linksY = simY.failRandomLinks(2);
    bool sameLinks = (linksX.size() == linksY.size());
    for (size_t i = 0; sameLinks && i < linksX.size(); i++) {
        if (linksX[i].from != linksY[i].from || linksX[i].to != linksY[i].to)
            sameLinks = false;
    }
    check(sameLinks, "same seed: same failed links");

    // setSeed restarts the sequence
    Network net;
    buildRingNetwork(net);
    FailureSimulator sim(net, 1);
    sim.setSeed(5);
    std::vector<int> run1 = sim.failRandomNodes(3);
    sim.restoreAll();
    sim.setSeed(5);
    std::vector<int> run2 = sim.failRandomNodes(3);
    check(run1 == run2, "setSeed(5) again repeats the same failures");

    // fixed expected values for seed 42 (fresh network each time)
    Network netN;
    buildRingNetwork(netN);
    FailureSimulator simN(netN, 42);
    std::vector<int> n42 = simN.failRandomNodes(3);
    check(n42.size() == 3 && n42[0] == 3 && n42[1] == 1 && n42[2] == 2,
          "seed 42 fails nodes 3, 1, 2 in this order");

    Network netL;
    buildRingNetwork(netL);
    FailureSimulator simL(netL, 42);
    std::vector<LinkId> l42 = simL.failRandomLinks(3);
    check(l42.size() == 3 &&
          l42[0].from == 1 && l42[0].to == 2 &&
          l42[1].from == 4 && l42[1].to == 5 &&
          l42[2].from == 2 && l42[2].to == 3,
          "seed 42 fails links 1-2, 4-5, 2-3 in this order");
}

// Test 9: predefined scenario 1
void testScenarioOne() {
    std::cout << "\nTest 9: Predefined scenario 1 (node, link, restore node)\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net);

    Scenario s = createNodeAndLinkScenario(B, A, D);
    check(s.steps.size() == 3, "scenario has 3 steps");

    ScenarioResult r = sim.runScenario(s);
    check(r.totalSteps == 3 && r.changedSteps == 3 &&
          r.noChangeSteps == 0 && r.invalidSteps == 0,
          "3 steps run, all changed the network");
    check(net.isNodeActive(B), "B is ACTIVE at the end");
    check(!net.isLinkActive(A, D), "link A-D is FAILED at the end");
    check(net.isLinkUsable(A, B) && net.isLinkUsable(B, C) && net.isLinkUsable(D, C),
          "A-B, B-C and D-C are usable");
    check(net.getActiveLinkCount() == 3, "effective active links = 3");
    check(net.getFailedLinkCount() == 1, "effective failed links = 1");
    check(net.getStoredFailedLinkCount() == 1, "stored failed links = 1");
    check(sim.getEventCount() == 5, "5 log entries (start + 3 events + finish)");
}

// Test 10: predefined scenario 2
void testScenarioTwo() {
    std::cout << "\nTest 10: Predefined scenario 2 (two links, restore one)\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net);

    ScenarioResult r = sim.runScenario(createTwoLinkScenario(A, B, B, C));
    check(r.totalSteps == 3 && r.changedSteps == 3, "3 steps run, all changed the network");
    check(net.isLinkUsable(A, B), "A-B was restored");
    check(!net.isLinkUsable(B, C), "B-C is still failed");
    check(net.getStoredFailedLinkCount() == 1, "stored failed links = 1");
    check(net.getActiveLinkCount() == 3, "effective active links = 3");
    check(net.getFailedNodeCount() == 0, "no node failed");
}

// Test 11: custom scenario with invalid, repeated and random steps
void testCustomScenario() {
    std::cout << "\nTest 11: Custom scenario (invalid, repeated and random steps)\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net, 8);

    Scenario s("Custom");
    s.addFailNode(99);               // invalid
    s.addFailNode(B);                // changes
    s.addFailNode(B);                // repeated: no change
    s.addRandomLinkFailures(10);     // fails the 2 usable links (A-D, D-C)
    s.addRandomNodeFailures(10);     // fails A, C, D
    s.addRandomNodeFailures(1);      // nothing left: no change

    ScenarioResult r = sim.runScenario(s);
    check(r.totalSteps == 6, "6 steps run");
    check(r.changedSteps == 3, "3 steps changed the network");
    check(r.noChangeSteps == 2, "2 steps had no effect");
    check(r.invalidSteps == 1, "1 step was invalid");
    check(net.getFailedNodeCount() == 4, "all 4 nodes failed");
    check(net.getStoredFailedLinkCount() == 2, "2 links failed on their own (A-D, D-C)");
    check(net.getNodeBlockedLinkCount() == 2, "2 links blocked by failed nodes (A-B, B-C)");
    check(net.getActiveLinkCount() == 0, "no usable links");

    // restoreAll brings everything back
    int restored = sim.restoreAll();
    check(restored == 6, "restoreAll restored 4 nodes + 2 links");
    check(net.getFailedNodeCount() == 0 && net.getFailedLinkCount() == 0,
          "network is fully restored");
    check(sim.restoreAll() == 0, "second restoreAll restores nothing");
}

// Test 12: event log
void testEventLog() {
    std::cout << "\nTest 12: Event logging\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net);

    check(sim.getEventLog().empty(), "log starts empty");

    sim.failNode(B);
    sim.failLink(A, D);
    sim.restoreNode(B);
    sim.restoreLink(A, D);

    const std::vector<FailureEvent>& log = sim.getEventLog();
    check(log.size() == 4, "4 events logged");
    check(log[0].number == 1 && log[3].number == 4, "events are numbered 1 to 4");
    check(log[0].type == EventType::NODE_FAILED && log[0].nodeId == B,
          "event 1: NODE_FAILED for node B");
    check(log[1].type == EventType::LINK_FAILED && log[1].linkFrom == A && log[1].linkTo == D,
          "event 2: LINK_FAILED for link A-D");
    check(log[2].type == EventType::NODE_RESTORED, "event 3: NODE_RESTORED");
    check(log[3].type == EventType::LINK_RESTORED, "event 4: LINK_RESTORED");
    check(log[0].description == "Node 1 failed", "description 1 is \"Node 1 failed\"");
    check(log[1].description == "Link 0-3 failed", "description 2 is \"Link 0-3 failed\"");
    check(log[2].description == "Node 1 restored", "description 3 is \"Node 1 restored\"");
    check(log[3].description == "Link 0-3 restored", "description 4 is \"Link 0-3 restored\"");
    check(eventTypeToString(log[0].type) == "NODE_FAILED", "eventTypeToString works");

    sim.displayEventLog();

    sim.clearEventLog();
    check(sim.getEventCount() == 0, "clearEventLog empties the log");
    sim.failNode(C);
    check(sim.getEventLog()[0].number == 1, "numbering restarts at 1 after clearing");
}

// Test 13: counts stay consistent after mixed failures
void testCountConsistency() {
    std::cout << "\nTest 13: NetworkModel counts after mixed failures\n";
    Network net;
    buildSampleNetwork(net);
    FailureSimulator sim(net);

    sim.failNode(B);
    sim.failLink(A, D);

    check(net.getActiveLinkCount() == 1, "usable links = 1 (D-C)");
    check(net.getFailedLinkCount() == 3, "unusable links = 3");
    check(net.getStoredFailedLinkCount() == 1, "stored failed = 1 (A-D)");
    check(net.getNodeBlockedLinkCount() == 2, "blocked by node = 2 (A-B, B-C)");
    check(net.getActiveLinkCount() + net.getFailedLinkCount() == net.getTotalLinks(),
          "usable + unusable = total");
    check(net.getFailedLinkCount() ==
          net.getStoredFailedLinkCount() + net.getNodeBlockedLinkCount(),
          "unusable = stored failed + blocked by node");
    check(net.getNeighbors(D).size() == 1 && net.getNeighbors(D)[0].nodeId == C,
          "getNeighbors(D) returns only C");
    check(net.getNeighbors(A).empty(), "A has no usable neighbours");

    sim.restoreNode(B);
    check(net.getActiveLinkCount() == 3, "after restoring B: usable links = 3");
    check(net.getNodeBlockedLinkCount() == 0, "after restoring B: blocked = 0");
    check(net.getStoredFailedLinkCount() == 1, "A-D is still failed");
}

int main() {
    std::cout << "===== FailureSimulator Tests =====\n";

    testManualNode();
    testManualLink();
    testInvalidIds();
    testRepeatedRequests();
    testIndependence();
    testRandomNodes();
    testRandomLinks();
    testSeedReproducibility();
    testScenarioOne();
    testScenarioTwo();
    testCustomScenario();
    testEventLog();
    testCountConsistency();

    std::cout << "\n===== SUMMARY =====\n";
    std::cout << "Passed: " << passedCount << '\n';
    std::cout << "Failed: " << failedCount << '\n';

    if (failedCount == 0)
        std::cout << "ALL TESTS PASSED\n";
    else
        std::cout << "SOME TESTS FAILED\n";

    return (failedCount == 0) ? 0 : 1;
}
