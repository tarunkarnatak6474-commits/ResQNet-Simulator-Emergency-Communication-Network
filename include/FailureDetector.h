#ifndef RESQNET_FAILURE_DETECTOR_H
#define RESQNET_FAILURE_DETECTOR_H

// ============================================================
// FailureDetector.h  -  ResQNet Simulator, Module 3
//
// Simulates HEARTBEAT-BASED failure detection and recovery
// detection on a Network (NetworkModel.h).
//
// THE IDEA
//   A monitor sends a "heartbeat" (a small "are you alive?"
//   message) to each monitored node. An alive node answers.
//   If answers stop coming, the monitor does not panic at the
//   first miss. It counts MISSED heartbeats in a row, and only
//   when the count reaches a THRESHOLD does it declare the node
//   FAILED. When answers start coming again it declares the node
//   RECOVERED.
//
// IMPORTANT: this is a SIMULATION. No sockets, timers or threads.
//   One call = one heartbeat "tick". Results are deterministic.
//
// TWO KINDS OF STATE (kept separate on purpose)
//   1. ACTUAL state    : Network node status (what is really true).
//                        Changed by FailureSimulator or by this
//                        detector when it declares failure/recovery.
//   2. BELIEVED state  : HeartbeatState here (what the detector
//                        thinks, based only on heartbeats).
//   A failure injected by FailureSimulator is NOT detected
//   automatically. It is detected only after enough missed
//   heartbeats have been recorded.
//
// TWO WAYS TO FEED HEARTBEAT RESULTS
//   - sendHeartbeat(id)          : simulates a ping. The node
//                                  "responds" if it is ACTIVE in the
//                                  Network (read through the public
//                                  Network API only).
//   - recordHeartbeatSuccess(id) / recordHeartbeatMiss(id)
//                                : you choose the outcome (scripted,
//                                  fully deterministic tests).
//
// WHAT THIS MODULE DOES NOT DO
//   - no routing, no JSON, no metrics, no link logic.
//   - it never touches link status. It only calls
//     Network::failNode / restoreNode, so links that failed on
//     their own stay failed when a node is restored (Network's
//     stored-vs-effective rule does this for us).
//
// Uses only the public Network API: hasNode, isNodeActive,
// failNode, restoreNode. Does NOT include FailureSimulator.h;
// both modules just share the same Network object.
// ============================================================

#include <iostream>
#include <string>
#include <vector>
#include "NetworkModel.h"

// ------------------------------------------------------------
// Result of trying to register a node for monitoring
// ------------------------------------------------------------
enum class RegisterResult {
    REGISTERED,         // node is now monitored
    ALREADY_MONITORED,  // it was already monitored (state kept)
    NOT_FOUND           // the node does not exist in the Network
};

inline std::string registerResultToString(RegisterResult r) {
    if (r == RegisterResult::REGISTERED)
        return "REGISTERED";
    if (r == RegisterResult::ALREADY_MONITORED)
        return "ALREADY_MONITORED";
    return "NOT_FOUND";
}

// ------------------------------------------------------------
// What one heartbeat result caused
// ------------------------------------------------------------
enum class HeartbeatEffect {
    NOT_MONITORED,      // node id is not being monitored
    NONE,               // counters updated, no state change
    FAILURE_DETECTED,   // threshold reached: node declared FAILED
    RECOVERY_DETECTED   // responses resumed: node declared RECOVERED
};

inline std::string heartbeatEffectToString(HeartbeatEffect e) {
    if (e == HeartbeatEffect::NOT_MONITORED)
        return "NOT_MONITORED";
    if (e == HeartbeatEffect::NONE)
        return "NONE";
    if (e == HeartbeatEffect::FAILURE_DETECTED)
        return "FAILURE_DETECTED";
    return "RECOVERY_DETECTED";
}

// ------------------------------------------------------------
// Detection event log (separate from FailureSimulator's log:
// that one records what was INJECTED, this one records what
// was DETECTED).
// ------------------------------------------------------------
enum class DetectionEventType {
    FAILURE_DETECTED,
    RECOVERY_DETECTED
};

inline std::string detectionEventTypeToString(DetectionEventType t) {
    if (t == DetectionEventType::FAILURE_DETECTED)
        return "FAILURE_DETECTED";
    return "RECOVERY_DETECTED";
}

struct DetectionEvent {
    int number;                 // 1, 2, 3 ... in order of detection
    DetectionEventType type;
    int nodeId;
    std::string description;    // e.g. "Node 2 declared FAILED ..."
};

