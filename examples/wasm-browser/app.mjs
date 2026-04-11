const fileInput = document.querySelector("#dat-file");
const sizeSelect = document.querySelector("#size");
const statusNode = document.querySelector("#status");
const canvas = document.querySelector("#preview");
const metaNode = document.querySelector("#meta");

const sliderDefinitions = [
  { id: "contrast", valueId: "contrast-value", formatter: formatSignedInteger },
  { id: "brightness", valueId: "brightness-value", formatter: formatSignedInteger },
  { id: "vividness", valueId: "vividness-value", formatter: formatSignedInteger },
  { id: "sharpness", valueId: "sharpness-value", formatter: formatSignedInteger },
  { id: "red-balance", valueId: "red-balance-value", formatter: String },
  { id: "green-balance", valueId: "green-balance-value", formatter: String },
  { id: "blue-balance", valueId: "blue-balance-value", formatter: String },
];

const sliderElements = Object.fromEntries(
  sliderDefinitions.map((definition) => [definition.id, document.querySelector(`#${definition.id}`)]),
);
const sliderValueElements = Object.fromEntries(
  sliderDefinitions.map((definition) => [definition.id, document.querySelector(`#${definition.valueId}`)]),
);

let renderWorker = null;
let loadedFile = null;
let sessionReady = false;
let activeFileToken = 0;
let latestRenderRequestId = 0;
let pendingOpen = null;

function setStatus(message) {
  statusNode.textContent = message;
}

function formatSignedInteger(value) {
  if (value > 0) {
    return `+${value}`;
  }
  return String(value);
}

function sliderNumericValue(id) {
  return Number.parseInt(sliderElements[id].value, 10);
}

function currentOptions() {
  return {
    size: sizeSelect.value,
    redBalance: sliderNumericValue("red-balance"),
    greenBalance: sliderNumericValue("green-balance"),
    blueBalance: sliderNumericValue("blue-balance"),
    contrast: sliderNumericValue("contrast") + 3,
    brightness: sliderNumericValue("brightness") + 3,
    vividness: sliderNumericValue("vividness") + 3,
    sharpness: sliderNumericValue("sharpness") + 3,
  };
}

function currentOptionSummary() {
  return [
    `size=${sizeSelect.value}`,
    `contrast=${formatSignedInteger(sliderNumericValue("contrast"))}`,
    `brightness=${formatSignedInteger(sliderNumericValue("brightness"))}`,
    `vividness=${formatSignedInteger(sliderNumericValue("vividness"))}`,
    `sharpness=${formatSignedInteger(sliderNumericValue("sharpness"))}`,
    `rgb=${sliderNumericValue("red-balance")}/${sliderNumericValue("green-balance")}/${sliderNumericValue("blue-balance")}`,
  ].join(" · ");
}

function updateSliderReadouts() {
  for (const definition of sliderDefinitions) {
    const rawValue = sliderNumericValue(definition.id);
    sliderValueElements[definition.id].textContent = definition.formatter(rawValue);
  }
}

function ensureWorker() {
  if (renderWorker !== null) {
    return renderWorker;
  }

  renderWorker = new Worker(new URL("./worker.mjs", import.meta.url), { type: "module" });
  renderWorker.addEventListener("message", handleWorkerMessage);
  renderWorker.addEventListener("error", handleWorkerFailure);
  return renderWorker;
}

function handleWorkerFailure(event) {
  console.error(event);
  setStatus(
    [
      "Worker initialization failed.",
      String(event.message ?? event.error ?? event),
      "",
      "Make sure you built the WASM target and are serving the repo root over HTTP.",
    ].join("\n"),
  );
}

function settlePendingOpen(fileToken, error = null) {
  if (pendingOpen === null || pendingOpen.fileToken !== fileToken) {
    return;
  }

  const { resolve, reject } = pendingOpen;
  pendingOpen = null;
  if (error === null) {
    resolve();
    return;
  }
  reject(error);
}

