/* =====================================================================
   api.js  -  the ONLY file that knows where data comes from.

   BACKEND CONTRACT (the future C++ backend must return these shapes).
   Every method is async and rejects with Error(message) on a bad request.

   getNetworkState()   -> { nodes:[{id,name,role,x,y,active}],
                            links:[{id,from,to,cost,bandwidth,delay,load,active,congested}] }
   getSimulationState()-> { status:"IDLE"|"ACTIVE"|"STOPPED"|"NO_ROUTE",
                            source, destination, adaptiveRouting,
                            currentRoute:[ids]|null, initialRoute:[ids]|null,
                            alternateRoute:[ids]|null, packetCount, finalMessage }
   getMetrics()        -> { sent, delivered, lost, deliveryRate, packetLoss, pathDelay,
                            throughput, routeChanges, recoveryTime }
   getRoutes()         -> [{ name, path:[ids], cost, delay, status }]
                          status: "In use" | "Valid, unused" | "Broken, still assigned" | "Invalid"
   getEventLog()       -> [{ time:"10:04", message, type:"info"|"event"|"detect"|"error"|"ok" }]
   getComparison()     -> { scenario, static:{...}, adaptive:{...} } | null
                          each side: { delivered, sent, deliveryRate, packetLoss,
                                       routeChanges, recoveryTime, finalState }
   listScenarios()     -> [{ id, label }]

   Commands:
   createEmergencyRequest({source,destination,priority,packetCount,adaptiveRouting})
   transmitBatch(batchSize) -> { route:[ids]|null, delivered, lost }  (used for packet animation)
   triggerEvent({type,target,load})  type: failNode|restoreNode|failLink|restoreLink|congestLink|clearCongestion
   restoreAll(), resetSimulation()
   runScenario(scenarioId) -> same shape as transmitBatch
   addNode({name,role}), addLink({from,to,cost,bandwidth,delay})

   TO CONNECT THE C++ BACKEND: write a CppBackendAPI class with the same
   methods (each one does fetch("/api/...") to the C++ server) and change the
   last line of this file to:  const api = new CppBackendAPI();
   ===================================================================== */

/* ---------------------------------------------------------------------
   MockAPI: fake backend so the UI can be demonstrated.
   It contains NO real routing algorithm. Routes come from a fixed list of
   candidate paths; the "network behaviour" is a few canned rules.
   --------------------------------------------------------------------- */
const MOCK_PATHS = [["A", "B", "C", "D"], ["A", "E", "F", "D"]];   // fixture, not Dijkstra

const MOCK_SCENARIOS = {
  main:  { label: "Node failure with alternate path (main demo)", steps: [["failNode", "B"]] },
  test1: { label: "Test 1: Normal communication",                 steps: [] },
  test3: { label: "Test 3: Link failure with alternate path",     steps: [["failLink", "B-C"]] },
  test4: { label: "Test 4: No alternate path",                    steps: [["failNode", "B"], ["failNode", "E"]] },
  test5: { label: "Test 5: Congestion",                           steps: [["congestLink", "A-B", 90]] },
  test6: { label: "Test 6: Multiple events, two reroutes",        steps: [["failNode", "B"], ["restoreNode", "B"], ["failNode", "F"]] }
};

class MockAPI {
  constructor() { this._init(); }

  _init() {
    this.clock = 0;
    this.events = [];
    this.nodes = [
      { id: "A", name: "A", role: "Hospital", x: 90, y: 230 },
      { id: "B", name: "B", role: "Tower", x: 300, y: 100 },
      { id: "C", name: "C", role: "Tower", x: 560, y: 100 },
      { id: "D", name: "D", role: "Emergency center", x: 810, y: 230 },
      { id: "E", name: "E", role: "Tower", x: 300, y: 360 },
      { id: "F", name: "F", role: "Tower", x: 560, y: 360 }
    ].map(n => ({ ...n, active: true }));
    const L = (from, to, cost, bandwidth, delay) =>
      ({ id: from + "-" + to, from, to, cost, bandwidth, delay, load: 0, active: true, congested: false });
    this.links = [L("A","B",2,100,20), L("B","C",2,80,25), L("C","D",2,100,20),
                  L("A","E",4,60,40), L("E","F",2,80,25), L("F","D",2,100,20)];
    this.sim = { status: "IDLE", source: null, destination: null, adaptiveRouting: true,
                 currentRoute: null, initialRoute: null, alternateRoute: null,
                 packetCount: 0, finalMessage: "No simulation yet. Create a request or run a scenario." };
    this.m = { sent: 0, delivered: 0, routeChanges: 0, recoveryTime: 0 };
    this._log("Emergency network created.", "info");
  }

