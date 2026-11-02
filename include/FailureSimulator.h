#ifndef RESQNET_FAILURE_SIMULATOR_H
#define RESQNET_FAILURE_SIMULATOR_H

// ============================================================
// FailureSimulator.h  -  ResQNet Simulator, Module 2
//
// Causes and reverses failures on a Network (NetworkModel.h).
//
// What this module DOES:
//   - manual node / link failure and recovery
//   - random node / link failures (reproducible with a seed)
//   - simple scenarios (a list of steps that can be replayed)
//   - an event log of every real change
//
// What this module does NOT do:
//   - it does not store node/link state (Network does that)
//   - no routing, no failure detection, no JSON, no metrics
//
// The simulator only calls the public Network functions
// (failNode, restoreNode, failLink, restoreLink, ...), so the
// rules in NetworkModel.h (stored vs effective link status)
// are never duplicated or bypassed.
//
// ABOUT RETURN VALUES (ActionResult):
//   SUCCESS   : the state really changed (and the event is logged)
//   NO_CHANGE : the element was already in the requested state
//   NOT_FOUND : the node or link does not exist
// Only real changes (SUCCESS) are written to the event log, so
// the log is an accurate history of the network.
// ============================================================

#include <iostream>
#include <string>
#include <vector>
#include "NetworkModel.h"

// ------------------------------------------------------------
// Result of one manual action
// ------------------------------------------------------------
enum class ActionResult { SUCCESS, NO_CHANGE, NOT_FOUND };

inline std::string actionResultToString(ActionResult r) {
    if (r == ActionResult::SUCCESS)
        return "SUCCESS";
    if (r == ActionResult::NO_CHANGE)
        return "NO_CHANGE";
    return "NOT_FOUND";
}

// ------------------------------------------------------------
// Event log
// ------------------------------------------------------------
enum class EventType {
    NODE_FAILED,
    NODE_RESTORED,
    LINK_FAILED,
    LINK_RESTORED,
    INFO            // messages such as "Scenario started"
};

inline std::string eventTypeToString(EventType t) {
    switch (t) {
        case EventType::NODE_FAILED:   return "NODE_FAILED";
        case EventType::NODE_RESTORED: return "NODE_RESTORED";
        case EventType::LINK_FAILED:   return "LINK_FAILED";
        case EventType::LINK_RESTORED: return "LINK_RESTORED";
        default:                       return "INFO";
    }
}

// One log entry. Unused id fields are set to -1.
struct FailureEvent {
    int number;               // 1, 2, 3 ... in order of happening
    EventType type;
    int nodeId;               // for node events
    int linkFrom;             // for link events
    int linkTo;               // for link events
    std::string description;  // e.g. "Node 1 failed"
};

// A link identified by its two end nodes
struct LinkId {
    int from;
    int to;
};

// ------------------------------------------------------------
// Scenarios: a named list of steps. Steps use node/link IDs
// given by the user, so no topology is hardcoded.
// ------------------------------------------------------------
enum class StepType {
    FAIL_NODE,
    RESTORE_NODE,
    FAIL_LINK,
    RESTORE_LINK,
    FAIL_RANDOM_NODES,
    FAIL_RANDOM_LINKS
};

struct ScenarioStep {
    StepType type;
    int a;       // node id, or first end of a link
    int b;       // second end of a link (unused otherwise)
    int count;   // how many, for random steps (unused otherwise)
};

struct Scenario {
    std::string name;
    std::vector<ScenarioStep> steps;

    Scenario(std::string scenarioName = "Unnamed scenario") {
        name = scenarioName;
    }

    void addFailNode(int id) {
        ScenarioStep s = { StepType::FAIL_NODE, id, -1, 0 };
        steps.push_back(s);
    }

    void addRestoreNode(int id) {
        ScenarioStep s = { StepType::RESTORE_NODE, id, -1, 0 };
        steps.push_back(s);
    }

    void addFailLink(int a, int b) {
        ScenarioStep s = { StepType::FAIL_LINK, a, b, 0 };
        steps.push_back(s);
    }

    void addRestoreLink(int a, int b) {
        ScenarioStep s = { StepType::RESTORE_LINK, a, b, 0 };
        steps.push_back(s);
    }

    void addRandomNodeFailures(int count) {
        ScenarioStep s = { StepType::FAIL_RANDOM_NODES, -1, -1, count };
        steps.push_back(s);
    }

    void addRandomLinkFailures(int count) {
        ScenarioStep s = { StepType::FAIL_RANDOM_LINKS, -1, -1, count };
        steps.push_back(s);
    }
};

