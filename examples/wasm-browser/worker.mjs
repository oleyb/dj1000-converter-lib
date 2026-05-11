let converterPromise = null;
let loadedSession = null;
let latestOpenToken = 0;
let activeFileToken = 0;
let renderInFlight = false;
let pendingRender = null;

async function loadConverter() {
  if (converterPromise !== null) {
    return converterPromise;
  }

  converterPromise = import("../../build-wasm/native/dj1000_wasm_api.mjs")
    .then(({ createDj1000WasmConverter }) => createDj1000WasmConverter())
    .catch((error) => {
      converterPromise = null;
      throw error;
    });
  return converterPromise;
}

function closeSession() {
  if (loadedSession !== null) {
    loadedSession.close();
    loadedSession = null;
  }
  pendingRender = null;
}

function postWorkerError(type, fileToken, requestId, error) {
  self.postMessage({
    type,
    fileToken,
    requestId,
    error: String(error?.message ?? error),
  });
}

function processRenderQueue() {
  if (renderInFlight || pendingRender === null || loadedSession === null) {
    return;
  }

  const task = pendingRender;
  pendingRender = null;
  renderInFlight = true;
  try {
    const result = loadedSession.renderToRgba(task.options);
    if (task.fileToken === activeFileToken) {
      self.postMessage(
        {
          type: "render-complete",
          fileToken: task.fileToken,
          requestId: task.requestId,
          width: result.width,
          height: result.height,
          pixels: result.pixels.buffer,
        },
        [result.pixels.buffer],
      );
    }
  } catch (error) {
    if (task.fileToken === activeFileToken) {
      postWorkerError("render-error", task.fileToken, task.requestId, error);
    }
  } finally {
    renderInFlight = false;
    if (pendingRender !== null) {
      queueMicrotask(processRenderQueue);
    }
  }
}

async function openFile(message) {
  const { fileToken, buffer } = message;
  latestOpenToken = fileToken;

  try {
    const converter = await loadConverter();
    if (fileToken !== latestOpenToken) {
      return;
    }

    closeSession();
    loadedSession = converter.openSession(new Uint8Array(buffer));
    activeFileToken = fileToken;
    self.postMessage({ type: "session-opened", fileToken });
    processRenderQueue();
  } catch (error) {
    if (fileToken === latestOpenToken) {
      postWorkerError("session-open-error", fileToken, 0, error);
    }
  }
}

self.addEventListener("message", (event) => {
  const message = event.data ?? {};

  switch (message.type) {
    case "open-file":
      void openFile(message);
      break;
    case "render":
      if (message.fileToken !== activeFileToken || loadedSession === null) {
        return;
      }
      pendingRender = message;
      processRenderQueue();
      break;
    case "close-session":
      latestOpenToken = message.fileToken ?? 0;
      activeFileToken = 0;
      closeSession();
      break;
    default:
      break;
  }
});
