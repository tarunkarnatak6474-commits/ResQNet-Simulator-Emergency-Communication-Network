
#ifndef RESQNET_ROUTING_H
#define RESQNET_ROUTING_H

#include "NetworkModel.h"

#include <vector>
#include <map>
#include <queue>
#include <limits>
#include <functional>
#include <algorithm>

// Stores the result of a shortest-path calculation.
struct RouteResult {
    bool found;
    std::vector<int> path;
    double totalCost;

    RouteResult() {
        found = false;
        totalCost = 0.0;
    }
};

class Routing {
private:
    Network& network;

    std::vector<int> currentRoute;
    double currentRouteCost;

    bool adaptiveMode;
    int routeChanges;

    // Compare two routes to see whether their paths differ.
    bool samePath(const std::vector<int>& a,
                  const std::vector<int>& b) const {
        return a == b;
    }

public:
    // The Routing module shares the existing Network object.
    Routing(Network& net) : network(net) {
        currentRouteCost = 0.0;
        adaptiveMode = false;
        routeChanges = 0;
    }

    // =====================================================
    // DIJKSTRA'S SHORTEST PATH ALGORITHM
    // =====================================================

    RouteResult findShortestPath(int source, int destination) const {
        RouteResult result;

        // Both nodes must exist and be active.
        if (!network.hasNode(source) ||
            !network.hasNode(destination)) {
            return result;
        }

        if (!network.isNodeActive(source) ||
            !network.isNodeActive(destination)) {
            return result;
        }

        // Distance stores the minimum known cost to each node.
        std::map<int, double> distance;

        // Previous stores the parent of each node in the path.
        std::map<int, int> previous;

        // Min-priority queue: smallest distance comes first.
        typedef std::pair<double, int> QueueItem;

        std::priority_queue<
            QueueItem,
            std::vector<QueueItem>,
            std::greater<QueueItem>
        > pq;

        const double INF = std::numeric_limits<double>::infinity();

        // Initially, every node has infinite distance.
        const std::vector<Node>& nodes = network.getNodes();

        for (size_t i = 0; i < nodes.size(); i++) {
            distance[nodes[i].id] = INF;
        }

        // Distance from source to itself is zero.
        distance[source] = 0.0;
        pq.push(std::make_pair(0.0, source));

        while (!pq.empty()) {
            double currentDistance = pq.top().first;
            int currentNode = pq.top().second;

            pq.pop();

            // Ignore an outdated queue entry.
            if (currentDistance > distance[currentNode]) {
                continue;
            }

            // Destination reached with minimum cost.
            if (currentNode == destination) {
                break;
            }

            // Get only currently usable neighbours.
            std::vector<Neighbor> neighbors =
                network.getNeighbors(currentNode);

            for (size_t i = 0; i < neighbors.size(); i++) {
                int nextNode = neighbors[i].nodeId;
                double edgeCost = neighbors[i].cost;

                double newDistance =
                    currentDistance + edgeCost;

                // Is this path cheaper?
                if (newDistance < distance[nextNode]) {
                    distance[nextNode] = newDistance;
                    previous[nextNode] = currentNode;

                    pq.push(std::make_pair(
                        newDistance, nextNode
                    ));
                }
            }
        }

        // No route exists.
        if (distance[destination] == INF) {
            return result;
        }

        // Reconstruct the path from destination to source.
        std::vector<int> reversedPath;

        int current = destination;
        reversedPath.push_back(current);

        while (current != source) {
            if (previous.find(current) == previous.end()) {
                return result;
            }

            current = previous[current];
            reversedPath.push_back(current);
        }

        // Reverse it to get source -> destination.
        std::reverse(reversedPath.begin(), reversedPath.end());

        result.found = true;
        result.path = reversedPath;
        result.totalCost = distance[destination];

        return result;
    }

    // =====================================================
    // STATIC ROUTING
    // =====================================================

    RouteResult setStaticRoute(int source, int destination) {
        adaptiveMode = false;

        RouteResult result =
            findShortestPath(source, destination);

        if (result.found) {
            if (!samePath(currentRoute, result.path)) {
                if (!currentRoute.empty()) {
                    routeChanges++;
                }
            }

            currentRoute = result.path;
            currentRouteCost = result.totalCost;
        }

        return result;
    }

    // =====================================================
    // ADAPTIVE ROUTING
    // =====================================================

    RouteResult setAdaptiveRoute(int source, int destination) {
        adaptiveMode = true;

        RouteResult result =
            findShortestPath(source, destination);

        if (result.found) {
            if (!samePath(currentRoute, result.path)) {
                if (!currentRoute.empty()) {
                    routeChanges++;
                }
            }

            currentRoute = result.path;
            currentRouteCost = result.totalCost;
        }

        return result;
    }

    // Recalculate the route after a network change.
    // This works only when adaptive mode is enabled.
    RouteResult updateRoute(int source, int destination) {
        if (!adaptiveMode) {
            RouteResult result;
            result.found = !currentRoute.empty();
            result.path = currentRoute;
            result.totalCost = currentRouteCost;
            return result;
        }

        RouteResult result =
            findShortestPath(source, destination);

        if (result.found) {
            if (!samePath(currentRoute, result.path)) {
                if (!currentRoute.empty()) {
                    routeChanges++;
                }
            }

            currentRoute = result.path;
            currentRouteCost = result.totalCost;
        } else {
            if (!currentRoute.empty()) {
                routeChanges++;
            }

            currentRoute.clear();
            currentRouteCost = 0.0;
        }

        return result;
    }

    // =====================================================
    // ROUTE INFORMATION
    // =====================================================

    std::vector<int> getCurrentRoute() const {
        return currentRoute;
    }

    double getRouteCost() const {
        return currentRouteCost;
    }

    int getRouteChanges() const {
        return routeChanges;
    }

    bool isAdaptiveMode() const {
        return adaptiveMode;
    }

    bool isCurrentRouteUsable() const {
        if (currentRoute.empty()) {
            return false;
        }

        // Every node in the route must be active.
        for (size_t i = 0; i < currentRoute.size(); i++) {
            if (!network.isNodeActive(currentRoute[i])) {
                return false;
            }
        }

        // Every connection in the route must be usable.
        for (size_t i = 0; i + 1 < currentRoute.size(); i++) {
            if (!network.isLinkUsable(
                    currentRoute[i],
                    currentRoute[i + 1])) {
                return false;
            }
        }

        return true;
    }

    // Clear the currently stored route and its cost.
    void clearRoute() {
        currentRoute.clear();
        currentRouteCost = 0.0;
    }

    // Reset the route-change counter.
    void resetRouteChanges() {
        routeChanges = 0;
    }
};

#endif