  /* ---- small mock helpers ---- */
  _log(message, type) {
    const h = 10 + Math.floor(this.clock / 60), m = this.clock % 60;
    this.events.push({ time: h + ":" + String(m).padStart(2, "0"), message, type });
    this.clock++;
  }
  _copy(x) { return JSON.parse(JSON.stringify(x)); }
  _node(id) { return this.nodes.find(n => n.id === id); }
  _linkBetween(a, b) { return this.links.find(l => (l.from === a && l.to === b) || (l.from === b && l.to === a)); }
  _pathOk(p) {
    return p.every(id => this._node(id).active) &&
      p.slice(1).every((id, i) => { const l = this._linkBetween(p[i], id); return l && l.active; });
  }
  _candidates(s, d) {   // slice fixture paths so they start at s and end at d
    return MOCK_PATHS.map(p => { const i = p.indexOf(s), j = p.indexOf(d); return i >= 0 && j > i ? p.slice(i, j + 1) : null; })
                     .filter(Boolean);
  }
  _info(p) {
    const ls = p.slice(1).map((id, i) => this._linkBetween(p[i], id));
    return { cost: ls.reduce((a, l) => a + l.cost, 0), delay: ls.reduce((a, l) => a + l.delay, 0),
             bottleneck: Math.min(...ls.map(l => l.bandwidth)), congested: ls.some(l => l.congested) };
  }
  _arrow(p) { return p.join(" → "); }

  /* After any event: pretend the backend detects a break and reacts. */
  _react() {
    const s = this.sim;
    if (s.status === "IDLE") return;
    if (s.currentRoute && this._pathOk(s.currentRoute)) {
      if (s.status === "STOPPED") { s.status = "ACTIVE"; this._log("Route works again. Communication resumed.", "ok"); }
      return;
    }
    if (s.currentRoute) this._log("FAILURE DETECTED: route " + this._arrow(s.currentRoute) + " is broken.", "detect");
    if (!s.adaptiveRouting) {
      s.status = "STOPPED";
      s.finalMessage = "Route broken. Adaptive routing is off, so communication has stopped.";
      this._log(s.finalMessage, "error"); return;
    }
    this._log("Searching for alternate route.", "info");
    const alt = this._candidates(s.source, s.destination).find(p => this._pathOk(p));
    if (!alt) {
      s.status = "NO_ROUTE"; s.currentRoute = null;
      s.finalMessage = "No alternate route exists. Emergency communication cannot continue.";
      this._log(s.finalMessage, "error"); return;
    }
    s.currentRoute = alt; s.status = "ACTIVE";
    this.m.routeChanges++; this.m.recoveryTime = 160;           // canned value
    s.finalMessage = "Communication maintained through alternate route.";
    this._log("Alternate route found: " + this._arrow(alt) + ".", "ok");
    this._log("Emergency communication resumed.", "ok");
  }

  /* ---- queries ---- */
  async getNetworkState() { return this._copy({ nodes: this.nodes, links: this.links }); }
  async getSimulationState() { return this._copy(this.sim); }
  async getEventLog() { return this._copy(this.events); }
  async listScenarios() { return Object.keys(MOCK_SCENARIOS).map(id => ({ id, label: MOCK_SCENARIOS[id].label })); }

  async getMetrics() {
    const { sent, delivered, routeChanges, recoveryTime } = this.m, s = this.sim;
    const info = s.currentRoute ? this._info(s.currentRoute) : null;
    return { sent, delivered, lost: sent - delivered,
             deliveryRate: sent ? Math.round(delivered / sent * 100) : 0,
             packetLoss: sent ? Math.round((sent - delivered) / sent * 100) : 0,
             pathDelay: info ? info.delay : 0,
             throughput: info ? Math.round(info.bottleneck * 0.78) : 0,
             routeChanges, recoveryTime };
  }

