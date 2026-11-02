#ifndef RESQNET_NETWORK_MODEL_H
#define RESQNET_NETWORK_MODEL_H

// ============================================================
// NetworkModel.h  -  ResQNet Simulator, Module 1
//
// Holds the real network (nodes + links) and its failure state.
// It does NOT do routing, random failures, failure detection or
// metrics. Those are separate modules that USE this one.
//
// KEY IDEA - two kinds of link status:
//
//   STORED status    : what was explicitly set on the link itself
//                      (failLink / restoreLink change this).
//   EFFECTIVE status : "can this link carry traffic right now?"
//                      A link is USABLE only if
//                        - the link's stored status is ACTIVE, and
//                        - BOTH of its end nodes are ACTIVE.
//
// Failing a node never changes any link's stored status, so
// restoring the node can never wrongly restore a link that
// failed on its own.
//
// COUNT RULES (always true for a valid network):
//   getTotalLinks()
//       = getActiveLinkCount() + getFailedLinkCount()        (effective)
//       = getStoredActiveLinkCount() + getStoredFailedLinkCount() (stored)
//   getFailedLinkCount()
//       = getStoredFailedLinkCount() + getNodeBlockedLinkCount()
// ============================================================

#include <iostream>
#include <string>
#include <vector>

// Status shared by nodes and links
enum class ElementStatus { ACTIVE, FAILED };

inline std::string statusToString(ElementStatus s) {
    return (s == ElementStatus::ACTIVE) ? "ACTIVE" : "FAILED";
}

// ------------------------------------------------------------
// Node: one device in the network.
// Add future fields here (name, x, y, type ...) without
// affecting the rest of the code.
// ------------------------------------------------------------
struct Node {
    int id;
    ElementStatus status;

    Node(int nodeId = 0) {
        id = nodeId;
        status = ElementStatus::ACTIVE;
    }
};

// ------------------------------------------------------------
// Link: a two-way connection between two nodes.
// 'status' is the STORED status of the link only.
// Whether the link is actually usable also depends on its
// end nodes (see Network::isLinkUsable).
// ------------------------------------------------------------
struct Link {
    int from;
    int to;
    double cost;
    ElementStatus status;

    Link(int a = 0, int b = 0, double linkCost = 1.0) {
        from = a;
        to = b;
        cost = linkCost;
        status = ElementStatus::ACTIVE;
    }
};

// ------------------------------------------------------------
// Neighbor: one usable connection seen from a node.
// Returned by Network::getNeighbors() for the Routing module.
// ------------------------------------------------------------
struct Neighbor {
    int nodeId;
    double cost;
};

// ------------------------------------------------------------
// Network: owns all nodes and links and applies the rules.
// Links are UNDIRECTED: link (A,B) is the same as (B,A).
// Nodes and links are never deleted, only failed/restored.
// ------------------------------------------------------------
class Network {
private:
    std::vector<Node> nodes;
    std::vector<Link> links;

    // Position of a node in 'nodes', or -1 if not found
    int findNodeIndex(int id) const {
        for (size_t i = 0; i < nodes.size(); i++) {
            if (nodes[i].id == id)
                return static_cast<int>(i);
        }
        return -1;
    }

    // Position of a link in 'links' (either direction), or -1
    int findLinkIndex(int a, int b) const {
        for (size_t i = 0; i < links.size(); i++) {
            bool sameWay = (links[i].from == a && links[i].to == b);
            bool reverse = (links[i].from == b && links[i].to == a);
            if (sameWay || reverse)
                return static_cast<int>(i);
        }
        return -1;
    }

public:
    // ===================== Building =====================

    // Add a node. Fails (false) if the id is negative or exists.
    bool addNode(int id) {
        if (id < 0)
            return false;
        if (findNodeIndex(id) != -1)
            return false;

        nodes.push_back(Node(id));
        return true;
    }

    // Add a link between two EXISTING nodes.
    // Fails (false) for: missing node, self-loop, duplicate
    // link (in either direction), or cost <= 0.
    bool addLink(int a, int b, double cost = 1.0) {
        if (findNodeIndex(a) == -1 || findNodeIndex(b) == -1)
            return false;
        if (a == b)
            return false;
        if (cost <= 0)
            return false;
        if (findLinkIndex(a, b) != -1)
            return false;

        links.push_back(Link(a, b, cost));
        return true;
    }

    // ===================== Failure control =====================
    // All return true if the node/link exists (even if it was
    // already in the requested state), false if it does not exist.

    // Fails a node. Link stored statuses are NOT changed.
    bool failNode(int id) {
        int i = findNodeIndex(id);
        if (i == -1)
            return false;
        nodes[i].status = ElementStatus::FAILED;
        return true;
    }

    // Restores a node. Independently failed links stay failed.
    bool restoreNode(int id) {
        int i = findNodeIndex(id);
        if (i == -1)
            return false;
        nodes[i].status = ElementStatus::ACTIVE;
        return true;
    }

    // Fails only the link. End nodes are NOT changed.
    bool failLink(int a, int b) {
        int i = findLinkIndex(a, b);
        if (i == -1)
            return false;
        links[i].status = ElementStatus::FAILED;
        return true;
    }

    // Restores only the link's own status. If an end node is
    // still failed, the link stays unusable.
    bool restoreLink(int a, int b) {
        int i = findLinkIndex(a, b);
        if (i == -1)
            return false;
        links[i].status = ElementStatus::ACTIVE;
        return true;
    }