function handleWorkerMessage(event) {
  const message = event.data ?? {};

  switch (message.type) {
    case "session-opened":
      if (message.fileToken !== activeFileToken) {
        return;
      }
      sessionReady = true;
      settlePendingOpen(message.fileToken);
      return;
    case "session-open-error":
      if (message.fileToken !== activeFileToken) {
        return;
      }
      sessionReady = false;
      settlePendingOpen(message.fileToken, new Error(message.error ?? "session open failed"));
      setStatus(
        [
          "Conversion failed.",
          String(message.error ?? "session open failed"),
          "",
          "Make sure you built the WASM target and are serving the repo root over HTTP.",
        ].join("\n"),
      );
      return;
    case "render-complete":
      if (message.fileToken !== activeFileToken || message.requestId !== latestRenderRequestId) {
        return;
      }
      drawResult({
        width: message.width,
        height: message.height,
        pixels: new Uint8Array(message.pixels),
      });
      metaNode.textContent =
        `${loadedFile?.name ?? "unknown"} · ${message.width}x${message.height} · ${message.pixels.byteLength} RGBA bytes · ${currentOptionSummary()}`;
      setStatus("Conversion complete.");
      return;
    case "render-error":
      if (message.fileToken !== activeFileToken || message.requestId !== latestRenderRequestId) {
        return;
      }
      setStatus(
        [
          "Conversion failed.",
          String(message.error ?? "render failed"),
          "",
          "Make sure you built the WASM target and are serving the repo root over HTTP.",
        ].join("\n"),
      );
      return;
    default:
      return;
  }
}

function drawResult(result) {
  canvas.width = result.width;
  canvas.height = result.height;

  const context = canvas.getContext("2d");
  const imageData = new ImageData(new Uint8ClampedArray(result.pixels), result.width, result.height);
  context.putImageData(imageData, 0, 0);
}

async function openSessionForFile(file, bytes) {
  const fileToken = activeFileToken;
  if (pendingOpen !== null) {
    settlePendingOpen(pendingOpen.fileToken, new Error("superseded by a newer file selection"));
  }

  const worker = ensureWorker();
  await new Promise((resolve, reject) => {
    pendingOpen = { fileToken, resolve, reject };
    worker.postMessage(
      {
        type: "open-file",
        fileToken,
        fileName: file.name,
        buffer: bytes.buffer,
      },
      [bytes.buffer],
    );
  });
}

function requestRender(reason = "Updating preview...") {
  if (loadedFile === null || !sessionReady) {
    return;
  }

  latestRenderRequestId += 1;
  setStatus(reason);
  ensureWorker().postMessage({
    type: "render",
    fileToken: activeFileToken,
    requestId: latestRenderRequestId,
    options: currentOptions(),
  });
}

async function loadSelectedFile() {
  const file = fileInput.files?.[0] ?? null;
  loadedFile = file;
  activeFileToken += 1;
  sessionReady = false;

  if (file === null) {
    if (pendingOpen !== null) {
      settlePendingOpen(pendingOpen.fileToken, new Error("session closed"));
    }
    ensureWorker().postMessage({ type: "close-session", fileToken: activeFileToken });
    metaNode.textContent = "No image loaded yet.";
    setStatus("Choose a DAT file to begin.");
    return;
  }

  try {
    setStatus(`Reading ${file.name}...`);
    const bytes = new Uint8Array(await file.arrayBuffer());
    if (file !== loadedFile) {
      return;
    }

    setStatus(`Opening worker session for ${file.name}...`);
    await openSessionForFile(file, bytes);
    requestRender("Converting uploaded DAT in a background worker...");
  } catch (error) {
    if (file !== loadedFile) {
      return;
    }
    console.error(error);
    setStatus(
      [
        "Conversion failed.",
        String(error?.message ?? error),
        "",
        "Make sure you built the WASM target and are serving the repo root over HTTP.",
      ].join("\n"),
    );
  }
}

fileInput.addEventListener("change", () => {
  void loadSelectedFile();
});

sizeSelect.addEventListener("change", () => {
  requestRender(`Updating ${sizeSelect.value} preview in a background worker...`);
});

for (const definition of sliderDefinitions) {
  sliderElements[definition.id].addEventListener("input", () => {
    updateSliderReadouts();
    requestRender(`Applying ${definition.id.replaceAll("-", " ")} in a background worker...`);
  });
}

updateSliderReadouts();