  async getRoutes() {
    const s = this.sim;
    if (!s.initialRoute) return [];
    const row = (name, p) => {
      const inUse = s.currentRoute && p.join() === s.currentRoute.join(), ok = this._pathOk(p), i = this._info(p);
      const status = inUse ? (ok ? "In use" : "Broken, still assigned") : (ok ? "Valid, unused" : "Invalid");
      return { name, path: p, cost: i.cost, delay: i.delay, status };
    };
    const rows = [row("Initial route", s.initialRoute)];
    if (s.alternateRoute) rows.push(row("Alternate route", s.alternateRoute));
    return rows;
  }

  async getComparison() {   // canned numbers: the real backend runs both modes with one seed
    const s = this.sim, m = await this.getMetrics();
    if (!s.initialRoute || !m.sent) return null;
    const side = (d, r, rc, rt, f) => ({ delivered: d, sent: m.sent, deliveryRate: Math.round(d / m.sent * 100),
                                         packetLoss: 100 - Math.round(d / m.sent * 100), routeChanges: rc, recoveryTime: rt, finalState: f });
    const adaptive = side(m.delivered, m.routeChanges, m.recoveryTime, s.finalMessage);
    const broke = m.routeChanges > 0 || s.status !== "ACTIVE";
    const staticSide = broke ? side(Math.round(m.sent * 0.48), 0, 0, "Route broken, communication stopped.") : { ...adaptive };
    return { scenario: "Current simulation", static: staticSide, adaptive };
  }

  /* ---- commands ---- */
  async createEmergencyRequest(r) {
    const { source, destination, priority, packetCount, adaptiveRouting } = r;
    if (!this._node(source) || !this._node(destination)) throw new Error("Choose a valid source and destination.");
    if (source === destination) throw new Error("Source and destination must be different.");
    const routes = this._candidates(source, destination), s = this.sim;
    const first = routes.find(p => this._pathOk(p));
    this.m = { sent: 0, delivered: 0, routeChanges: 0, recoveryTime: 0 };
    Object.assign(s, { source, destination, priority, packetCount, adaptiveRouting,
      initialRoute: first || null, currentRoute: first || null,
      alternateRoute: first ? routes.find(p => p.join() !== first.join()) || null : null });
    this._log("Emergency request created: " + source + " → " + destination + ".", "info");
    if (!first) {
      s.status = "NO_ROUTE"; s.finalMessage = "No route exists. Emergency communication cannot start.";
      this._log(s.finalMessage, "error"); return;
    }
    s.status = "ACTIVE"; s.finalMessage = "Communication normal on the initial route.";
    this._log("Initial route selected: " + this._arrow(first) + ".", "ok");
  }

  async transmitBatch(batchSize) {
    const s = this.sim, left = s.packetCount - this.m.sent;
    if (s.status === "IDLE") throw new Error("Create an emergency request first.");
    if (left <= 0) throw new Error("All packets of this request were already sent.");
    const n = Math.min(batchSize, left);
    let rate = 0;
    if (s.status === "ACTIVE" && s.currentRoute) rate = this._info(s.currentRoute).congested ? 0.9 : 0.98;
    const delivered = Math.round(n * rate);
    this.m.sent += n; this.m.delivered += delivered;
    this._log("Batch of " + n + " packets sent, " + delivered + " delivered.", delivered === n ? "ok" : "error");
    return { route: s.status === "ACTIVE" ? this._copy(s.currentRoute) : (s.currentRoute ? this._copy(s.currentRoute) : null),
             delivered, lost: n - delivered };
  }

  async triggerEvent({ type, target, load }) {
    const node = this._node(target), link = this.links.find(l => l.id === target);
    const needNode = /Node$/.test(type);
    if (!(needNode ? node : link)) throw new Error("Choose a valid target for this event.");
    if (type === "failNode") { node.active = false; this._log("Node " + target + " has failed.", "event"); }
    if (type === "restoreNode") { node.active = true; this._log("Node " + target + " restored.", "event"); }
    if (type === "failLink") { link.active = false; this._log("Link " + target + " has failed.", "event"); }
    if (type === "restoreLink") { link.active = true; this._log("Link " + target + " restored.", "event"); }
    if (type === "congestLink") { link.congested = true; link.load = load || 90; this._log("Link " + target + " is congested (" + link.load + "% load).", "event"); }
    if (type === "clearCongestion") { link.congested = false; link.load = 0; this._log("Congestion cleared on link " + target + ".", "event"); }
    this._react();
  }

