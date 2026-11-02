// NetworkModelTest.cpp - tests for NetworkModel.h
//
// Test network used in most tests (A=0, B=1, C=2, D=3):
//
//   A -- B -- C
//   A -- D -- C
//
// Links: A-B, B-C, A-D, D-C (all cost 1, except A-D cost 2)

#include <iostream>
#include <string>
#include "NetworkModel.h"

const int A = 0;
const int B = 1;
const int C = 2;
const int D = 3;

int passedCount = 0;
int failedCount = 0;

// Prints PASS/FAIL for one check and keeps the totals
void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "  [PASS] " << description << '\n';
        passedCount++;
    } else {
        std::cout << "  [FAIL] " << description << '\n';
        failedCount++;
    }
}

// Builds the A-B-C / A-D-C network
void buildSampleNetwork(Network& net) {
    net.addNode(A);
    net.addNode(B);
    net.addNode(C);
    net.addNode(D);

    net.addLink(A, B, 1);
    net.addLink(B, C, 1);
    net.addLink(A, D, 2);
    net.addLink(D, C, 1);
}

// Test 1: empty network
void testEmptyNetwork() {
    std::cout << "\nTest 1: Creating an empty network\n";
    Network net;

    check(net.getTotalNodes() == 0, "total nodes is 0");
    check(net.getTotalLinks() == 0, "total links is 0");
    check(net.getNodes().empty(), "getNodes() is empty");
    check(net.getLinks().empty(), "getLinks() is empty");
    check(!net.hasNode(A), "node A does not exist");
    check(net.getNeighbors(A).empty(), "neighbours of missing node is empty");
    net.displayNetwork();
}

// Test 2: adding nodes
void testAddNodes() {
    std::cout << "\nTest 2: Adding multiple nodes\n";
    Network net;

    check(net.addNode(A), "add node A");
    check(net.addNode(B), "add node B");
    check(net.addNode(C), "add node C");
    check(net.addNode(D), "add node D");
    check(net.getTotalNodes() == 4, "total nodes is 4");
    check(!net.addNode(A), "duplicate node A is rejected");
    check(!net.addNode(-5), "negative node id is rejected");
    check(net.getTotalNodes() == 4, "total nodes still 4");
    check(net.getNodes()[0].id == A, "first stored node is A");
}

// Test 3: adding links
void testAddLinks() {
    std::cout << "\nTest 3: Adding links between nodes\n";
    Network net;
    buildSampleNetwork(net);

    check(net.getTotalLinks() == 4, "total links is 4");
    check(net.hasLink(A, B), "link A-B exists");
    check(net.hasLink(B, A), "link B-A is the same link (undirected)");
    check(!net.hasLink(A, C), "link A-C does not exist");
    check(net.getNeighbors(A).size() == 2, "A has 2 neighbours (B and D)");
    check(net.getNeighbors(C).size() == 2, "C has 2 neighbours (B and D)");
    check(net.getLinks()[2].cost == 2, "link A-D has cost 2");
}

// Test 4: display
void testDisplay() {
    std::cout << "\nTest 4: Displaying the network\n";
    Network net;
    buildSampleNetwork(net);
    net.displayNetwork();
    check(true, "displayNetwork() ran (check output above)");
}

// Test 5: fail node
void testFailNode() {
    std::cout << "\nTest 5: Failing a node (B)\n";
    Network net;
    buildSampleNetwork(net);

    check(net.failNode(B), "failNode(B) succeeds");
    check(!net.isNodeActive(B), "B is FAILED");
    check(net.isNodeActive(A) && net.isNodeActive(C) && net.isNodeActive(D),
          "A, C, D stay ACTIVE");
    check(net.isLinkActive(A, B) && net.isLinkActive(B, C),
          "stored status of A-B and B-C is still ACTIVE");
    check(!net.isLinkUsable(A, B) && !net.isLinkUsable(B, C),
          "A-B and B-C are not usable");
    check(net.isLinkUsable(A, D) && net.isLinkUsable(D, C),
          "A-D and D-C remain usable (alternative path A-D-C)");
    check(net.getActiveLinkCount() == 2, "effective active links = 2");
    check(net.getFailedLinkCount() == 2, "effective failed links = 2");
    check(net.getStoredFailedLinkCount() == 0, "stored failed links = 0");
    check(net.getNodeBlockedLinkCount() == 2, "links blocked by node = 2");
    check(net.getFailedNodeCount() == 1, "failed nodes = 1");
    check(net.getNeighbors(A).size() == 1 && net.getNeighbors(A)[0].nodeId == D,
          "A's only usable neighbour is D");
    check(net.getNeighbors(B).empty(), "failed node B has no neighbours");
    check(!net.failNode(99), "failing a missing node returns false");
    net.displayNetwork();
}

