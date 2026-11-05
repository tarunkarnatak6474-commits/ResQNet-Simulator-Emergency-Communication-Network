
#ifndef RESQNET_METRICS_H
#define RESQNET_METRICS_H

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <cstdio>

// Input data for one network simulation
struct NetworkScenario {
    std::string name;
    int packetsSent;
    int packetsDelivered;
    int totalNodes;
    int failedNodes;
    int activeLinks;
    int failedLinks;
    int routeChanges;
    int totalLinks;

    NetworkScenario(
        std::string n,
        int sent = 0,
        int delivered = 0,
        int nodes = 0,
        int failed = 0,
        int active = 0,
        int linksFailed = 0,
        int changes = -1,
        int linksTotal = -1
    ) {
        name = n;
        packetsSent = sent;
        packetsDelivered = delivered;
        totalNodes = nodes;
        failedNodes = failed;
        activeLinks = active;
        failedLinks = linksFailed;
        routeChanges = changes;
        totalLinks = linksTotal;
    }
};

// Calculated results
struct MetricsResult {
    std::string name;
    bool valid = true;
    std::string error;

    int packetsSent = 0;
    int packetsDelivered = 0;
    int packetsLost = 0;

    bool hasPacketData = false;
    double deliveryRatio = 0.0;
    double lossRate = 0.0;

    int totalNodes = 0;
    int failedNodes = 0;
    int operationalNodes = 0;

    bool hasNodeData = false;
    double nodeAvailability = 0.0;

    int activeLinks = 0;
    int failedLinks = 0;
    int totalLinks = -1;
    int routeChanges = -1;

    std::string rating;
};

// Validate input data
inline std::string validateScenario(const NetworkScenario& s) {
    if (s.packetsSent < 0 || s.packetsDelivered < 0 ||
        s.totalNodes < 0 || s.failedNodes < 0 ||
        s.activeLinks < 0 || s.failedLinks < 0) {
        return "negative value in input";
    }

    if (s.packetsDelivered > s.packetsSent)
        return "delivered packets exceed sent packets";

    if (s.failedNodes > s.totalNodes)
        return "failed nodes exceed total nodes";

    if (s.routeChanges < -1)
        return "routeChanges must be -1 (not recorded) or >= 0";

    if (s.totalLinks < -1)
        return "totalLinks must be -1 (not provided) or >= 0";

    if (s.totalLinks >= 0) {
        if (s.activeLinks > s.totalLinks)
            return "active links exceed total links";

        if (s.failedLinks > s.totalLinks)
            return "failed links exceed total links";

        if (s.activeLinks + s.failedLinks > s.totalLinks)
            return "active plus failed links exceed total links";
    }

    return "";
}

// Calculate metrics
inline MetricsResult calculateMetrics(const NetworkScenario& s) {
    MetricsResult r;
    r.name = s.name;
    r.error = validateScenario(s);

    if (!r.error.empty()) {
        r.valid = false;
        r.rating = "N/A";
        return r;
    }

    r.packetsSent = s.packetsSent;
    r.packetsDelivered = s.packetsDelivered;
    r.packetsLost = s.packetsSent - s.packetsDelivered;

    if (s.packetsSent > 0) {
        r.hasPacketData = true;
        r.deliveryRatio =
            static_cast<double>(s.packetsDelivered) /
            s.packetsSent * 100.0;

        r.lossRate =
            static_cast<double>(r.packetsLost) /
            s.packetsSent * 100.0;
    }

    r.totalNodes = s.totalNodes;
    r.failedNodes = s.failedNodes;
    r.operationalNodes = s.totalNodes - s.failedNodes;

    if (s.totalNodes > 0) {
        r.hasNodeData = true;
        r.nodeAvailability =
            static_cast<double>(r.operationalNodes) /
            s.totalNodes * 100.0;
    }

    r.activeLinks = s.activeLinks;
    r.failedLinks = s.failedLinks;
    r.totalLinks = s.totalLinks;
    r.routeChanges = s.routeChanges;

    if (!r.hasPacketData)
        r.rating = "N/A";
    else if (r.deliveryRatio >= 95)
        r.rating = "EXCELLENT";
    else if (r.deliveryRatio >= 80)
        r.rating = "GOOD";
    else if (r.deliveryRatio >= 50)
        r.rating = "MODERATE";
    else
        r.rating = "POOR";

    return r;
}

// Format percentages
inline std::string percentText(bool available, double value) {
    if (!available)
        return "N/A";

    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value << "%";
    return out.str();
}

