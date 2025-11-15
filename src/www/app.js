// UI elements
const videoFile = document.getElementById('videoFile');
const video = document.getElementById('video');
const startButton = document.getElementById('startButton');
const stopButton = document.getElementById('stopButton');
const previewImage = document.getElementById('previewImage');
const batteryVoltageDisplay = document.getElementById('batteryVoltageDisplay');
const jpegQualitySlider = document.getElementById('jpegQuality');
const scalingModeSelect = document.getElementById('scalingMode');
const fpsDisplay = document.getElementById('fpsDisplay');
const frameSizeDisplay = document.getElementById('frameSizeDisplay');
const settingsForm = document.getElementById('settingsForm');
const ssidInput = document.getElementById('ssid');
const passInput = document.getElementById('pass');
const brightnessSlider = document.getElementById('brightness');
const osdLevelSelect = document.getElementById('osdLevel');
const streamingTabLabel = document.getElementById('streamingTabLabel');
const settingsTabRadio = document.getElementById('tab-settings');
const splashscreen = document.getElementById('splashscreen');
const updateForm = document.getElementById('updateForm');
const firmwareFile = document.getElementById('firmwareFile');
const updateProgress = document.getElementById('updateProgress');

let ws;
let videoFrameId;
let fpsInterval;
let lastFrameTime;
let frameTimeBuffer;
let apMode = false;

// Fetch all settings from the server
async function fetchSettings() {
  let success = false;
  await fetch('/settings')
    .then(response => response.json())
    .then(settings => {
      ssidInput.value = settings.ssid;
      brightnessSlider.value = settings.brightness;
      osdLevelSelect.value = settings.osdLevel;
      apMode = settings.apMode;
      success = true;
    })
    .catch(error => console.warn('Error fetching settings:', error));
  return success;
  }

// Save all settings
settingsForm.addEventListener('submit', (event) => {
  event.preventDefault();
  const settings = {
    ssid: ssidInput.value,
    pass: passInput.value,
    brightness: parseInt(brightnessSlider.value),
    osdLevel: parseInt(osdLevelSelect.value)
  };

  fetch('/settings', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(settings)
  }).then(() => {
    alert('Settings saved!');
  }).catch(error => {
    console.error('Error saving settings:', error);
    alert('Failed to save settings.');
  });
});

// OTA update
updateForm.addEventListener('submit', (event) => {
  event.preventDefault();
  const file = firmwareFile.files[0];
  if (!file) {
    alert('Please select a firmware file.');
    return;
  }

  updateProgress.style.display = 'block';
  updateProgress.value = 0;

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update', true);

  xhr.upload.onprogress = (event) => {
    if (event.lengthComputable) {
      const percentComplete = (event.loaded / event.total) * 100;
      updateProgress.value = percentComplete;
    }
  };

  xhr.onload = () => {
    if (xhr.status === 200) {
      alert('Update successful! The device will now reboot.');
    } else {
      alert(`Update failed! Server responded with status: ${xhr.status}`);
    }
    updateProgress.style.display = 'none';
  };

  xhr.onerror = () => {
    alert('An error occurred during the update.');
    updateProgress.style.display = 'none';
  };

  const formData = new FormData();
  formData.append('update', file);
  xhr.send(formData);
});


// Function to fetch and display battery voltage
function fetchBatteryVoltage() {
  fetch('/voltage')
    .then(response => response.json())
    .then(data => {
      if (data && data.voltage !== undefined) {
        batteryVoltageDisplay.textContent = data.voltage.toFixed(2);
      }
    })
    .catch(error => console.error('Error fetching battery voltage:', error));
}

// WebSocket connection
function connectWebSocket() {
  try {
    ws = new WebSocket(`ws://${window.location.host}/ws`);
    ws.onopen = () => {
      console.log("WebSocket connection established");
    };
    ws.onmessage = (event) => {
      if (event.data === "ready") {
        video.requestVideoFrameCallback(sendFrame);
      }
    };
    ws.onclose = () => {
      console.log("WebSocket connection closed, retrying...");
      setTimeout(connectWebSocket, 1000);
    };
    ws.onerror = (error) => console.error("WebSocket error:", error);
  } catch (e) {
    console.warn("WebSocket connection failed.", e);
  }
}

videoFile.addEventListener('change', () => {
  const file = videoFile.files[0];
  if (file) {
    video.src = URL.createObjectURL(file);
  }
});

