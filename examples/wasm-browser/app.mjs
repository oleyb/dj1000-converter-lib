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
  sliderDefinitions.map((definition) => [definition.id, document.querySelector(`#${definition.id}`)])
);
const sliderValueElements = Object.fromEntries(
  sliderDefinitions.map((definition) => [definition.id, document.querySelector(`#${definition.valueId}`)])
);

let converterPromise = null;
let loadedFile = null;
let loadedSession = null;
let renderTimer = null;
let renderGeneration = 0;

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

function drawResult(result) {
  canvas.width = result.width;
  canvas.height = result.height;

  const context = canvas.getContext("2d");
  const imageData = new ImageData(new Uint8ClampedArray(result.pixels), result.width, result.height);
  context.putImageData(imageData, 0, 0);
}

function clearScheduledRender() {
  if (renderTimer !== null) {
    window.clearTimeout(renderTimer);
    renderTimer = null;
  }
}

function scheduleRender(reason = "Updating preview...") {
  if (loadedSession === null) {
    return;
  }
  clearScheduledRender();
  renderTimer = window.setTimeout(() => {
    renderTimer = null;
    void convertSelectedFile(reason);
  }, 140);
}

async function loadSelectedFile() {
  const file = fileInput.files?.[0] ?? null;
  loadedFile = file;
  if (loadedSession !== null) {
    loadedSession.close();
    loadedSession = null;
  }

  if (file === null) {
    metaNode.textContent = "No image loaded yet.";
    setStatus("Choose a DAT file to begin.");
    return;
  }

  setStatus(`Reading ${file.name}...`);
  const converter = await loadConverter();
  const bytes = new Uint8Array(await file.arrayBuffer());
  setStatus(`Opening session for ${file.name}...`);
  loadedSession = converter.openSession(bytes);
  await convertSelectedFile("Converting uploaded DAT...");
}

async function convertSelectedFile(reason = "Converting...") {
  if (loadedFile === null || loadedSession === null) {
    setStatus("Choose a DAT file first.");
    return;
  }

  const renderId = ++renderGeneration;
  setStatus("Loading WASM module...");

  try {
    const converter = await loadConverter();
    if (renderId !== renderGeneration) {
      return;
    }
    setStatus(reason);
    const result = loadedSession.renderToRgba(currentOptions());
    if (renderId !== renderGeneration) {
      return;
    }
    drawResult(result);
    metaNode.textContent =
      `${loadedFile.name} · ${result.width}x${result.height} · ${result.pixels.length} RGBA bytes · ${currentOptionSummary()}`;
    setStatus("Conversion complete.");
  } catch (error) {
    console.error(error);
    setStatus(
      [
        "Conversion failed.",
        String(error?.message ?? error),
        "",
        "Make sure you built the WASM target and are serving the repo root over HTTP.",
      ].join("\n")
    );
  }
}

fileInput.addEventListener("change", () => {
  void loadSelectedFile();
});

sizeSelect.addEventListener("change", () => {
  scheduleRender(`Updating ${sizeSelect.value} preview...`);
});

for (const definition of sliderDefinitions) {
  sliderElements[definition.id].addEventListener("input", () => {
    updateSliderReadouts();
  });
  sliderElements[definition.id].addEventListener("change", () => {
    scheduleRender(`Applying ${definition.id.replaceAll("-", " ")}...`);
  });
}

updateSliderReadouts();
