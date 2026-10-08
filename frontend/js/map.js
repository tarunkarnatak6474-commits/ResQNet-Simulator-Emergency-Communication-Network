/* map.js - draws the network as SVG and handles clicks/drags.
   It only DRAWS what the backend reports. No routing or failure logic here. */
const map = {
  NS: "http://www.w3.org/2000/svg",
  svg: null, handlers: {}, data: null,
  positions: {},          // dragged positions (display only, never sent to the backend)
  selected: null,         // { kind: "node"|"link", id }
  moved: false,

  s(tag, attrs, cls) {    // create an SVG element
    const e = document.createElementNS(this.NS, tag);
    for (const k in attrs) e.setAttribute(k, attrs[k]);
    if (cls) e.setAttribute("class", cls);
    return e;
  },

  init(svg, handlers) {   // handlers: { onNodeClick(id), onLinkClick(id) }
    this.svg = svg; this.handlers = handlers;
    ["links", "route", "nodes", "packets"].forEach(n => svg.append(this.s("g", { id: "layer-" + n })));
    svg.addEventListener("pointermove", e => this.handleNodeDrag(e));
    svg.addEventListener("pointerup", () => { this.dragId = null; });
  },

  pos(n) { return this.positions[n.id] || { x: n.x, y: n.y }; },
  layer(name) { return this.svg.querySelector("#layer-" + name); },

  render(network, sim) {
    this.data = { network, sim };
    this.renderLinks(network);
    this.renderRoute(network, sim);
    this.renderNodes(network, sim);
  },

  renderLinks(network) {
    const g = this.layer("links"); g.replaceChildren();
    for (const l of network.links) {
      const a = this.pos(network.nodes.find(n => n.id === l.from)), b = this.pos(network.nodes.find(n => n.id === l.to));
      const grp = this.s("g", {}, "link" + (l.active ? "" : " failed") + (this.isSel("link", l.id) ? " selected" : ""));
      const line = { x1: a.x, y1: a.y, x2: b.x, y2: b.y };
      if (l.congested) grp.append(this.s("line", line, "halo"));
      grp.append(this.s("line", line, "link-line"));
      const hit = this.s("line", { ...line, tabindex: 0, role: "button",
        "aria-label": "Link " + l.id + ", " + (l.active ? "working" : "failed") + (l.congested ? ", congested" : "") }, "hit");
      hit.addEventListener("click", () => this.handleLinkClick(l.id));
      hit.addEventListener("keydown", e => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); this.handleLinkClick(l.id); } });
      grp.append(hit);
      const t = this.s("text", { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 - 8 }, "label");
      t.textContent = "cost " + l.cost + ", " + l.delay + " ms";
      grp.append(t);
      g.append(grp);
    }
  },

  renderRoute(network, sim) {          // green overlay on the links of the current route
    const g = this.layer("route"); g.replaceChildren();
    const r = sim.currentRoute;
    if (!r || sim.status !== "ACTIVE") return;
    for (let i = 1; i < r.length; i++) {
      const a = this.pos(network.nodes.find(n => n.id === r[i - 1])), b = this.pos(network.nodes.find(n => n.id === r[i]));
      g.append(this.s("line", { x1: a.x, y1: a.y, x2: b.x, y2: b.y }, "route-line"));
    }
  },

  renderNodes(network, sim) {
    const g = this.layer("nodes"); g.replaceChildren();
    for (const n of network.nodes) {
      const p = this.pos(n);
      const grp = this.s("g", { transform: "translate(" + p.x + "," + p.y + ")", tabindex: 0, role: "button",
        "aria-label": "Node " + n.id + ", " + n.role + ", " + (n.active ? "active" : "failed") +
          (n.id === sim.source ? ", source" : "") + (n.id === sim.destination ? ", destination" : "") },
        "node" + (n.active ? "" : " failed") + (this.isSel("node", n.id) ? " selected" : ""));
      grp.append(this.s("circle", { r: 26 }));
      const id = this.s("text", {}, "id"); id.textContent = n.active ? n.id : "✕ " + n.id; grp.append(id);
      const nm = this.s("text", { y: 44 }, "name"); nm.textContent = n.role; grp.append(nm);
      const tag = n.id === sim.source ? "SOURCE" : n.id === sim.destination ? "DESTINATION" : "";
      if (tag) { const b = this.s("text", { y: -36 }, "badge-svg"); b.textContent = tag; grp.append(b); }
      grp.addEventListener("pointerdown", e => { this.dragId = n.id; this.moved = false; e.preventDefault(); });
      grp.addEventListener("click", () => { if (!this.moved) this.handleNodeClick(n.id); });
      grp.addEventListener("keydown", e => this.handleNodeKey(e, n));
      g.append(grp);
    }
  },

  /* Packets travel along the route the backend gave us. Green = delivered, red = lost. */
  renderPackets(route, delivered, lost) {
    if (!route || window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;
    const pts = route.map(id => this.pos(this.data.network.nodes.find(n => n.id === id)));
    const d = "M" + pts.map(p => p.x + "," + p.y).join(" L");
    const total = delivered + lost, count = Math.min(10, total), g = this.layer("packets");
    for (let i = 0; i < count; i++) {
      const ok = i < Math.round(count * delivered / total);   // show the same ratio, max 10 dots
      const c = this.s("circle", { r: 6, opacity: 0, fill: ok ? "var(--route)" : "var(--fail)" });
      const mv = this.s("animateMotion", { dur: "1.8s", path: d, begin: "indefinite", fill: "freeze" });
      const show = this.s("set", { attributeName: "opacity", to: 1, begin: "indefinite", fill: "freeze" });
      c.append(mv, show); g.append(c);
      mv.beginElementAt(i * 0.2); show.beginElementAt(i * 0.2);
      setTimeout(() => c.remove(), 1800 + i * 200 + 300);
    }
  },

  isSel(kind, id) { return this.selected && this.selected.kind === kind && this.selected.id === id; },
  select(kind, id) { this.selected = { kind, id }; if (this.data) this.render(this.data.network, this.data.sim); },

  handleNodeClick(id) { this.select("node", id); this.handlers.onNodeClick(id); },
  handleLinkClick(id) { this.select("link", id); this.handlers.onLinkClick(id); },

  handleNodeDrag(e) {
    if (!this.dragId) return;
    const pt = this.svg.createSVGPoint(); pt.x = e.clientX; pt.y = e.clientY;
    const p = pt.matrixTransform(this.svg.getScreenCTM().inverse());
    this.positions[this.dragId] = { x: Math.max(30, Math.min(870, p.x)), y: Math.max(40, Math.min(420, p.y)) };
    this.moved = true;
    this.render(this.data.network, this.data.sim);
  },

  handleNodeKey(e, n) {               // Enter selects, arrow keys nudge the node
    const step = { ArrowLeft: [-12, 0], ArrowRight: [12, 0], ArrowUp: [0, -12], ArrowDown: [0, 12] }[e.key];
    if (e.key === "Enter" || e.key === " ") { e.preventDefault(); this.handleNodeClick(n.id); }
    else if (step) {
      e.preventDefault();
      const p = this.pos(n); this.positions[n.id] = { x: p.x + step[0], y: p.y + step[1] };
      this.render(this.data.network, this.data.sim);
      this.layer("nodes").children[this.data.network.nodes.indexOf(n)].focus();
    }
  }
};