// Test 6: restore node
void testRestoreNode() {
    std::cout << "\nTest 6: Restoring a node (B)\n";
    Network net;
    buildSampleNetwork(net);

    net.failNode(B);
    check(net.restoreNode(B), "restoreNode(B) succeeds");
    check(net.isNodeActive(B), "B is ACTIVE again");
    check(net.isLinkUsable(A, B) && net.isLinkUsable(B, C),
          "A-B and B-C usable again");
    check(net.getActiveLinkCount() == 4, "effective active links = 4");
    check(net.getFailedNodeCount() == 0, "failed nodes = 0");
    check(!net.restoreNode(99), "restoring a missing node returns false");
}

// Test 7: fail link independently
void testFailLink() {
    std::cout << "\nTest 7: Failing a link independently (A-B)\n";
    Network net;
    buildSampleNetwork(net);

    check(net.failLink(A, B), "failLink(A,B) succeeds");
    check(net.isNodeActive(A) && net.isNodeActive(B), "A and B stay ACTIVE");
    check(!net.isLinkActive(A, B), "stored status of A-B is FAILED");
    check(!net.isLinkUsable(A, B), "A-B is not usable");
    check(net.isLinkUsable(B, C), "B-C is still usable");
    check(net.getStoredFailedLinkCount() == 1, "stored failed links = 1");
    check(net.getFailedLinkCount() == 1, "effective failed links = 1");
    check(net.getNodeBlockedLinkCount() == 0, "links blocked by node = 0");
    check(net.getFailedNodeCount() == 0, "failed nodes = 0");
    check(net.getNeighbors(A).size() == 1 && net.getNeighbors(A)[0].nodeId == D,
          "A's only usable neighbour is D");
    check(net.getNeighbors(B).size() == 1 && net.getNeighbors(B)[0].nodeId == C,
          "B's only usable neighbour is C");
    check(!net.failLink(A, C), "failing a missing link returns false");
}

// Test 8: restore link
void testRestoreLink() {
    std::cout << "\nTest 8: Restoring a link (A-B)\n";
    Network net;
    buildSampleNetwork(net);

    net.failLink(A, B);
    check(net.restoreLink(B, A), "restoreLink(B,A) succeeds (either order)");
    check(net.isLinkActive(A, B), "stored status of A-B is ACTIVE");
    check(net.isLinkUsable(A, B), "A-B is usable again");
    check(net.getActiveLinkCount() == 4, "effective active links = 4");
    check(!net.restoreLink(A, C), "restoring a missing link returns false");
}

// Test 9: invalid links
void testInvalidLinks() {
    std::cout << "\nTest 9: Preventing invalid links\n";
    Network net;
    buildSampleNetwork(net);

    check(!net.addLink(A, 99, 1), "link to missing node is rejected");
    check(!net.addLink(99, A, 1), "link from missing node is rejected");
    check(!net.addLink(A, A, 1), "self-loop is rejected");
    check(!net.addLink(A, B, 5), "duplicate link A-B is rejected");
    check(!net.addLink(B, A, 5), "duplicate link B-A is rejected");
    check(!net.addLink(A, C, 0), "cost 0 is rejected");
    check(!net.addLink(A, C, -3), "negative cost is rejected");
    check(net.getTotalLinks() == 4, "total links is still 4");
    check(net.addLink(A, C, 3), "valid new link A-C is accepted");
    check(net.getTotalLinks() == 5, "total links is now 5");
}

// Test 10: status of nodes and links
void testStatusChecks() {
    std::cout << "\nTest 10: Checking status of nodes and links\n";
    Network net;
    buildSampleNetwork(net);

    net.failNode(C);
    net.failLink(A, D);

    check(net.isNodeActive(A), "A is ACTIVE");
    check(net.isNodeActive(B), "B is ACTIVE");
    check(!net.isNodeActive(C), "C is FAILED");
    check(net.isNodeActive(D), "D is ACTIVE");
    check(!net.isNodeActive(99), "missing node is reported not active");

    check(net.isLinkUsable(A, B), "A-B usable");
    check(!net.isLinkUsable(B, C), "B-C not usable (C failed)");
    check(!net.isLinkUsable(A, D), "A-D not usable (link failed)");
    check(!net.isLinkUsable(D, C), "D-C not usable (C failed)");
    check(!net.isLinkUsable(A, C), "missing link is not usable");

    check(net.isLinkActive(B, C), "B-C stored ACTIVE");
    check(!net.isLinkActive(A, D), "A-D stored FAILED");
}