// ------------------------------------------------------------
// Heartbeat state of ONE monitored node (the detector's belief)
// ------------------------------------------------------------
struct HeartbeatState {
    int nodeId;
    int missedCount;            // CONSECUTIVE missed heartbeats
    int consecutiveSuccesses;   // consecutive responses in a row
    bool declaredFailed;        // what the detector currently believes
    int heartbeatsSent;         // total heartbeat results processed
    int responsesReceived;      // how many of them were responses
};

// ------------------------------------------------------------
// FailureDetector
// Holds a REFERENCE to a Network (it does not own or copy it).
// The Network must outlive the detector.
// ------------------------------------------------------------
class FailureDetector {
public:
    static const int DEFAULT_MISS_THRESHOLD = 3;
    static const int DEFAULT_RECOVERY_THRESHOLD = 1;

private:
    Network& network;
    std::vector<HeartbeatState> states;
    std::vector<DetectionEvent> eventLog;
    int missThreshold;       // misses in a row needed to declare failure
    int recoveryThreshold;   // responses in a row needed to declare recovery

    // Position of a node in 'states', or -1 if not monitored
    int findStateIndex(int id) const {
        for (size_t i = 0; i < states.size(); i++) {
            if (states[i].nodeId == id)
                return static_cast<int>(i);
        }
        return -1;
    }

    void logEvent(DetectionEventType type, int nodeId,
                  const std::string& description) {
        DetectionEvent e;
        e.number = static_cast<int>(eventLog.size()) + 1;
        e.type = type;
        e.nodeId = nodeId;
        e.description = description;
        eventLog.push_back(e);
    }

    // The detector now BELIEVES the node is failed.
    // Update the real Network only if it still shows ACTIVE
    // (the node may already have been failed by FailureSimulator).
    void declareFailure(int index) {
        HeartbeatState& s = states[index];
        s.declaredFailed = true;

        if (network.isNodeActive(s.nodeId))
            network.failNode(s.nodeId);

        logEvent(DetectionEventType::FAILURE_DETECTED, s.nodeId,
                 "Node " + std::to_string(s.nodeId) +
                 " declared FAILED after " +
                 std::to_string(s.missedCount) + " missed heartbeats");
    }

    // The detector now BELIEVES the node is back.
    // Restoring only the node is safe: Network never changed any
    // link's stored status when the node failed, so links that
    // failed independently stay failed.
    void declareRecovery(int index) {
        HeartbeatState& s = states[index];
        s.declaredFailed = false;
        s.missedCount = 0;

        if (!network.isNodeActive(s.nodeId))
            network.restoreNode(s.nodeId);

        logEvent(DetectionEventType::RECOVERY_DETECTED, s.nodeId,
                 "Node " + std::to_string(s.nodeId) +
                 " declared RECOVERED after " +
                 std::to_string(s.consecutiveSuccesses) +
                 " successful heartbeat(s)");
    }

    // Core logic: process ONE heartbeat outcome for a node.
    HeartbeatEffect applyResult(int index, bool responded) {
        HeartbeatState& s = states[index];
        s.heartbeatsSent++;

        if (responded) {
            s.responsesReceived++;
            s.missedCount = 0;              // misses must be consecutive
            s.consecutiveSuccesses++;

            if (s.declaredFailed &&
                s.consecutiveSuccesses >= recoveryThreshold) {
                declareRecovery(index);
                return HeartbeatEffect::RECOVERY_DETECTED;
            }
            return HeartbeatEffect::NONE;
        }

        s.missedCount++;
        s.consecutiveSuccesses = 0;

        if (!s.declaredFailed && s.missedCount >= missThreshold) {
            declareFailure(index);
            return HeartbeatEffect::FAILURE_DETECTED;
        }
        return HeartbeatEffect::NONE;
    }

public:
    // Thresholds below 1 make no sense, so they fall back to defaults.
    FailureDetector(Network& net,
                    int missedHeartbeatThreshold = DEFAULT_MISS_THRESHOLD,
                    int recoveryResponseThreshold = DEFAULT_RECOVERY_THRESHOLD)
        : network(net) {
        missThreshold = (missedHeartbeatThreshold >= 1)
                            ? missedHeartbeatThreshold
                            : DEFAULT_MISS_THRESHOLD;
        recoveryThreshold = (recoveryResponseThreshold >= 1)
                            ? recoveryResponseThreshold
                            : DEFAULT_RECOVERY_THRESHOLD;
    }

