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
const api = new MockAPI();