// Summary of one scenario run. Every step is counted once:
//   changedSteps  : the step changed the network
//   noChangeSteps : nothing to do (already in that state, or a
//                   random step found no candidates)
//   invalidSteps  : the node or link does not exist
struct ScenarioResult {
    int totalSteps;
    int changedSteps;
    int noChangeSteps;
    int invalidSteps;
};

// Predefined scenario 1:
//   fail a node, fail a link, then restore the node.
inline Scenario createNodeAndLinkScenario(int nodeId,
                                          int linkFrom, int linkTo) {
    Scenario s("Node failure, link failure, node recovery");
    s.addFailNode(nodeId);
    s.addFailLink(linkFrom, linkTo);
    s.addRestoreNode(nodeId);
    return s;
}

// Predefined scenario 2:
//   fail two links, then restore the first one.
inline Scenario createTwoLinkScenario(int a1, int b1, int a2, int b2) {
    Scenario s("Two link failures, one recovery");
    s.addFailLink(a1, b1);
    s.addFailLink(a2, b2);
    s.addRestoreLink(a1, b1);
    return s;
}

// ------------------------------------------------------------
// FailureSimulator
// Holds a REFERENCE to a Network (it does not own or copy it),
// so Routing, Metrics and FailureDetector all see the same state.
// The Network must outlive the simulator.
// ------------------------------------------------------------
class FailureSimulator {
private:
    Network& network;
    std::vector<FailureEvent> eventLog;
    unsigned int randomState;

    // Adds one entry to the log
    void logEvent(EventType type, int nodeId, int linkFrom, int linkTo,
                  const std::string& description) {
        FailureEvent e;
        e.number = static_cast<int>(eventLog.size()) + 1;
        e.type = type;
        e.nodeId = nodeId;
        e.linkFrom = linkFrom;
        e.linkTo = linkTo;
        e.description = description;
        eventLog.push_back(e);
    }

    // Simple linear congruential generator (LCG).
    // Returns a number from 0 to limit-1 (limit must be > 0).
    // Written by hand (instead of rand()) so the same seed gives
    // the same numbers on every compiler and computer.
    int nextRandomIndex(int limit) {
        randomState = randomState * 1103515245u + 12345u;
        return static_cast<int>((randomState >> 16) % limit);
    }

public:
    // Seed controls the random failures: same seed + same
    // network = same failures.
    FailureSimulator(Network& net, unsigned int seed = 1)
        : network(net) {
        randomState = seed;
    }

    void setSeed(unsigned int seed) {
        randomState = seed;
    }

    // ===================== Manual failures / recovery =====================

    ActionResult failNode(int id) {
        if (!network.hasNode(id))
            return ActionResult::NOT_FOUND;
        if (!network.isNodeActive(id))
            return ActionResult::NO_CHANGE;

        network.failNode(id);
        logEvent(EventType::NODE_FAILED, id, -1, -1,
                 "Node " + std::to_string(id) + " failed");
        return ActionResult::SUCCESS;
    }

    ActionResult restoreNode(int id) {
        if (!network.hasNode(id))
            return ActionResult::NOT_FOUND;
        if (network.isNodeActive(id))
            return ActionResult::NO_CHANGE;

        network.restoreNode(id);
        logEvent(EventType::NODE_RESTORED, id, -1, -1,
                 "Node " + std::to_string(id) + " restored");
        return ActionResult::SUCCESS;
    }

    // Fails the link itself (its stored status). Works even if an
    // end node is already failed.
    ActionResult failLink(int a, int b) {
        if (!network.hasLink(a, b))
            return ActionResult::NOT_FOUND;
        if (!network.isLinkActive(a, b))
            return ActionResult::NO_CHANGE;

        network.failLink(a, b);
        logEvent(EventType::LINK_FAILED, -1, a, b,
                 "Link " + std::to_string(a) + "-" +
                 std::to_string(b) + " failed");
        return ActionResult::SUCCESS;
    }

    // Restores the link's own status. The link stays unusable if
    // an end node is still failed (Network decides that).
    ActionResult restoreLink(int a, int b) {
        if (!network.hasLink(a, b))
            return ActionResult::NOT_FOUND;
        if (network.isLinkActive(a, b))
            return ActionResult::NO_CHANGE;

        network.restoreLink(a, b);
        logEvent(EventType::LINK_RESTORED, -1, a, b,
                 "Link " + std::to_string(a) + "-" +
                 std::to_string(b) + " restored");
        return ActionResult::SUCCESS;
    }

    // ===================== Random failures =====================