  async restoreAll() {
    this.nodes.forEach(n => n.active = true);
    this.links.forEach(l => { l.active = true; l.congested = false; l.load = 0; });
    this._log("All nodes and links restored.", "event");
    this._react();
  }

  async resetSimulation() { this._init(); }

  async runScenario(id) {
    const sc = MOCK_SCENARIOS[id];
    if (!sc) throw new Error("Unknown scenario.");
    this._init();
    await this.createEmergencyRequest({ source: "A", destination: "D", priority: 2, packetCount: 100, adaptiveRouting: true });
    for (const [type, target, load] of sc.steps) await this.triggerEvent({ type, target, load });
    return this.transmitBatch(100);
  }

  async addNode({ name, role }) {
    if (!name) throw new Error("Node name is required.");
    if (this._node(name)) throw new Error("A node named " + name + " already exists.");
    const k = this.nodes.length - 6;
    this.nodes.push({ id: name, name, role: role || "Tower", x: 80 + (k * 110) % 740, y: 430, active: true });
    this._log("Node " + name + " added.", "info");
  }

  async addLink({ from, to, cost, bandwidth, delay }) {
    if (!this._node(from) || !this._node(to)) throw new Error("Choose both end nodes.");
    if (from === to) throw new Error("A link needs two different nodes.");
    if (this._linkBetween(from, to)) throw new Error("These nodes are already linked.");
    this.links.push({ id: from + "-" + to, from, to, cost, bandwidth, delay, load: 0, active: true, congested: false });
    this._log("Link " + from + "-" + to + " added.", "info");
  }
}

/* The rest of the UI only ever talks to this object. */

const NODE_INFO = {
  0: { id: "A", name: "A", role: "Hospital", x: 90, y: 230 },
  1: { id: "B", name: "B", role: "Tower", x: 300, y: 100 },
  2: { id: "C", name: "C", role: "Tower", x: 560, y: 100 },
  3: { id: "D", name: "D", role: "Emergency center", x: 810, y: 230 },
  4: { id: "E", name: "E", role: "Tower", x: 300, y: 360 },
  5: { id: "F", name: "F", role: "Tower", x: 560, y: 360 }
};

const LETTER_TO_ID = { A: 0, B: 1, C: 2, D: 3, E: 4, F: 5 };

const BACKEND_SCENARIOS = {
  main: {
    label: "Node failure with alternate path (main demo)",
    steps: [{ type: "failNode", id: 1 }]
  },
  test1: {
    label: "Test 1: Normal communication",
    steps: []
  },
  test3: {
    label: "Test 3: Link failure with alternate path",
    steps: [{ type: "failLink", from: 1, to: 2 }]
  },
  test4: {
    label: "Test 4: No alternate path",
    steps: [
      { type: "failNode", id: 1 },
      { type: "failNode", id: 4 }
    ]
  },
  test6: {
    label: "Test 6: Multiple events, two reroutes",
    steps: [
      { type: "failNode", id: 1 },
      { type: "restoreNode", id: 1 },
      { type: "failNode", id: 5 }
    ]
  }
};

class CppBackendAPI {
  constructor() {
    this.baseUrl = "http://localhost:8080";
    this.requestActive = false;
    this.packetCount = 0;
    this.initialRoute = null;
    this.finalMessage =
      "No simulation yet. Create a request or run a scenario.";
  }

  async _request(path, method = "GET", body = null) {
    const options = {
      method,
      headers: { "Content-Type": "application/json" }
    };

    if (body !== null) {
      options.body = JSON.stringify(body);
    }

    const response = await fetch(this.baseUrl + path, options);
    const raw = await response.text();

    let data = {};
    try {
      data = raw ? JSON.parse(raw) : {};
    } catch {
      data = { message: raw };
    }

    if (!response.ok) {
      throw new Error(data.message || "Backend request failed.");
    }

    return data;
  }

  _letter(id) {
    return NODE_INFO[id]?.id || String(id);
  }

  _number(letter) {
    const id = LETTER_TO_ID[letter];

    if (id === undefined) {
      throw new Error("Invalid node: " + letter);
    }

    return id;
  }

  _mapPath(path) {
    if (!Array.isArray(path)) return null;
    return path.map(id => this._letter(id));
  }

  // -------------------- QUERIES --------------------