    // ===================== Status queries =====================

    bool hasNode(int id) const {
        return findNodeIndex(id) != -1;
    }

    bool hasLink(int a, int b) const {
        return findLinkIndex(a, b) != -1;
    }

    // Node status. Returns false if the node does not exist.
    bool isNodeActive(int id) const {
        int i = findNodeIndex(id);
        if (i == -1)
            return false;
        return nodes[i].status == ElementStatus::ACTIVE;
    }

    // STORED status of the link only (ignores end nodes).
    // Returns false if the link does not exist.
    bool isLinkActive(int a, int b) const {
        int i = findLinkIndex(a, b);
        if (i == -1)
            return false;
        return links[i].status == ElementStatus::ACTIVE;
    }

    // EFFECTIVE status: link ACTIVE and both end nodes ACTIVE.
    // Returns false if the link does not exist.
    bool isLinkUsable(int a, int b) const {
        int i = findLinkIndex(a, b);
        if (i == -1)
            return false;
        return isLinkUsable(links[i]);
    }

    // Same check for a Link object (handy inside loops)
    bool isLinkUsable(const Link& link) const {
        if (link.status != ElementStatus::ACTIVE)
            return false;
        return isNodeActive(link.from) && isNodeActive(link.to);
    }

    // ===================== Retrieval =====================

    // Read-only access to ALL nodes and links (stored status).
    const std::vector<Node>& getNodes() const { return nodes; }
    const std::vector<Link>& getLinks() const { return links; }

    // Usable neighbours of a node, with the link cost.
    // Empty if the node does not exist or is FAILED.
    // Failed links and failed neighbours are never included.
    std::vector<Neighbor> getNeighbors(int id) const {
        std::vector<Neighbor> result;

        if (!isNodeActive(id))
            return result;

        for (size_t i = 0; i < links.size(); i++) {
            if (!isLinkUsable(links[i]))
                continue;

            Neighbor n;
            n.cost = links[i].cost;

            if (links[i].from == id) {
                n.nodeId = links[i].to;
                result.push_back(n);
            } else if (links[i].to == id) {
                n.nodeId = links[i].from;
                result.push_back(n);
            }
        }
        return result;
    }

    // ===================== Counts =====================

    // ---- Nodes (a node has only one status) ----
    int getTotalNodes() const {
        return static_cast<int>(nodes.size());
    }

    int getActiveNodeCount() const {
        int count = 0;
        for (size_t i = 0; i < nodes.size(); i++) {
            if (nodes[i].status == ElementStatus::ACTIVE)
                count++;
        }
        return count;
    }

    int getFailedNodeCount() const {
        return getTotalNodes() - getActiveNodeCount();
    }

    // ---- Links ----
    int getTotalLinks() const {
        return static_cast<int>(links.size());
    }

    // STORED counts: look only at each link's own status.
    int getStoredActiveLinkCount() const {
        int count = 0;
        for (size_t i = 0; i < links.size(); i++) {
            if (links[i].status == ElementStatus::ACTIVE)
                count++;
        }
        return count;
    }

    int getStoredFailedLinkCount() const {
        return getTotalLinks() - getStoredActiveLinkCount();
    }

    // EFFECTIVE counts: what can really carry traffic now.
    // Use these for Metrics.h (activeLinks / failedLinks).
    int getActiveLinkCount() const {
        int count = 0;
        for (size_t i = 0; i < links.size(); i++) {
            if (isLinkUsable(links[i]))
                count++;
        }
        return count;
    }

    int getFailedLinkCount() const {
        return getTotalLinks() - getActiveLinkCount();
    }

    // Links whose own status is ACTIVE but are unusable ONLY
    // because an end node has failed.
    int getNodeBlockedLinkCount() const {
        return getStoredActiveLinkCount() - getActiveLinkCount();
    }

    // ===================== Display =====================

    void displayNetwork() const {
        std::cout << "\n=== NETWORK TOPOLOGY ===\n";

        std::cout << "Nodes (" << getTotalNodes() << "):\n";
        if (nodes.empty())
            std::cout << "  (none)\n";
        for (size_t i = 0; i < nodes.size(); i++) {
            std::cout << "  Node " << nodes[i].id << " : "
                      << statusToString(nodes[i].status) << '\n';
        }

        std::cout << "Links (" << getTotalLinks() << "):\n";
        if (links.empty())
            std::cout << "  (none)\n";
        for (size_t i = 0; i < links.size(); i++) {
            std::cout << "  " << links[i].from << " -- " << links[i].to
                      << " | cost " << links[i].cost
                      << " | stored: " << statusToString(links[i].status)
                      << " | usable: "
                      << (isLinkUsable(links[i]) ? "YES" : "NO") << '\n';
        }

        std::cout << "Summary:\n";
        std::cout << "  Nodes active/failed        : "
                  << getActiveNodeCount() << " / "
                  << getFailedNodeCount() << '\n';
        std::cout << "  Links usable/unusable      : "
                  << getActiveLinkCount() << " / "
                  << getFailedLinkCount() << '\n';
        std::cout << "  Links stored active/failed : "
                  << getStoredActiveLinkCount() << " / "
                  << getStoredFailedLinkCount() << '\n';
        std::cout << "  Blocked by failed node     : "
                  << getNodeBlockedLinkCount() << '\n';
    }
};

#endif