// Test 11: links of a failed node cannot be used
void testFailedNodeLinksUnusable() {
    std::cout << "\nTest 11: Links of a failed node cannot be used\n";
    Network net;
    buildSampleNetwork(net);

    net.failNode(D);

    check(!net.isLinkUsable(A, D), "A-D unusable");
    check(!net.isLinkUsable(D, C), "D-C unusable");
    check(net.isLinkActive(A, D) && net.isLinkActive(D, C),
          "A-D and D-C stored status still ACTIVE");
    check(net.getNeighbors(D).empty(), "D has no usable neighbours");

    std::vector<Neighbor> nearA = net.getNeighbors(A);
    bool dFound = false;
    for (size_t i = 0; i < nearA.size(); i++) {
        if (nearA[i].nodeId == D)
            dFound = true;
    }
    check(!dFound, "failed node D is not a neighbour of A");
    check(nearA.size() == 1 && nearA[0].nodeId == B && nearA[0].cost == 1,
          "A's only neighbour is B with cost 1");
}

// Test 12: restoring a node must not restore independently failed links
void testRestoreNodeKeepsFailedLinks() {
    std::cout << "\nTest 12: Restoring a node keeps independently failed links failed\n";
    Network net;
    buildSampleNetwork(net);

    net.failLink(A, B);   // independent link failure
    net.failNode(B);      // node failure on top of it

    check(net.getStoredFailedLinkCount() == 1, "stored failed links = 1 (A-B)");
    check(net.getNodeBlockedLinkCount() == 1, "blocked by node = 1 (B-C)");
    check(net.getFailedLinkCount() == 2, "effective failed links = 2");

    net.restoreNode(B);

    check(net.isNodeActive(B), "B is ACTIVE after restore");
    check(!net.isLinkActive(A, B), "A-B stored status is still FAILED");
    check(!net.isLinkUsable(A, B), "A-B is still NOT usable");
    check(net.isLinkUsable(B, C), "B-C is usable again");
    check(net.getStoredFailedLinkCount() == 1, "stored failed links still 1");
    check(net.getNodeBlockedLinkCount() == 0, "blocked by node = 0");
    check(net.getFailedLinkCount() == 1, "effective failed links = 1");

    net.restoreLink(A, B);
    check(net.isLinkUsable(A, B), "A-B usable after restoreLink");
    check(net.getFailedLinkCount() == 0, "effective failed links = 0");
}

// Test 13: count consistency in a mixed failure state
void testCountConsistency() {
    std::cout << "\nTest 13: Count consistency (node + link failures together)\n";
    Network net;
    buildSampleNetwork(net);

    net.failNode(B);
    net.failLink(A, D);

    check(net.getTotalLinks() == 4, "total links = 4");
    check(net.getActiveLinkCount() + net.getFailedLinkCount() ==
          net.getTotalLinks(), "usable + unusable = total");
    check(net.getStoredActiveLinkCount() + net.getStoredFailedLinkCount() ==
          net.getTotalLinks(), "stored active + stored failed = total");
    check(net.getFailedLinkCount() ==
          net.getStoredFailedLinkCount() + net.getNodeBlockedLinkCount(),
          "unusable = stored failed + blocked by node");
    check(net.getActiveLinkCount() == 1, "usable links = 1 (D-C)");
    check(net.getStoredFailedLinkCount() == 1, "stored failed = 1 (A-D)");
    check(net.getNodeBlockedLinkCount() == 2, "blocked by node = 2 (A-B, B-C)");
    check(net.getActiveNodeCount() + net.getFailedNodeCount() ==
          net.getTotalNodes(), "active nodes + failed nodes = total nodes");
}

int main() {
    std::cout << "===== NetworkModel Tests =====\n";

    testEmptyNetwork();
    testAddNodes();
    testAddLinks();
    testDisplay();
    testFailNode();
    testRestoreNode();
    testFailLink();
    testRestoreLink();
    testInvalidLinks();
    testStatusChecks();
    testFailedNodeLinksUnusable();
    testRestoreNodeKeepsFailedLinks();
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
