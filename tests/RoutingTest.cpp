
#include <iostream>
#include <cassert>
#include "../include/Routing.h"

using namespace std;

void buildNetwork(Network& net) {
    net.addNode(1);
    net.addNode(2);
    net.addNode(3);
    net.addNode(4);

    // Two possible routes:
    //
    // 1 --1-- 2 --1-- 4
    //  \               /
    //   2             2
    //    \           /
    //       3

    net.addLink(1, 2, 1);
    net.addLink(2, 4, 1);
    net.addLink(1, 3, 2);
    net.addLink(3, 4, 2);
}

void testShortestPath() {
    Network net;
    buildNetwork(net);

    Routing routing(net);

    RouteResult result = routing.findShortestPath(1, 4);

    assert(result.found == true);
    assert(result.totalCost == 2);
    assert(result.path.size() == 3);
    assert(result.path[0] == 1);
    assert(result.path[1] == 2);
    assert(result.path[2] == 4);

    cout << "Shortest path test: PASS\n";
}

void testFailedNodeAvoidance() {
    Network net;
    buildNetwork(net);

    net.failNode(2);

    Routing routing(net);

    RouteResult result = routing.findShortestPath(1, 4);

    assert(result.found == true);
    assert(result.totalCost == 4);
    assert(result.path[0] == 1);
    assert(result.path[1] == 3);
    assert(result.path[2] == 4);

    cout << "Failed node avoidance test: PASS\n";
}

void testFailedLinkAvoidance() {
    Network net;
    buildNetwork(net);

    net.failLink(1, 2);

    Routing routing(net);

    RouteResult result = routing.findShortestPath(1, 4);

    assert(result.found == true);
    assert(result.path[1] == 3);

    cout << "Failed link avoidance test: PASS\n";
}

void testStaticRouting() {
    Network net;
    buildNetwork(net);

    Routing routing(net);

    RouteResult initial = routing.setStaticRoute(1, 4);
    assert(initial.found == true);

    net.failNode(2);

    // Static routing does not automatically recalculate.
    RouteResult afterFailure = routing.updateRoute(1, 4);

    assert(afterFailure.found == true);
    assert(afterFailure.path[1] == 2);
    assert(routing.isCurrentRouteUsable() == false);

    cout << "Static routing test: PASS\n";
}

void testAdaptiveRouting() {
    Network net;
    buildNetwork(net);

    Routing routing(net);

    RouteResult initial = routing.setAdaptiveRoute(1, 4);
    assert(initial.found == true);
    assert(initial.path[1] == 2);

    net.failNode(2);

    RouteResult updated = routing.updateRoute(1, 4);

    assert(updated.found == true);
    assert(updated.path[1] == 3);
    assert(routing.isCurrentRouteUsable() == true);
    assert(routing.getRouteChanges() == 1);

    cout << "Adaptive routing test: PASS\n";
}

void testNoAvailableRoute() {
    Network net;

    net.addNode(1);
    net.addNode(2);
    net.addLink(1, 2, 5);

    net.failLink(1, 2);

    Routing routing(net);

    RouteResult result = routing.findShortestPath(1, 2);

    assert(result.found == false);
    assert(result.path.empty());

    cout << "No available route test: PASS\n";
}

void testInvalidNodes() {
    Network net;
    buildNetwork(net);

    Routing routing(net);

    RouteResult result = routing.findShortestPath(1, 99);

    assert(result.found == false);

    cout << "Invalid node test: PASS\n";
}

void testSameSourceAndDestination() {
    Network net;
    buildNetwork(net);

    Routing routing(net);

    RouteResult result = routing.findShortestPath(1, 1);

    assert(result.found == true);
    assert(result.path.size() == 1);
    assert(result.path[0] == 1);
    assert(result.totalCost == 0);

    cout << "Same source and destination test: PASS\n";
}

int main() {
    testShortestPath();
    testFailedNodeAvoidance();
    testFailedLinkAvoidance();
    testStaticRouting();
    testAdaptiveRouting();
    testNoAvailableRoute();
    testInvalidNodes();
    testSameSourceAndDestination();

    cout << "\nAll Routing tests passed!\n";

    return 0;
}