    // ===================== Configuration =====================

    // Returns false (and changes nothing) if value < 1.
    // Does not re-check nodes by itself: call checkAllTimeouts()
    // afterwards if you lowered the threshold.
    bool setMissThreshold(int value) {
        if (value < 1)
            return false;
        missThreshold = value;
        return true;
    }

    bool setRecoveryThreshold(int value) {
        if (value < 1)
            return false;
        recoveryThreshold = value;
        return true;
    }

    int getMissThreshold() const { return missThreshold; }
    int getRecoveryThreshold() const { return recoveryThreshold; }

    // ===================== Registering nodes =====================

    // Start monitoring a node. The node must exist in the Network.
    // Registering twice does NOT reset its heartbeat state.
    RegisterResult registerNode(int id) {
        if (!network.hasNode(id))
            return RegisterResult::NOT_FOUND;
        if (findStateIndex(id) != -1)
            return RegisterResult::ALREADY_MONITORED;

        HeartbeatState s = { id, 0, 0, false, 0, 0 };
        states.push_back(s);
        return RegisterResult::REGISTERED;
    }

    // Registers every node currently in the Network.
    // Returns how many were newly registered.
    int registerAllNodes() {
        int added = 0;
        const std::vector<Node>& allNodes = network.getNodes();
        for (size_t i = 0; i < allNodes.size(); i++) {
            if (registerNode(allNodes[i].id) == RegisterResult::REGISTERED)
                added++;
        }
        return added;
    }

    // Stop monitoring a node. Returns false if it was not monitored.
    // The Network is not changed.
    bool unregisterNode(int id) {
        int i = findStateIndex(id);
        if (i == -1)
            return false;
        states.erase(states.begin() + i);
        return true;
    }

    bool isMonitored(int id) const {
        return findStateIndex(id) != -1;
    }

    int getMonitoredCount() const {
        return static_cast<int>(states.size());
    }

    // IDs of all monitored nodes, in registration order
    std::vector<int> getMonitoredIds() const {
        std::vector<int> ids;
        for (size_t i = 0; i < states.size(); i++)
            ids.push_back(states[i].nodeId);
        return ids;
    }

    // ===================== Heartbeats =====================

    // Simulates sending ONE heartbeat to a node. The node responds
    // if it is ACTIVE in the Network, otherwise the heartbeat is
    // missed. The result is then recorded.
    HeartbeatEffect sendHeartbeat(int id) {
        int i = findStateIndex(id);
        if (i == -1)
            return HeartbeatEffect::NOT_MONITORED;

        bool responded = network.isNodeActive(id);
        return applyResult(i, responded);
    }

    // Sends one heartbeat to EVERY monitored node (one "round").
    // Returns how many detection events (failures + recoveries)
    // this round caused.
    int runHeartbeatRound() {
        int events = 0;
        for (size_t i = 0; i < states.size(); i++) {
            HeartbeatEffect e = applyResult(static_cast<int>(i),
                                  network.isNodeActive(states[i].nodeId));
            if (e == HeartbeatEffect::FAILURE_DETECTED ||
                e == HeartbeatEffect::RECOVERY_DETECTED)
                events++;
        }
        return events;
    }

    // Scripted outcome: the node answered this heartbeat.
    HeartbeatEffect recordHeartbeatSuccess(int id) {
        int i = findStateIndex(id);
        if (i == -1)
            return HeartbeatEffect::NOT_MONITORED;
        return applyResult(i, true);
    }

    // Scripted outcome: the node did NOT answer this heartbeat.
    HeartbeatEffect recordHeartbeatMiss(int id) {
        int i = findStateIndex(id);
        if (i == -1)
            return HeartbeatEffect::NOT_MONITORED;
        return applyResult(i, false);
    }

    // ===================== Threshold checks =====================

    // True if the node has missed at least 'missThreshold'
    // heartbeats in a row. False if not monitored.
    bool hasReachedThreshold(int id) const {
        int i = findStateIndex(id);
        if (i == -1)
            return false;
        return states[i].missedCount >= missThreshold;
    }

    // Declares failure if the threshold is reached and the node is
    // not yet declared failed. Normally recordHeartbeatMiss does
    // this already; this is useful after setMissThreshold().
    HeartbeatEffect checkTimeout(int id) {
        int i = findStateIndex(id);
        if (i == -1)
            return HeartbeatEffect::NOT_MONITORED;

        if (!states[i].declaredFailed &&
            states[i].missedCount >= missThreshold) {
            declareFailure(i);
            return HeartbeatEffect::FAILURE_DETECTED;
        }
        return HeartbeatEffect::NONE;
    }

