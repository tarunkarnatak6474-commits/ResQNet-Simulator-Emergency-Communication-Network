#include <iostream>
#include "Metrics.h"
using namespace std;

int main() {
    // NOTE: every number below is SAMPLE/TEST data typed by hand.
    // They are NOT measurements from the ResQNet simulator.
    // totalLinks = 12 here is simply active + failed from the original sample.
    NetworkScenario normal = {
        "Normal Network",
        100, 100,
        10, 0,
        12, 0,
        -1, 12      // routeChanges not recorded, totalLinks 12 (= 12 active + 0 failed)
    };

    NetworkScenario failure = {
        "Network Failure",
        100, 55,
        10, 3,
        8, 4,
        -1, 12      // routeChanges not recorded, totalLinks 12 (= 8 active + 4 failed)
    };

    // SAMPLE static-vs-adaptive pair: same traffic and same failures.
    // Static route changes were not recorded, so they stay at -1.
    NetworkScenario staticRun   = { "Static (sample)",   100, 55, 10, 3, 8, 4, -1, 12 };
    NetworkScenario adaptiveRun = { "Adaptive (sample)", 100, 82, 10, 3, 8, 4,  4, 12 };

    cout << "========================================\n";
    cout << "       ResQNet Network Analyzer\n";
    cout << "   (all values below are SAMPLE data)\n";
    cout << "========================================\n";

    displayReport(normal);
    displayReport(failure);
    compareScenarios(normal, failure);
    compareRouting(staticRun, adaptiveRun);

    cout << "\nJSON preview of one result (future frontend format):\n"
         << toJson(calculateMetrics(adaptiveRun)) << '\n';

    cout << "\nAnalysis complete.\n";
    return 0;
}