inline std::string routeChangesText(int value) {
    return value < 0 ? "not recorded" : std::to_string(value);
}

// Display one scenario
inline void displayReport(const NetworkScenario& s) {
    MetricsResult r = calculateMetrics(s);

    std::cout << "\n========================================\n";
    std::cout << "Scenario: " << s.name << '\n';
    std::cout << "========================================\n";

    if (!r.valid) {
        std::cout << "INVALID INPUT: " << r.error << '\n';
        return;
    }

    std::cout << "Packets sent       : " << r.packetsSent << '\n';
    std::cout << "Packets delivered  : " << r.packetsDelivered << '\n';
    std::cout << "Packets lost       : " << r.packetsLost << '\n';
    std::cout << "Delivery ratio     : "
              << percentText(r.hasPacketData, r.deliveryRatio) << '\n';
    std::cout << "Packet loss rate   : "
              << percentText(r.hasPacketData, r.lossRate) << '\n';

    std::cout << "\n--- Network Status ---\n";
    std::cout << "Total nodes        : " << r.totalNodes << '\n';
    std::cout << "Failed nodes       : " << r.failedNodes << '\n';
    std::cout << "Operational nodes  : " << r.operationalNodes << '\n';
    std::cout << "Total links        : "
              << (r.totalLinks < 0 ? "not provided"
                                   : std::to_string(r.totalLinks)) << '\n';
    std::cout << "Active links       : " << r.activeLinks << '\n';
    std::cout << "Failed links       : " << r.failedLinks << '\n';
    std::cout << "Node availability  : "
              << percentText(r.hasNodeData, r.nodeAvailability) << '\n';
    std::cout << "Route changes      : "
              << routeChangesText(r.routeChanges) << '\n';
    std::cout << "Performance        : " << r.rating << '\n';
}

// Compare normal and failure scenarios
inline void compareScenarios(const NetworkScenario& a,
                             const NetworkScenario& b) {
    MetricsResult ra = calculateMetrics(a);
    MetricsResult rb = calculateMetrics(b);

    std::cout << "\n=== SCENARIO COMPARISON ===\n";
    std::cout << a.name << " delivery ratio: "
              << percentText(ra.valid && ra.hasPacketData,
                             ra.deliveryRatio) << '\n';
    std::cout << b.name << " delivery ratio: "
              << percentText(rb.valid && rb.hasPacketData,
                             rb.deliveryRatio) << '\n';

    if (ra.valid && rb.valid && ra.hasPacketData && rb.hasPacketData) {
        std::cout << "Delivery ratio decrease: "
                  << std::fixed << std::setprecision(2)
                  << ra.deliveryRatio - rb.deliveryRatio
                  << " percentage points\n";
    }
}

// Fairness check for static versus adaptive routing
struct FairnessReport {
    bool countsMatch = true;
    std::vector<std::string> mismatches;
    std::vector<std::string> limitations;
};

inline FairnessReport checkFairness(const NetworkScenario& a,
                                    const NetworkScenario& b) {
    FairnessReport f;

    auto compare = [&](const std::string& label, int x, int y) {
        if (x != y) {
            f.mismatches.push_back(
                label + " differs (" + std::to_string(x) +
                " vs " + std::to_string(y) + ")");
        }
    };

    compare("packetsSent", a.packetsSent, b.packetsSent);
    compare("totalNodes", a.totalNodes, b.totalNodes);
    compare("failedNodes", a.failedNodes, b.failedNodes);
    compare("activeLinks", a.activeLinks, b.activeLinks);
    compare("failedLinks", a.failedLinks, b.failedLinks);

    if (a.totalLinks >= 0 && b.totalLinks >= 0)
        compare("totalLinks", a.totalLinks, b.totalLinks);
    else
        f.limitations.push_back(
            "totalLinks was not provided for both runs");

    f.limitations.push_back(
        "Node and link IDs and topology were not compared");
    f.limitations.push_back(
        "Traffic source, destination and send times were not compared");

    f.countsMatch = f.mismatches.empty();
    return f;
}

inline bool isFairComparison(const NetworkScenario& a,
                             const NetworkScenario& b,
                             std::string& reason) {
    FairnessReport f = checkFairness(a, b);
    reason.clear();

    for (size_t i = 0; i < f.mismatches.size(); i++) {
        if (i > 0)
            reason += "; ";
        reason += f.mismatches[i];
    }

    return f.countsMatch;
}

