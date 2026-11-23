# ResQNet Simulator – Adaptive Emergency Communication Network

An interactive network simulation system that demonstrates how emergency communication networks can maintain connectivity when nodes or links fail, or when network links become congested.

ResQNet combines a **C++ simulation backend** with an **HTML, CSS, and JavaScript frontend** to visualize network conditions, simulate failures, calculate alternate routes, and compare static and adaptive routing behaviour.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Problem Statement](#2-problem-statement)
3. [Objectives](#3-objectives)
4. [Key Features](#4-key-features)
5. [Technology Stack](#5-technology-stack)
6. [System Architecture](#6-system-architecture)
7. [Network Topology](#7-network-topology)
8. [Routing Algorithm](#8-routing-algorithm)
9. [Failure Detection and Recovery](#9-failure-detection-and-recovery)
10. [Congestion Management](#10-congestion-management)
11. [Project Structure](#11-project-structure)
12. [Installation and Setup](#12-installation-and-setup)
13. [Running the Project](#13-running-the-project)
14. [API Reference](#14-api-reference)
15. [Demonstration Scenarios](#15-demonstration-scenarios)
16. [Performance Metrics](#16-performance-metrics)
17. [Testing](#17-testing)
18. [Current Limitations](#18-current-limitations)
19. [Future Enhancements](#19-future-enhancements)
20. [Contributors](#20-contributors)

---

## 1. Project Overview

ResQNet Simulator is designed to demonstrate adaptive routing in emergency communication networks.

During disasters and emergencies, communication infrastructure may become unreliable due to:

- Network node failures.
- Communication link failures.
- Network congestion.
- Changes in network availability.

A conventional static routing system may continue using its original route even when that route becomes unavailable.

ResQNet demonstrates an adaptive approach. It monitors changes in the simulated network, detects failures, recalculates available routes, and selects an alternative path when possible.

The project provides a graphical network topology where users can interact with nodes and links, trigger network events, and observe routing changes.

**Important:** ResQNet is a network simulator and educational prototype. It does not directly control real emergency communication infrastructure.

## 2. Problem Statement

Emergency communication networks must remain reliable even when parts of their infrastructure become unavailable.

Traditional fixed-route communication can be disrupted when a node or communication link fails. Congestion can also make an otherwise available route less suitable.

The project addresses this problem by demonstrating a network model that can:

1. Represent communication infrastructure using nodes and links.
2. Find an initial route between a source and destination.
3. Simulate node failures and link failures.
4. Detect failures through a heartbeat-based mechanism.
5. Calculate alternative routes using Dijkstra's algorithm.
6. Account for congestion while calculating route costs.
7. Compare static and adaptive routing performance.

## 3. Objectives

- Develop a graph-based emergency communication network model.
- Implement shortest-path routing using Dijkstra's algorithm.
- Simulate node and link failures.
- Implement heartbeat-based failure detection.
- Support link congestion simulation.
- Demonstrate automatic alternate-route selection.
- Simulate packet transmission and calculate delivery statistics.
- Visualize network states and routing changes.
- Compare static and adaptive routing behaviour.

## 4. Key Features

### 4.1 Interactive Network Visualization

- Displays the network as interconnected nodes and links.
- Represents communication points such as hospitals, towers, and emergency centres.
- Highlights the currently selected route.
- Distinguishes failed and congested links visually.
- Allows users to select network elements for event simulation.
- Supports dragging nodes to rearrange the visualization.

### 4.2 Node Failure Simulation

Users can simulate the failure of a network node.

When a node fails:
- It becomes unavailable for routing.
- The failure detection mechanism processes the change.
- Adaptive routing searches for another available path.
- The updated route is reflected in the interface.

### 4.3 Link Failure Simulation

Users can disable a communication link between two nodes.

The routing engine avoids unavailable links and searches for an alternate route when one exists.

### 4.4 Automatic Adaptive Routing

When adaptive routing is enabled, the simulator recalculates the route when network conditions change.

The new route is selected based on the current effective link costs and network availability.

### 4.5 Congestion Simulation

Users can increase the simulated congestion load of a communication link.

Congestion increases the effective routing cost of that link. The routing algorithm can consequently select a less congested alternative.

Users can also clear congestion and observe the original route being selected again.

### 4.6 Heartbeat-Based Failure Detection

The backend uses a heartbeat mechanism to track network node availability.

The configured failure detector uses a missed-heartbeat threshold of three and a recovery threshold of one successful heartbeat.

### 4.7 Packet Transmission

The simulator supports simulated emergency packet transmission and tracks:

- Packets sent.
- Packets delivered.
- Packets lost.
- Delivery percentage.
- Packet-loss percentage.

### 4.8 Static vs Adaptive Comparison

The comparison functionality evaluates static and adaptive routing under a simulated network scenario.

It provides a way to observe differences in packet delivery, packet loss, route changes, and other available simulation metrics.

### 4.9 Event Logging

Network events are recorded in an event log so users can follow the sequence of failures, restorations, and routing changes.

### 4.10 Predefined Scenarios

The interface supports predefined simulation scenarios for demonstrating different network conditions.

---

## 5. Technology Stack

| Component | Technology |
|---|---|
| Frontend | HTML5 |
| Styling | CSS3 |
| Frontend logic | JavaScript |
| Backend | C++ |
| Routing algorithm | Dijkstra's algorithm |
| Backend HTTP server | cpp-httplib |
| JSON processing | nlohmann/json |
| Development environment | Visual Studio Code |
| Compiler | MSYS2 UCRT64 g++ |
| Version control | Git and GitHub |

### Backend dependencies

**cpp-httplib**

Used to create the HTTP server and expose API endpoints for communication between the frontend and backend.

**nlohmann/json**

Used to parse incoming JSON requests and generate JSON responses.

---

## 6. System Architecture

ResQNet follows a frontend-backend architecture.

The frontend handles user interaction and network visualization, while the C++ backend handles simulation logic, network state, routing, and performance calculations.

### Architecture Overview

```text
             USER
               |
               v
      HTML / CSS / JavaScript
               |
               v
       Frontend API Layer
               |
          HTTP + JSON
               |
               v
       C++ Backend Server
               |
       Simulation Controller
               |
      +--------+---------+
      |        |         |
      v        v         v
   Network  Failure   Routing
    Model   Detection  Engine
      |        |         |
      +--------+---------+
               |
               v
       Metrics and Events
               |
               v
        JSON Responses
               |
               v
       Frontend Visualization
```

### Main Backend Modules

**Network Model**

- Stores network nodes and links.
- Maintains node and link status.
- Stores link cost and congestion information.
- Provides network information to the routing engine.

**Routing Module**

- Implements Dijkstra's shortest-path algorithm.
- Calculates route costs.
- Finds alternate paths.
- Supports static and adaptive routing behaviour.

**Failure Detection Module**

- Uses heartbeat information to identify unavailable nodes.
- Applies configured failure and recovery thresholds.

**Failure Simulation Module**

- Simulates node failures.
- Simulates link failures.
- Restores failed network components.
- Supports restoration operations.

**Simulation Controller**

Coordinates the network model, failure handling, routing, packet simulation, and related operations.

**Metrics Module**

Tracks simulation results such as packet delivery, packet loss, and route changes.

**Backend Server**

Exposes HTTP endpoints and exchanges JSON data with the frontend.

---

## 7. Network Topology

The initial demonstration network contains six nodes and six communication links.

### Nodes

| Node | Identifier | Role |
|---|---|---|
| A | 0 | Hospital |
| B | 1 | Tower |
| C | 2 | Tower |
| D | 3 | Emergency Center |
| E | 4 | Tower |
| F | 5 | Tower |

### Links

| Link | Cost |
|---|---:|
| A-B | 2 |
| B-C | 2 |
| C-D | 2 |
| A-E | 4 |
| E-F | 2 |
| F-D | 2 |

### Available Routes

**Primary route:**

```text
A → B → C → D
```

Total cost:

```text
2 + 2 + 2 = 6
```

**Alternative route:**

```text
A → E → F → D
```

Total cost:

```text
4 + 2 + 2 = 8
```

The primary route is initially preferred because it has a lower total cost.

If a node or link on the primary route becomes unavailable, the adaptive routing mechanism can select the alternative route.

---

## 8. Routing Algorithm

ResQNet uses **Dijkstra's shortest-path algorithm** to calculate routes through the network.

Dijkstra's algorithm finds a minimum-cost path from a source node to a destination node when the edge costs are non-negative.

### Basic Working

1. Initialize the source node distance to zero.
2. Initialize the remaining node distances to infinity.
3. Select the unvisited node with the smallest known distance.
4. Examine its available neighbours.
5. Update a neighbour's distance when a shorter path is found.
6. Repeat until the destination is reached or no reachable nodes remain.
7. Reconstruct the final route using predecessor information.

### Relaxation Condition

The distance to a neighbouring node is updated when:

```cpp
if (dist[u] + weight < dist[v])
{
    dist[v] = dist[u] + weight;
}
```

### Static Routing

Static routing retains the initially selected route, even if a later network event makes that route unusable.

### Adaptive Routing

Adaptive routing recalculates the route using the current network state.

It can avoid:
- Failed nodes.
- Failed links.
- Links with increased congestion costs.

This allows the simulator to demonstrate how adaptive routing can improve communication continuity.

---

## 9. Failure Detection and Recovery

ResQNet uses simulated heartbeat-based failure detection.

A heartbeat represents a periodic indication that a network node is functioning.

### Failure Detection

The configured failure detector uses:

- Miss threshold: 3 missed heartbeats.
- Recovery threshold: 1 successful heartbeat.

When the configured failure threshold is reached, the detector can mark the node as failed.

### Recovery

When a previously failed node starts responding again and meets the recovery threshold, the detector can mark it as available.

### Adaptive Recovery Flow

```text
Network Event
      |
      v
Update Network State
      |
      v
Check Current Route
      |
      v
Is Current Route Valid?
      |
   +--+--+
   |     |
  Yes    No
   |     |
Continue  Recalculate Route
         |
         v
    Alternate Path?
         |
      +--+--+
      |     |
     Yes    No
      |     |
  Use New   Report
   Route   No Route
```

The heartbeat mechanism is simulated within the project. It does not monitor physical network equipment.

---

## 10. Congestion Management

Congestion is represented by a load value associated with a communication link.

The backend calculates an effective link cost using:

```cpp
effectiveCost =
    originalCost * (1.0 + congestionLoad / 50.0);
```

This makes heavily congested links less attractive during route calculation.

### Example

For a link with an original cost of 2 and a congestion load of 100:

```text
Effective cost = 2 × (1 + 100/50)

Effective cost = 2 × 3

Effective cost = 6
```

The original primary route costs 6, so after congestion is applied to A-B, its total route cost becomes:

```text
6 + 2 + 2 = 10
```

The alternative route remains:

```text
4 + 2 + 2 = 8
```

Therefore, adaptive routing can select:

```text
A → E → F → D
```

When congestion is cleared, the original link cost is restored and the lower-cost route becomes:

```text
A → B → C → D
```

The frontend currently accepts congestion load values from 1 to 100.

---

## 11. Project Structure

The main project files are organized as follows:

```text
self-healing-network-simulator/
│
├── frontend/
│   ├── index.html
│   │
│   ├── css/
│   │   └── style.css
│   │
│   └── js/
│       ├── api.js
│       ├── app.js
│       ├── ui.js
│       └── map.js
│
├── include/
│   ├── NetworkModel.h
│   ├── SimulationController.h
│   ├── FailureSimulator.h
│   ├── FailureDetector.h
│   ├── Routing.h
│   └── Metrics.h
│
├── src/
│   └── ResQNetServer.cpp
│
├── third_party/
│   ├── httplib.h
│   └── nlohmann/
│       └── json.hpp
│
├── test.cpp
├── ResQNetServer.exe
└── README.md
```

Additional test files may be present in the repository.

**Note:** The executable is generated during compilation. It does not need to be committed to the repository.

---

## 12. Installation and Setup

### Prerequisites

Install the following:

1. Visual Studio Code.
2. Git.
3. MSYS2 with the UCRT64 environment.
4. A modern C++ compiler (g++).
5. A web browser.
6. VS Code Live Server extension.

The project uses C++11-compatible compilation.

### Step 1: Clone the Repository

```bash
git clone <YOUR_GITHUB_REPOSITORY_URL>
```

Navigate into the project:

```bash
cd self-healing-network-simulator
```

Replace the placeholder with the actual repository URL.

### Step 2: Open the Project

Open the project folder in Visual Studio Code.

### Step 3: Verify Dependencies

Ensure the following files exist:

```text
third_party/httplib.h
third_party/nlohmann/json.hpp
```

The project uses these local header dependencies during compilation.

---

## 13. Running the Project

The project requires the C++ backend and frontend to run separately.

### Step 1: Start the Backend

Open the **MSYS2 UCRT64 terminal**.

Navigate to the project directory:

```bash
cd /d/GitHub/self-healing-network-simulator
```

Compile the backend:

```bash
g++ -std=c++11 -Wall -Wextra \
-Iinclude -Ithird_party \
src/ResQNetServer.cpp \
-o ResQNetServer.exe -lws2_32
```

Run the server:

```bash
./ResQNetServer.exe
```

The backend runs on:

```text
http://localhost:8080
```

Keep this terminal open while using the frontend.

### Step 2: Start the Frontend

1. Open `frontend/index.html` in VS Code.
2. Right-click the file.
3. Select **Open with Live Server**.

The frontend will open in your browser, generally at:

```text
http://127.0.0.1:5500/frontend/index.html
```

The exact port may vary depending on your Live Server configuration.

### Step 3: Verify the Backend

Open:

```text
http://localhost:8080/
```

The health endpoint should return a message indicating that the ResQNet C++ backend is running.

---

## 14. API Reference

The C++ backend exposes REST-style HTTP endpoints that exchange JSON data with the frontend.

### GET Endpoints

| Endpoint | Purpose |
|---|---|
| `/` | Backend health check |
| `/api/network` | Retrieve network topology and link information |
| `/api/simulation` | Retrieve simulation state |
| `/api/metrics` | Retrieve performance metrics |
| `/api/routes` | Retrieve routing information |
| `/api/events` | Retrieve event history |

### POST Endpoints

| Endpoint | Purpose |
|---|---|
| `/api/fail-node` | Simulate a node failure |
| `/api/restore-node` | Restore a node |
| `/api/fail-link` | Simulate a link failure |
| `/api/restore-link` | Restore a link |
| `/api/congest-link` | Apply congestion to a link |
| `/api/clear-congestion` | Clear link congestion |
| `/api/heartbeat` | Process heartbeat detection |
| `/api/restore-all` | Restore network components |
| `/api/reset` | Reset simulation state |
| `/api/packets` | Simulate packet transmission |
| `/api/routing` | Enable or disable adaptive routing |
| `/api/scenario` | Execute a predefined scenario |
| `/api/compare` | Compare static and adaptive routing |

### Example Requests

**Fail a node**

```json
{
  "id": 1
}
```

Send to:

```text
POST /api/fail-node
```

**Restore a node**

```json
{
  "id": 1
}
```

Send to:

```text
POST /api/restore-node
```

**Fail a link**

```json
{
  "from": 0,
  "to": 1
}
```

Send to:

```text
POST /api/fail-link
```

**Apply congestion**

```json
{
  "from": 0,
  "to": 1,
  "load": 100
}
```

Send to:

```text
POST /api/congest-link
```

**Clear congestion**

```json
{
  "from": 0,
  "to": 1
}
```

Send to:

```text
POST /api/clear-congestion
```

**Transmit packets**

```json
{
  "count": 20
}
```

Send to:

```text
POST /api/packets
```

**Change routing mode**

```json
{
  "adaptive": true
}
```

Send to:

```text
POST /api/routing
```

The backend expects JSON request bodies. Successful responses and error details are returned by the server.

---

## 15. Demonstration Scenarios

The following scenarios can be used to demonstrate the project.

### Scenario 1: Normal Communication

- Source: A (Hospital).
- Destination: D (Emergency Center).
- Adaptive routing: Enabled.
- No failures or congestion.

Expected route:

```text
A → B → C → D
```

Expected route cost: 6.

### Scenario 2: Node Failure

1. Start normal communication.
2. Fail node B.
3. Allow heartbeat-based detection to process the failure.
4. Observe the new route.

Expected adaptive route:

```text
A → E → F → D
```

### Scenario 3: Link Failure

1. Start normal communication.
2. Fail link A-B.
3. Observe the route recalculation.

Expected adaptive route:

```text
A → E → F → D
```

### Scenario 4: Congestion

1. Select the manual event option.
2. Choose **Congest a link**.
3. Select A-B.
4. Set congestion load to 100.
5. Trigger the event.

Expected adaptive route:

```text
A → E → F → D
```

### Scenario 5: Clear Congestion

1. Select **Clear congestion**.
2. Select A-B.
3. Trigger the event.

Expected route:

```text
A → B → C → D
```

### Scenario 6: Restore Everything

Use the restore operation to return the network to its initial state before another demonstration.

---

## 16. Performance Metrics

The simulator provides performance information to help evaluate network behaviour.

| Metric | Description |
|---|---|
| Packets Sent | Total packets submitted for transmission |
| Packets Delivered | Packets successfully delivered |
| Packets Lost | Packets that did not reach the destination |
| Delivery Rate | Percentage of packets successfully delivered |
| Packet Loss Rate | Percentage of packets lost |
| Route Changes | Number of times the active route changes |
| Route Cost | Total cost of the selected route |
| Recovery Time | Recovery-related metric exposed by the comparison interface |

### Metric Calculations

Delivery rate:

```text
(Delivered Packets / Sent Packets) × 100
```

Packet loss rate:

```text
(Lost Packets / Sent Packets) × 100
```

These are simulated results based on the project's network model and assumptions, not measurements of a real emergency communication network.

**Recovery-time note:** The current comparison implementation uses a placeholder value for recovery time. It should not be interpreted as a measured recovery duration.

---

## 17. Testing

The project has been tested through backend compilation, API requests, and frontend integration.

The following behaviours have been verified during development:

- Backend server starts successfully.
- Front