  async getNetworkState() {
    const data = await this._request("/api/network");

    const nodes = data.nodes.map(n => ({
      ...NODE_INFO[n.id],
      active: n.active
    }));

    const links = data.links.map(l => {
      const from = this._letter(l.from);
      const to = this._letter(l.to);

      return {
        id: from + "-" + to,
        from,
        to,
        cost: l.cost,
        bandwidth: null,
        delay: "N/A",
        load: 0,
        active: l.active,
        congested: false
      };
    });

    return { nodes, links };
  }

  async getSimulationState() {
    const [sim, routes] = await Promise.all([
      this._request("/api/simulation"),
      this._request("/api/routes")
    ]);

    const currentRoute = this._mapPath(routes.currentRoute);

    let status = "IDLE";

    if (this.requestActive || sim.packetsSent > 0) {

  // Recover previous simulation after browser refresh.
  this.requestActive = true;

  if (!this.packetCount) {
    this.packetCount = sim.packetsSent;
  }

  if (!this.initialRoute) {
    this.initialRoute = currentRoute;
  }

  if (routes.routeUsable) {
    status = "ACTIVE";
  } else if (sim.adaptiveRouting) {
    status = "NO_ROUTE";
  } else {
    status = "STOPPED";
  }
}
    if (status === "NO_ROUTE") {
      this.finalMessage = "No usable route exists. Communication cannot continue.";
    } else if (status === "STOPPED") {
      this.finalMessage = "The route is broken. Communication has stopped.";
    } else if (status === "ACTIVE") {
      this.finalMessage = "Communication is active on the current route.";
    }

    return {
      status,
      source: this._letter(sim.source),
      destination: this._letter(sim.destination),
      adaptiveRouting: sim.adaptiveRouting,
      currentRoute,
      initialRoute: this.initialRoute,
      alternateRoute: null,
      packetCount: this.packetCount,
      finalMessage: this.finalMessage
    };
  }

  async getMetrics() {
    const m = await this._request("/api/metrics");

    const sent = m.packetsSent || 0;
    const delivered = m.packetsDelivered || 0;
    const lost = m.packetsLost || 0;

    return {
      sent,
      delivered,
      lost,
      deliveryRate: sent ? Math.round(delivered / sent * 100) : 0,
      packetLoss: sent ? Math.round(lost / sent * 100) : 0,
      pathDelay: "N/A",
      throughput: "N/A",
      routeChanges: m.routeChanges || 0,
      recoveryTime: "N/A"
    };
  }

  async getRoutes() {
    const r = await this._request("/api/routes");
    const path = this._mapPath(r.currentRoute);

    if (!path || path.length === 0) return [];

    return [{
      name: "Current route",
      path,
      cost: r.routeCost,
      delay: "N/A",
      status: r.routeUsable ? "In use" : "Broken, still assigned"
    }];
  }

 
async getEventLog() {
  const data = await this._request("/api/events");

  const formatEvent = (e, type) => {
    let message = e.description || "Network event";

    // Convert numeric node IDs into frontend labels.
    if (e.nodeId !== undefined && e.nodeId !== null) {
      message = message.replace(
        new RegExp("\\bNode\\s+" + e.nodeId + "\\b", "gi"),
        "Node " + this._letter(e.nodeId)
      );
    }

    // Convert numeric link endpoints into frontend labels.
    if (e.linkFrom !== undefined && e.linkTo !== undefined) {
      const from = this._letter(e.linkFrom);
      const to = this._letter(e.linkTo);

      message = message
        .replace(/\bLink\s+\d+\s*-\s*\d+\b/gi, "Link " + from + "-" + to)
        .replace(/\b\d+\s*-\s*\d+\b/g, from + "-" + to);
    }

    return {
      number: e.number || 0,
      message,
      type
    };
  };

  const injected = (data.injectedEvents || []).map(
    e => formatEvent(e, "event")
  );

  const detected = (data.detectedEvents || []).map(
    e => formatEvent(e, "detect")
  );

  return [...injected, ...detected]
    .sort((a, b) => a.number - b.number)
    .map((e, i) => ({
    time: "Event " + (i + 1),
    message: e.message,
    type: e.type
}));
}


  
  async getComparison() {
    const metrics = await this.getMetrics();

    // Do not compare until packets have been transmitted.
    if (metrics.sent <= 0) {
      return null;
    }

    // Ask the C++ backend to run independent tests.
    const comparison = await this._request(
      "/api/compare",
      "POST",
      { count: metrics.sent }
    );

    return comparison;
  }