// Display static versus adaptive comparison
inline void compareRouting(const NetworkScenario& st,
                           const NetworkScenario& ad) {
    MetricsResult rs = calculateMetrics(st);
    MetricsResult ra = calculateMetrics(ad);

    std::cout << "\n=== STATIC vs ADAPTIVE ROUTING ===\n";

    if (!rs.valid || !ra.valid) {
        std::cout << "Cannot compare invalid input.\n";
        return;
    }

    FairnessReport f = checkFairness(st, ad);

    if (!f.countsMatch) {
        std::cout << "WARNING: compared counts differ:\n";
        for (const auto& m : f.mismatches)
            std::cout << " - " << m << '\n';
    } else {
        std::cout << "Compared counts match; this does not prove "
                     "the runs are fully fair.\n";
    }

    const int w1 = 22;
    const int w2 = 16;

    std::cout << std::left << std::setw(w1) << "Metric"
              << std::setw(w2) << "Static"
              << std::setw(w2) << "Adaptive" << '\n';
    std::cout << std::string(w1 + 2 * w2, '-') << '\n';

    auto row = [&](const std::string& label,
                   const std::string& x,
                   const std::string& y) {
        std::cout << std::left << std::setw(w1) << label
                  << std::setw(w2) << x
                  << std::setw(w2) << y << '\n';
    };

    row("Packets sent", std::to_string(rs.packetsSent),
        std::to_string(ra.packetsSent));
    row("Packets delivered", std::to_string(rs.packetsDelivered),
        std::to_string(ra.packetsDelivered));
    row("Packets lost", std::to_string(rs.packetsLost),
        std::to_string(ra.packetsLost));
    row("Delivery ratio",
        percentText(rs.hasPacketData, rs.deliveryRatio),
        percentText(ra.hasPacketData, ra.deliveryRatio));
    row("Packet loss rate",
        percentText(rs.hasPacketData, rs.lossRate),
        percentText(ra.hasPacketData, ra.lossRate));
    row("Route changes", routeChangesText(rs.routeChanges),
        routeChangesText(ra.routeChanges));
    row("Node availability",
        percentText(rs.hasNodeData, rs.nodeAvailability),
        percentText(ra.hasNodeData, ra.nodeAvailability));

    if (rs.hasPacketData && ra.hasPacketData) {
        std::cout << "Adaptive PDR improvement: "
                  << std::fixed << std::setprecision(2)
                  << ra.deliveryRatio - rs.deliveryRatio
                  << " percentage points\n";
    }

    for (const auto& l : f.limitations)
        std::cout << "Limitation: " << l << '\n';
}

// Escape special characters for JSON strings
inline std::string escapeJson(const std::string& text) {
    std::string out;

    for (size_t i = 0; i < text.size(); i++) {
        unsigned char c = static_cast<unsigned char>(text[i]);

        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;

            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }

    return out;
}

// Convert calculated metrics to JSON
inline std::string toJson(const MetricsResult& r) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(2);

    o << "{\"name\":\"" << escapeJson(r.name)
      << "\",\"valid\":" << (r.valid ? "true" : "false");

    if (!r.valid) {
        o << ",\"error\":\"" << escapeJson(r.error) << "\"}";
        return o.str();
    }

    o << ",\"packetsSent\":" << r.packetsSent
      << ",\"packetsDelivered\":" << r.packetsDelivered
      << ",\"packetsLost\":" << r.packetsLost
      << ",\"deliveryRatio\":";

    if (r.hasPacketData)
        o << r.deliveryRatio;
    else
        o << "null";

    o << ",\"lossRate\":";
    if (r.hasPacketData)
        o << r.lossRate;
    else
        o << "null";

    o << ",\"totalNodes\":" << r.totalNodes
      << ",\"failedNodes\":" << r.failedNodes
      << ",\"operationalNodes\":" << r.operationalNodes
      << ",\"nodeAvailability\":";

    if (r.hasNodeData)
        o << r.nodeAvailability;
    else
        o << "null";

    o << ",\"totalLinks\":";
    if (r.totalLinks >= 0)
        o << r.totalLinks;
    else
        o << "null";

    o << ",\"activeLinks\":" << r.activeLinks
      << ",\"failedLinks\":" << r.failedLinks
      << ",\"routeChanges\":";

    if (r.routeChanges >= 0)
        o << r.routeChanges;
    else
        o << "null";

    o << ",\"rating\":\"" << escapeJson(r.rating) << "\"}";
    return o.str();
}

#endif
