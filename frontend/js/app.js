/* app.js - starts the app and wires buttons to api calls.
   Pattern: read inputs -> call api -> refresh() -> redraw everything from api data. */

const $ = id => document.getElementById(id);

async function refresh() {            // ask the backend for everything and redraw
  const [network, sim, metrics, routes, events, comparison] = await Promise.all([
    api.getNetworkState(), api.getSimulationState(), api.getMetrics(),
    api.getRoutes(), api.getEventLog(), api.getComparison()]);
  ui.updateControls(network, await api.listScenarios());
  map.render(network, sim);
  ui.renderStatus(sim); ui.renderFinal(sim); ui.renderMetrics(metrics);
  ui.renderRoutes(routes); ui.renderComparison(comparison); ui.renderEventLog(events);
}

/* Run one api action, show errors from the backend, redraw, animate packets if any. */
async function act(action) {
  try {
    const result = await action();
    await refresh();
    if (result && result.route) map.renderPackets(result.route, result.delivered, result.lost);
  } catch (err) {
    ui.showNotice(err.message, "error");
  }
}

/* Only obvious input mistakes are checked here; the backend has the final say. */
function readInt(id, label, min, max) {
  const v = Number($(id).value);
  if (!Number.isInteger(v) || v < min || (max && v > max)) throw new Error(label + " must be a whole number" + (max ? " from " + min + " to " + max : " of at least " + min) + ".");
  return v;
}

function wire() {
  $("reqBtn").onclick = () => act(() => api.createEmergencyRequest({
    source: $("reqFrom").value, destination: $("reqTo").value, priority: Number($("reqPriority").value),
    packetCount: readInt("reqPackets", "Packets", 1, 5000), adaptiveRouting: $("reqAdaptive").checked }));
  $("batchBtn").onclick = () => act(() => api.transmitBatch(readInt("batchSize", "Batch size", 1, 5000)));

  $("evType").onchange = async () => ui.updateTargets(await api.getNetworkState());
  $("evBtn").onclick = () => act(() => {
    if (!$("evTarget").value) throw new Error("Choose a target for the event.");
    return api.triggerEvent({ type: $("evType").value, target: $("evTarget").value, load: readInt("evLoad", "Load", 1, 100) });
  });
  $("restoreBtn").onclick = () => act(() => api.restoreAll());

  $("scRun").onclick = () => act(() => api.runScenario($("scSelect").value));
  $("scReset").onclick = () => act(() => api.resetSimulation());

  $("addNodeBtn").onclick = () => act(async () => {
    const name = $("nodeName").value.trim();
    if (!name) throw new Error("Enter a node name.");
    await api.addNode({ name, role: $("nodeRole").value.trim() });
    $("nodeName").value = ""; $("nodeRole").value = "";
  });
  $("addLinkBtn").onclick = () => act(() => {
    if (!$("linkFrom").value || !$("linkTo").value) throw new Error("Choose both end nodes.");
    return api.addLink({ from: $("linkFrom").value, to: $("linkTo").value,
      cost: readInt("linkCost", "Cost", 1), bandwidth: readInt("linkBw", "Bandwidth", 1), delay: readInt("linkDelay", "Delay", 1) });
  });

  $("themeBtn").onclick = () => {
    const dark = document.documentElement.dataset.theme !== "dark";
    document.documentElement.dataset.theme = dark ? "dark" : "light";
    $("themeBtn").textContent = dark ? "Light theme" : "Dark theme";
    $("themeBtn").setAttribute("aria-pressed", dark);
  };
}

/* Clicking the map fills the event panel (node click -> node event, link click -> link event). */
async function pickTarget(id, nodeEvent) {
  const isNode = /Node$/.test($("evType").value);
  if (isNode !== nodeEvent) $("evType").value = nodeEvent ? "failNode" : "failLink";
  ui.updateTargets(await api.getNetworkState());
  $("evTarget").value = id;
}

map.init($("map"), { onNodeClick: id => pickTarget(id, true), onLinkClick: id => pickTarget(id, false) });
wire();
refresh();