  async listScenarios() {
    return Object.keys(BACKEND_SCENARIOS).map(id => ({
      id,
      label: BACKEND_SCENARIOS[id].label
    }));
  }

  // -------------------- COMMANDS --------------------

  async createEmergencyRequest(r) {
    if (r.source !== "A" || r.destination !== "D") {
      throw new Error(
        "The current C++ backend supports the fixed route A to D only."
      );
    }

    await this._request("/api/routing", "POST", {
      adaptive: !!r.adaptiveRouting
    });

    // Run 3 heartbeat rounds so node failures are detected immediately.
for (let i = 0; i < 3; i++) {
  await this._request("/api/heartbeat", "POST");
}

    const routes = await this._request("/api/routes");

    this.requestActive = true;
    this.packetCount = r.packetCount;
    this.initialRoute = this._mapPath(routes.currentRoute);
    this.finalMessage = routes.routeUsable
      ? "Communication normal on the initial route."
      : "No usable route exists.";

    return this.getSimulationState();
  }

  async transmitBatch(batchSize) {
    if (!this.requestActive) {
      throw new Error("Create an emergency request first.");
    }

    const sim = await this._request("/api/simulation");
    const remaining = this.packetCount - sim.packetsSent;

    if (remaining <= 0) {
      throw new Error("All packets of this request were already sent.");
    }

    const count = Math.min(batchSize, remaining);

    const result = await this._request("/api/packets", "POST", {
      count
    });

    await this._request("/api/heartbeat", "POST");

    const routes = await this._request("/api/routes");

    return {
      route: routes.routeUsable
        ? this._mapPath(routes.currentRoute)
        : null,
      delivered: result.delivered,
      lost: result.lost
    };
  }

  
async triggerEvent({ type, target, load }) {

  if (type === "failNode" || type === "restoreNode") {
    const id = this._number(target);

    await this._request(
      type === "failNode" ? "/api/fail-node" : "/api/restore-node",
      "POST",
      { id }
    );

  } else if (
    type === "failLink" ||
    type === "restoreLink" ||
    type === "congestLink" ||
    type === "clearCongestion"
  ) {
    const parts = target.split("-");

    if (parts.length !== 2) {
      throw new Error("Invalid link: " + target);
    }

    const from = this._number(parts[0]);
    const to = this._number(parts[1]);

    if (type === "congestLink") {
      await this._request("/api/congest-link", "POST", {
        from,
        to,
        load: Number(load || 90)
      });

    } else if (type === "clearCongestion") {
      await this._request("/api/clear-congestion", "POST", {
        from,
        to
      });

    } else {
      await this._request(
        type === "failLink" ? "/api/fail-link" : "/api/restore-link",
        "POST",
        { from, to }
      );
    }

  } else {
    throw new Error("Unsupported event: " + type);
  }

  // Heartbeat detection only for node/link failures and restoration
  if (
    type === "failNode" ||
    type === "restoreNode" ||
    type === "failLink" ||
    type === "restoreLink"
  ) {
    for (let i = 0; i < 3; i++) {
      await this._request("/api/heartbeat", "POST");
    }
  }
}


  async restoreAll() {
    await this._request("/api/restore-all", "POST");
    await this._request("/api/heartbeat", "POST");
  }

  async resetSimulation() {
    await this._request("/api/reset", "POST");

    this.requestActive = false;
    this.packetCount = 0;
    this.initialRoute = null;
    this.finalMessage =
      "No simulation yet. Create a request or run a scenario.";
  }

  async runScenario(id) {
    const scenario = BACKEND_SCENARIOS[id];

    if (!scenario) {
      throw new Error("Unknown or unsupported scenario.");
    }

    await this.resetSimulation();

    await this._request("/api/routing", "POST", {
      adaptive: true
    });

    const initial = await this._request("/api/routes");
    this.initialRoute = this._mapPath(initial.currentRoute);

    await this._request("/api/scenario", "POST", {
      name: scenario.label,
      steps: scenario.steps
    });

    await this._request("/api/heartbeat", "POST");

    this.requestActive = true;
    this.packetCount = 100;

    return this.transmitBatch(100);
  }

  async addNode() {
    throw new Error("Adding nodes is not supported by the C++ backend yet.");
  }

  async addLink() {
    throw new Error("Adding links is not supported by the C++ backend yet.");
  }
}

// The frontend now uses the real C++ backend.
const api = new CppBackendAPI();

