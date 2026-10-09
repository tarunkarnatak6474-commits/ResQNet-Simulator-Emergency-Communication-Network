
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