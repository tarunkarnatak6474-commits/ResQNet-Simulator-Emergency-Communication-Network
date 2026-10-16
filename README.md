
# ResQNet Simulator – Emergency Communication Network

## Overview

ResQNet Simulator is a project focused on simulating an emergency communication network. Its goal is to demonstrate how communication routes can respond to network failures and help maintain connectivity during emergencies.

## Current Features

* Interactive frontend built using HTML, CSS, and JavaScript.
* Visual representation of network nodes and links.
* Controls for emergency communication requests.
* Simulation controls for node and link failures.
* Display of network routes, events, and performance metrics.
* Mock API for demonstrating frontend behavior.

## Technologies Used

* **Frontend:** HTML, CSS, JavaScript
* **Backend:** C++ (planned)
* **Version Control:** Git and GitHub

## Project Structure

```text
ResQNet-Simulator-Emergency-Communication-Network/
├── frontend/    # User interface
│   ├── index.html
│   ├── css/
│   └── js/
├── include/     # C++ header files
├── src/         # C++ source files
├── tests/       # Testing files
├── data/        # Project data
├── README.md
├── .gitignore
└── .gitattributes
```

## Planned Development

1. Implement the C++ network model.
2. Implement Dijkstra's shortest-path algorithm.
3. Add network failure detection and handling.
4. Implement adaptive routing and rerouting.
5. Add packet simulation and performance metrics.
6. Integrate the frontend with the C++ backend.

## Project Status

The initial repository structure and frontend foundation have been created. The frontend currently uses a mock API. The C++ simulation backend and its integration with the frontend are planned for future development.

## Running the Frontend

### Requirements
- Python 3 installed, or another local HTTP server.

### Steps
1. Clone or download the repository.
2. Open a terminal in the project directory.
3. Navigate to the `frontend` folder.
4. Start the local server:

   ```bash
   python -m http.server 8000
   ```

5. Open `http://localhost:8000` in your browser.

The frontend currently uses a mock API for demonstration and testing.

## Planned C++ Backend

The C++ backend will handle the core network simulation logic. The existing HTML, CSS, and JavaScript frontend will remain responsible for visualization and user interaction.

Planned backend components:

- **Network Model:** Represents network nodes and communication links.
- **Routing Module:** Calculates routes using Dijkstra's algorithm.
- **Failure Management:** Handles failed nodes and links.
- **Adaptive Routing:** Finds alternative routes after network failures.
- **Simulation and Metrics:** Simulates packet delivery and measures network performance.

## Development Roadmap

1. Complete and test the frontend prototype.
2. Define the C++ network model.
3. Implement Dijkstra routing.
4. Add node and link failure handling.
5. Implement adaptive rerouting.
6. Add packet simulation and performance metrics.
7. Integrate the frontend with the C++ backend.
8. Test the complete simulator using different failure scenarios.