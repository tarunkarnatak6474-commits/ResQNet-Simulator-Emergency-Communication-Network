
#include <iostream>
#include <iomanip>
using namespace std;

struct NetworkScenario {
    string name;
    int packetsSent;
    int packetsDelivered;
    int totalNodes;
    int failedNodes;
    int activeLinks;
    int failedLinks;
};

void displayReport(NetworkScenario s) {
    int packetsLost = s.packetsSent - s.packetsDelivered;

    double deliveryRatio =
        (double)s.packetsDelivered / s.packetsSent * 100;

    double packetLoss =
        (double)packetsLost / s.packetsSent * 100;

    double nodeAvailability =
        (double)(s.totalNodes - s.failedNodes)
        / s.totalNodes * 100;

    cout << "\n========================================\n";
    cout << "Scenario: " << s.name << '\n';
    cout << "========================================\n";

    cout << "Packets sent       : " << s.packetsSent << '\n';
    cout << "Packets delivered  : " << s.packetsDelivered << '\n';
    cout << "Packets lost       : " << packetsLost << '\n';

    cout << fixed << setprecision(2);

    cout << "Delivery ratio     : " << deliveryRatio << "%\n";
    cout << "Packet loss        : " << packetLoss << "%\n";

    cout << "\n--- Network Status ---\n";
    cout << "Total nodes        : " << s.totalNodes << '\n';
    cout << "Failed nodes       : " << s.failedNodes << '\n';
    cout << "Operational nodes  : "
         << s.totalNodes - s.failedNodes << '\n';

    cout << "Active links       : " << s.activeLinks << '\n';
    cout << "Failed links       : " << s.failedLinks << '\n';
    cout << "Node availability  : " << nodeAvailability << "%\n";

    cout << "\nPerformance: ";

    if (deliveryRatio >= 95)
        cout << "EXCELLENT\n";
    else if (deliveryRatio >= 80)
        cout << "GOOD\n";
    else if (deliveryRatio >= 50)
        cout << "MODERATE\n";
    else
        cout << "POOR\n";
}

int main() {
    NetworkScenario normal = {
        "Normal Network",
        100, 100,
        10, 0,
        12, 0
    };

    NetworkScenario failure = {
        "Network Failure",
        100, 55,
        10, 3,
        8, 4
    };

    cout << "========================================\n";
    cout << "       ResQNet Network Analyzer\n";
    cout << "========================================\n";

    displayReport(normal);
    displayReport(failure);

    cout << "\n========================================\n";
    cout << "          SCENARIO COMPARISON\n";
    cout << "========================================\n";

    double normalRatio =
        (double)normal.packetsDelivered / normal.packetsSent * 100;

    double failureRatio =
        (double)failure.packetsDelivered / failure.packetsSent * 100;

    cout << fixed << setprecision(2);

    cout << "Normal delivery ratio  : "
         << normalRatio << "%\n";

    cout << "Failure delivery ratio : "
         << failureRatio << "%\n";

    cout << "Delivery ratio decrease: "
         << normalRatio - failureRatio << "% points\n";

    cout << "\nAnalysis complete.\n";

    return 0;
}