    // checkTimeout() for every monitored node.
    // Returns how many failures were newly detected.
    int checkAllTimeouts() {
        int detected = 0;
        for (size_t i = 0; i < states.size(); i++) {
            if (checkTimeout(states[i].nodeId) ==
                    HeartbeatEffect::FAILURE_DETECTED)
                detected++;
        }
        return detected;
    }

    // ===================== Heartbeat state queries =====================

    // Consecutive missed heartbeats, or -1 if not monitored.
    int getMissedCount(int id) const {
        int i = findStateIndex(id);
        return (i == -1) ? -1 : states[i].missedCount;
    }

    // Consecutive responses, or -1 if not monitored.
    int getConsecutiveSuccesses(int id) const {
        int i = findStateIndex(id);
        return (i == -1) ? -1 : states[i].consecutiveSuccesses;
    }

    // Total heartbeat results processed, or -1 if not monitored.
    int getHeartbeatsSent(int id) const {
        int i = findStateIndex(id);
        return (i == -1) ? -1 : states[i].heartbeatsSent;
    }

    // Total responses received, or -1 if not monitored.
    int getResponsesReceived(int id) const {
        int i = findStateIndex(id);
        return (i == -1) ? -1 : states[i].responsesReceived;
    }

    // What the detector BELIEVES (not necessarily the real state).
    // False if not monitored.
    bool isDeclaredFailed(int id) const {
        int i = findStateIndex(id);
        return (i == -1) ? false : states[i].declaredFailed;
    }

    // How many monitored nodes are currently declared failed
    int getDeclaredFailedCount() const {
        int count = 0;
        for (size_t i = 0; i < states.size(); i++) {
            if (states[i].declaredFailed)
                count++;
        }
        return count;
    }

    // ===================== Resetting =====================

    // Clears one node's heartbeat counters and belief. The node
    // stays monitored. The Network is NOT changed. Returns false
    // if the node is not monitored.
    bool resetNode(int id) {
        int i = findStateIndex(id);
        if (i == -1)
            return false;
        HeartbeatState fresh = { id, 0, 0, false, 0, 0 };
        states[i] = fresh;
        return true;
    }

    // resetNode() for every monitored node. Returns how many.
    int resetAll() {
        for (size_t i = 0; i < states.size(); i++) {
            HeartbeatState fresh = { states[i].nodeId, 0, 0, false, 0, 0 };
            states[i] = fresh;
        }
        return static_cast<int>(states.size());
    }

    // Stops monitoring every node (counters are discarded).
    // The Network and the event log are NOT changed.
    void clearMonitoring() {
        states.clear();
    }

    // ===================== Detection event log =====================

    const std::vector<DetectionEvent>& getEventLog() const {
        return eventLog;
    }

    int getEventCount() const {
        return static_cast<int>(eventLog.size());
    }

    // Number of events of one type in the log
    int countEvents(DetectionEventType type) const {
        int count = 0;
        for (size_t i = 0; i < eventLog.size(); i++) {
            if (eventLog[i].type == type)
                count++;
        }
        return count;
    }

    void clearEventLog() {
        eventLog.clear();
    }

    // ===================== Display =====================

    void displayEventLog() const {
        std::cout << "\n=== DETECTION LOG ===\n";
        if (eventLog.empty())
            std::cout << "  (no events)\n";
        for (size_t i = 0; i < eventLog.size(); i++) {
            std::cout << "  [" << eventLog[i].number << "] "
                      << eventLog[i].description << '\n';
        }
    }

    void displayStatus() const {
        std::cout << "\n=== HEARTBEAT STATUS (miss threshold "
                  << missThreshold << ", recovery threshold "
                  << recoveryThreshold << ") ===\n";
        if (states.empty())
            std::cout << "  (no monitored nodes)\n";
        for (size_t i = 0; i < states.size(); i++) {
            std::cout << "  Node " << states[i].nodeId
                      << " | missed " << states[i].missedCount
                      << " | believed "
                      << (states[i].declaredFailed ? "FAILED" : "ALIVE")
                      << " | actual "
                      << statusToString(network.isNodeActive(states[i].nodeId)
                                        ? ElementStatus::ACTIVE
                                        : ElementStatus::FAILED)
                      << '\n';
        }
    }
};

// C++11: static const int members used by value need no definition,
// but taking their address would. We only ever use them by value.

#endif