    // Fails up to 'count' randomly chosen ACTIVE nodes.
    // Already failed nodes are never chosen. If fewer than
    // 'count' active nodes exist, all of them are failed.
    // Returns the IDs that were failed, in the order chosen.
    std::vector<int> failRandomNodes(int count) {
        std::vector<int> failedIds;
        std::vector<int> candidates;

        const std::vector<Node>& allNodes = network.getNodes();
        for (size_t i = 0; i < allNodes.size(); i++) {
            if (allNodes[i].status == ElementStatus::ACTIVE)
                candidates.push_back(allNodes[i].id);
        }

        while (count > 0 && !candidates.empty()) {
            int pick = nextRandomIndex(static_cast<int>(candidates.size()));
            int id = candidates[pick];

            failNode(id);
            failedIds.push_back(id);

            candidates.erase(candidates.begin() + pick);
            count--;
        }
        return failedIds;
    }

    // Fails up to 'count' randomly chosen USABLE links (link
    // active and both end nodes active). Links that are already
    // failed or already cut off by a failed node are not chosen.
    // Returns the links that were failed, in the order chosen.
    std::vector<LinkId> failRandomLinks(int count) {
        std::vector<LinkId> failedLinks;
        std::vector<LinkId> candidates;

        const std::vector<Link>& allLinks = network.getLinks();
        for (size_t i = 0; i < allLinks.size(); i++) {
            if (network.isLinkUsable(allLinks[i])) {
                LinkId id = { allLinks[i].from, allLinks[i].to };
                candidates.push_back(id);
            }
        }

        while (count > 0 && !candidates.empty()) {
            int pick = nextRandomIndex(static_cast<int>(candidates.size()));
            LinkId chosen = candidates[pick];

            failLink(chosen.from, chosen.to);
            failedLinks.push_back(chosen);

            candidates.erase(candidates.begin() + pick);
            count--;
        }
        return failedLinks;
    }

    // ===================== Scenarios =====================

    // Runs every step of a scenario in order and logs
    // "Scenario started" / "Scenario finished" messages.
    ScenarioResult runScenario(const Scenario& scenario) {
        ScenarioResult result = { 0, 0, 0, 0 };

        logEvent(EventType::INFO, -1, -1, -1,
                 "Scenario started: " + scenario.name);

        for (size_t i = 0; i < scenario.steps.size(); i++) {
            const ScenarioStep& step = scenario.steps[i];
            result.totalSteps++;

            ActionResult action = ActionResult::NO_CHANGE;

            if (step.type == StepType::FAIL_NODE) {
                action = failNode(step.a);
            } else if (step.type == StepType::RESTORE_NODE) {
                action = restoreNode(step.a);
            } else if (step.type == StepType::FAIL_LINK) {
                action = failLink(step.a, step.b);
            } else if (step.type == StepType::RESTORE_LINK) {
                action = restoreLink(step.a, step.b);
            } else if (step.type == StepType::FAIL_RANDOM_NODES) {
                if (!failRandomNodes(step.count).empty())
                    action = ActionResult::SUCCESS;
            } else if (step.type == StepType::FAIL_RANDOM_LINKS) {
                if (!failRandomLinks(step.count).empty())
                    action = ActionResult::SUCCESS;
            }

            if (action == ActionResult::SUCCESS)
                result.changedSteps++;
            else if (action == ActionResult::NO_CHANGE)
                result.noChangeSteps++;
            else
                result.invalidSteps++;
        }

        logEvent(EventType::INFO, -1, -1, -1,
                 "Scenario finished: " + scenario.name);
        return result;
    }

    // Brings the whole network back: restores every failed node,
    // then every failed link. Returns how many were restored.
    int restoreAll() {
        int restored = 0;

        const std::vector<Node>& allNodes = network.getNodes();
        for (size_t i = 0; i < allNodes.size(); i++) {
            if (restoreNode(allNodes[i].id) == ActionResult::SUCCESS)
                restored++;
        }

        const std::vector<Link>& allLinks = network.getLinks();
        for (size_t i = 0; i < allLinks.size(); i++) {
            if (restoreLink(allLinks[i].from, allLinks[i].to)
                    == ActionResult::SUCCESS)
                restored++;
        }
        return restored;
    }

    // ===================== Event log =====================

    const std::vector<FailureEvent>& getEventLog() const {
        return eventLog;
    }

    int getEventCount() const {
        return static_cast<int>(eventLog.size());
    }

    void clearEventLog() {
        eventLog.clear();
    }

    void displayEventLog() const {
        std::cout << "\n=== EVENT LOG ===\n";
        if (eventLog.empty())
            std::cout << "  (no events)\n";
        for (size_t i = 0; i < eventLog.size(); i++) {
            std::cout << "  [" << eventLog[i].number << "] "
                      << eventLog[i].description << '\n';
        }
    }
};

#endif