function sendFrame() {
  if (video.paused || video.ended) {
    return;
  }

  const jpegQuality = parseFloat(jpegQualitySlider.value);
  const scalingMode = scalingModeSelect.value;
  const canvas = document.createElement('canvas');
  canvas.width = 288;
  canvas.height = 240;
  const context = canvas.getContext('2d');
  const videoAspectRatio = video.videoWidth / video.videoHeight;
  const canvasAspectRatio = canvas.width / canvas.height;
  let sx = 0, sy = 0, sWidth = video.videoWidth, sHeight = video.videoHeight;
  let dx = 0, dy = 0, dWidth = canvas.width, dHeight = canvas.height;

  if (scalingMode === 'letterbox') {
    if (videoAspectRatio > canvasAspectRatio) {
      dHeight = canvas.width / videoAspectRatio;
      dy = (canvas.height - dHeight) / 2;
    } else {
      dWidth = canvas.height * videoAspectRatio;
      dx = (canvas.width - dWidth) / 2;
    }
    context.fillStyle = 'black';
    context.fillRect(0, 0, canvas.width, canvas.height);
    context.drawImage(video, sx, sy, sWidth, sHeight, dx, dy, dWidth, dHeight);
  } else if (scalingMode === 'crop') {
    if (videoAspectRatio > canvasAspectRatio) {
      sWidth = video.videoHeight * canvasAspectRatio;
      sx = (video.videoWidth - sWidth) / 2;
    } else {
      sHeight = video.videoWidth / canvasAspectRatio;
      sy = (video.videoHeight - sHeight) / 2;
    }
    context.drawImage(video, sx, sy, sWidth, sHeight, 0, 0, canvas.width, canvas.height);
  } else { // stretch
    context.drawImage(video, 0, 0, canvas.width, canvas.height);
  }

  canvas.toBlob(blob => {
    if (blob) {
      const now = performance.now();
      if (lastFrameTime) {
        const frameTime = now - lastFrameTime;
        if (frameTime > 0 && frameTime < 1000) {
          frameTimeBuffer.push(frameTime);
        }
      }
      lastFrameTime = now;
      frameSizeDisplay.textContent = blob.size;
      const imageUrl = URL.createObjectURL(blob);
      previewImage.src = imageUrl;
      previewImage.onload = () => URL.revokeObjectURL(imageUrl);
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(blob);
      }
    }
  }, 'image/jpeg', jpegQuality);
}

startButton.onclick = () => {
  if (!video.src) {
    alert("Please select a video file first.");
    return;
  }
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    alert("WebSocket is not connected. Please wait.");
    return;
  }
  lastFrameTime = performance.now();
  frameTimeBuffer = [];
  fpsInterval = setInterval(() => {
    if (frameTimeBuffer && frameTimeBuffer.length > 0) {
      const avgInterval = frameTimeBuffer.reduce((a, b) => a + b) / frameTimeBuffer.length;
      const currentFps = 1000 / avgInterval;
      fpsDisplay.textContent = currentFps.toFixed(1);
      frameTimeBuffer = [];
    } else {
      fpsDisplay.textContent = '0.0';
    }
  }, 1000);
  video.play();
  ws.send("START");
};

stopButton.onclick = () => {
  if (videoFrameId) {
    video.cancelVideoFrameCallback(videoFrameId);
    videoFrameId = null;
  }
  video.pause();
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send("STOP");
  }
  clearInterval(fpsInterval);
  fpsDisplay.textContent = '-';
  frameSizeDisplay.textContent = '-';
};

scalingModeSelect.onchange = (e) => {
  const mode = e.target.value;
  switch (mode) {
  case 'letterbox':
    video.style.objectFit = 'contain';
    return;
  case 'crop':
    video.style.objectFit = 'cover';
    return;
  case 'stretch':
    video.style.objectFit = 'fill';
    return;
  default:
    break;
  }
}

// Initial setup
window.onload = async () => {
  const success = await fetchSettings();
  if (success) {
    fetchBatteryVoltage();
    setInterval(fetchBatteryVoltage, 60000);
  }
  if (!success || apMode) {
    streamingTabLabel.style.display = 'none';
    settingsTabRadio.checked = true;
  } else {
    connectWebSocket();
  }
  document.querySelector('.tabs').style.display = 'flex';
  splashscreen.style.display = 'none';
}