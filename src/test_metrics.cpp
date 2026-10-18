#include <iostream>
#include <string>
#include "Metrics.h"

using namespace std;

int testsPassed = 0;
int testsFailed = 0;

void testCheckNumber(const string& name, double actual, double expected) {
    if (actual == expected) {
        cout << "[PASS] " << name << '\n';
        testsPassed++;
    } else {
        cout << "[FAIL] " << name << " | Expected: " << expected
             << " | Actual: " << actual << '\n';
        testsFailed++;
    }
}

void testCheckString(const string& name,
                     const string& actual,
                     const string& expected) {
    if (actual == expected) {
        cout << "[PASS] " << name << '\n';
        testsPassed++;
    } else {
        cout << "[FAIL] " << name << " | Expected: " << expected
             << " | Actual: " << actual << '\n';
        testsFailed++;
    }
}

void testCheckBool(const string& name, bool actual, bool expected) {
    if (actual == expected) {
        cout << "[PASS] " << name << '\n';
        testsPassed++;
    } else {
        cout << "[FAIL] " << name << '\n';
        testsFailed++;
    }
}

int main() {
    cout << "=== ResQNet Metrics Tests ===\n\n";

    NetworkScenario normal = {
        "Normal", 100, 100, 10, 0, 12, 0, 0, 12
    };
    MetricsResult a = calculateMetrics(normal);

    testCheckBool("Normal input valid", a.valid, true);
    testCheckNumber("Normal packets lost", a.packetsLost, 0);
    testCheckNumber("Normal delivery ratio", a.deliveryRatio, 100);
    testCheckNumber("Normal loss rate", a.lossRate, 0);
    testCheckNumber("Normal operational nodes", a.operationalNodes, 10);
    testCheckNumber("Normal node availability", a.nodeAvailability, 100);
    testCheckString("Normal rating", a.rating, "EXCELLENT");

    NetworkScenario failure = {
        "Failure", 100, 55, 10, 3, 8, 4, -1, 12
    };
    MetricsResult b = calculateMetrics(failure);

    testCheckBool("Failure input valid", b.valid, true);
    testCheckNumber("Failure packets lost", b.packetsLost, 45);
    testCheckNumber("Failure delivery ratio", b.deliveryRatio, 55);
    testCheckNumber("Failure loss rate", b.lossRate, 45);
    testCheckNumber("Failure operational nodes", b.operationalNodes, 7);
    testCheckNumber("Failure node availability", b.nodeAvailability, 70);
    testCheckString("Failure rating", b.rating, "MODERATE");

    NetworkScenario staticRun = {
        "Static", 100, 55, 10, 3, 8, 4, 0, 12
    };
    NetworkScenario adaptiveRun = {
        "Adaptive", 100, 82, 10, 3, 8, 4, 4, 12
    };

    MetricsResult c = calculateMetrics(staticRun);
    MetricsResult d = calculateMetrics(adaptiveRun);

    testCheckNumber("Static delivery ratio", c.deliveryRatio, 55);
    testCheckNumber("Adaptive delivery ratio", d.deliveryRatio, 82);
    testCheckNumber("Adaptive improvement", d.deliveryRatio - c.deliveryRatio, 27);
    testCheckNumber("Adaptive route changes", d.routeChanges, 4);

    FairnessReport fairness = checkFairness(staticRun, adaptiveRun);
    testCheckBool("Matching network counts", fairness.countsMatch, true);

    NetworkScenario zeroPackets = {
        "Zero", 0, 0, 10, 0, 12, 0, -1, 12
    };
    MetricsResult e = calculateMetrics(zeroPackets);

    testCheckBool("Zero-packet input valid", e.valid, true);
    testCheckBool("Zero-packet ratio unavailable", e.hasPacketData, false);
    testCheckString("Zero-packet rating", e.rating, "N/A");

    NetworkScenario invalidPackets = {
        "Invalid", 10, 15, 10, 0, 8, 0, -1, 12
    };
    MetricsResult f = calculateMetrics(invalidPackets);

    testCheckBool("Excess delivered packets rejected", f.valid, false);
    testCheckString("Invalid packet error", f.error,
                    "delivered packets exceed sent packets");

    NetworkScenario invalidNodes = {
        "Invalid nodes", 100, 80, 5, 6, 8, 0, -1, 12
    };
    MetricsResult g = calculateMetrics(invalidNodes);

    testCheckBool("Excess failed nodes rejected", g.valid, false);

    NetworkScenario invalidLinks = {
        "Invalid links", 100, 80, 10, 2, 8, 5, -1, 12
    };
    MetricsResult h = calculateMetrics(invalidLinks);

    testCheckBool("Inconsistent links rejected", h.valid, false);

    NetworkScenario differentTraffic = {
        "Different traffic", 200, 150, 10, 3, 8, 4, 2, 12
    };
    FairnessReport fairness2 = checkFairness(staticRun, differentTraffic);

    testCheckBool("Different traffic detected", fairness2.countsMatch, false);

    string json = toJson(d);
    testCheckBool("JSON contains name",
                  json.find("\"name\":\"Adaptive\"") != string::npos, true);
    testCheckBool("JSON contains delivery ratio",
                  json.find("\"deliveryRatio\":82.00") != string::npos, true);
    testCheckBool("JSON contains total links",
                  json.find("\"totalLinks\":12") != string::npos, true);
    testCheckBool("JSON contains route changes",
                  json.find("\"routeChanges\":4") != string::npos, true);

    testCheckString("JSON quote escaping", escapeJson("A\"B"), "A\\\"B");
    testCheckString("JSON backslash escaping", escapeJson("A\\B"), "A\\\\B");
    testCheckString("JSON newline escaping", escapeJson("A\nB"), "A\\nB");

    cout << "\n=== Test Summary ===\n";
    cout << "Passed: " << testsPassed << '\n';
    cout << "Failed: " << testsFailed << '\n';
    cout << "Total : " << testsPassed + testsFailed << '\n';

    if (testsFailed == 0) {
        cout << "All tests passed!\n";
        return 0;
    }

    cout << "Some tests failed.\n";
    return 1;
}