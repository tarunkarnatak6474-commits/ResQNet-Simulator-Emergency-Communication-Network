/* ui.js - renders data into the page (panels, tables, log). No network logic. */
const ui = {
  $(id) { return document.getElementById(id); },

  h(tag, text, cls) {                       // tiny element builder
    const e = document.createElement(tag);
    if (text !== undefined) e.textContent = text;
    if (cls) e.className = cls;
    return e;
  },
  arrow(path) { return path ? path.join(" → ") : "—"; },

  renderStatus(sim) {
    const labels = { IDLE: "Idle", ACTIVE: "Active", STOPPED: "Stopped", NO_ROUTE: "No route" };
    const el = this.$("status");
    el.textContent = labels[sim.status] || sim.status;
    el.className = "badge" + (sim.status === "ACTIVE" ? " ok" : (sim.status === "STOPPED" || sim.status === "NO_ROUTE") ? " bad" : "");
  },

  renderFinal(sim) {
    const el = this.$("final");
    el.textContent = sim.finalMessage;
    el.className = "final" + (sim.status === "ACTIVE" ? " ok" : (sim.status === "STOPPED" || sim.status === "NO_ROUTE") ? " bad" : "");
  },

  renderMetrics(m) {
    const defs = [["sent", "Packets sent"], ["delivered", "Delivered"], ["lost", "Lost"],
      ["deliveryRate", "Delivery rate", "%"], ["packetLoss", "Packet loss", "%"], ["pathDelay", "Path delay", " ms"],
      ["throughput", "Throughput", " Mbps"], ["routeChanges", "Route changes"], ["recoveryTime", "Recovery time", " ms"]];
    const box = this.$("metrics");
    box.replaceChildren();
    for (const [key, label, unit] of defs) {
      const card = this.h("div", undefined, "metric");
      card.append(this.h("b", m[key] + (unit || "")), this.h("span", label));
      box.append(card);
    }
  },

  renderRoutes(routes) {
    const body = this.$("routes");
    body.replaceChildren();
    if (!routes.length) { const td = this.h("td", "No route yet."); td.colSpan = 5; body.append(this.h("tr")); body.lastChild.append(td); return; }
    for (const r of routes) {
      const tr = this.h("tr");
      [r.name, this.arrow(r.path), r.cost, r.delay + " ms", r.status].forEach(v => tr.append(this.h("td", v)));
      body.append(tr);
    }
  },

  renderComparison(c) {
    const body = this.$("comparison");
    body.replaceChildren();
    const rows = c ? [["Packets delivered", x => x.delivered + " / " + x.sent], ["Delivery rate", x => x.deliveryRate + "%"],
      ["Packet loss", x => x.packetLoss + "%"], ["Route changes", x => x.routeChanges],
      ["Recovery time", x => x.recoveryTime + " ms"], ["Final state", x => x.finalState]] : [];
    if (!c) { const tr = this.h("tr"), td = this.h("td", "Send a batch of packets to compare both modes."); td.colSpan = 3; tr.append(td); body.append(tr); return; }
    for (const [label, f] of rows) {
      const tr = this.h("tr");
      tr.append(this.h("td", label), this.h("td", f(c.static)), this.h("td", f(c.adaptive)));
      body.append(tr);
    }
  },

  renderEventLog(events) {
    const ol = this.$("log");
    ol.replaceChildren();
    for (const e of events) {
      const li = this.h("li", undefined, e.type);
      li.append(this.h("time", e.time), document.createTextNode(e.message));
      ol.append(li);
    }
    ol.scrollTop = ol.scrollHeight;
  },

  fillSelect(sel, items, labelFn) {          // keeps the current choice when possible
    const old = sel.value;
    sel.replaceChildren();
    for (const it of items) { const o = this.h("option", labelFn(it)); o.value = it.id; sel.append(o); }
    if (items.some(i => i.id === old)) sel.value = old;
  },

  /* Keep all dropdowns in step with the network the backend reported. */
  updateControls(network, scenarios) {
    const nodeLabel = n => n.id + " – " + n.role;
    ["reqFrom", "reqTo", "linkFrom", "linkTo"].forEach(id => this.fillSelect(this.$(id), network.nodes, nodeLabel));
    if (!this._init) {                                    // first load: sensible defaults
      this.$("reqFrom").value = "A"; this.$("reqTo").value = "D";
      this.fillSelect(this.$("scSelect"), scenarios, s => s.label);
      this._init = true;
    }
    this.updateTargets(network);
  },

  updateTargets(network) {                               // node events list nodes, link events list links
    const isNode = /Node$/.test(this.$("evType").value);
    this.fillSelect(this.$("evTarget"), isNode ? network.nodes : network.links,
                    isNode ? n => n.id + " – " + n.role : l => l.id);
    this.$("evLoadWrap").hidden = this.$("evType").value !== "congestLink";
  },

  showNotice(msg, kind) {
    const n = this.$("notice");
    n.textContent = msg; n.className = "notice" + (kind === "error" ? " error" : ""); n.hidden = false;
    clearTimeout(this._t); this._t = setTimeout(() => { n.hidden = true; }, 5000);
  }
};
