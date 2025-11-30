// UI elements
const videoFile = document.getElementById('videoFile');
const video = document.getElementById('video');
const startButton = document.getElementById('startButton');
const stopButton = document.getElementById('stopButton');
const previewImage = document.getElementById('previewImage');
const startScreenButton = document.getElementById('startScreenButton');
const stopScreenButton = document.getElementById('stopScreenButton');
const screenVideo = document.getElementById('screenVideo');
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
const timerMinutesSlider = document.getElementById('timerMinutes');
const timerMinutesDisplay = document.getElementById('timerMinutesDisplay');
const streamingTabLabel = document.getElementById('streamingTabLabel');
const settingsTabRadio = document.getElementById('tab-settings');
const splashscreen = document.getElementById('splashscreen');
const updateForm = document.getElementById('updateForm');
const updateButton = document.getElementById('updateButton');
const firmwareFile = document.getElementById('firmwareFile');
const updateProgress = document.getElementById('updateProgress');
const firmwareVersion = document.getElementById('firmwareVersion');
const firmwareBuild = document.getElementById('firmwareBuild');

let ws;
let apMode = false;
let activeStreamType = null;

const streamState = {
  file: {
    videoEl: video,
    fpsDisplayEl: fpsDisplay,
    frameSizeDisplayEl: frameSizeDisplay,
    previewImageEl: previewImage,
    startButtonEl: startButton,
    stopButtonEl: stopButton,
    fpsInterval: null,
    lastFrameTime: null,
    frameTimeBuffer: [],
  },
  screen: {
    videoEl: screenVideo,
    fpsDisplayEl: fpsDisplay,
    frameSizeDisplayEl: frameSizeDisplay,
    previewImageEl: previewImage,
    startButtonEl: startScreenButton,
    stopButtonEl: stopScreenButton,
    fpsInterval: null,
    lastFrameTime: null,
    frameTimeBuffer: [],
  }
};

// Fetch all settings from the server
async function fetchSettings() {
  let success = false;
  await fetch('/settings')
    .then(response => response.json())
    .then(settings => {
      ssidInput.value = settings.ssid;
      brightnessSlider.value = settings.brightness;
      osdLevelSelect.value = settings.osdLevel;
      timerMinutesSlider.value = settings.timerMinutes;
      updateTimerDisplay(settings.timerMinutes);
      apMode = settings.apMode;
      if (settings.version) {
        firmwareVersion.textContent = settings.version;
      }
      if (settings.build) {
        firmwareBuild.textContent = settings.build;
      }
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
    osdLevel: parseInt(osdLevelSelect.value),
    timerMinutes: parseInt(timerMinutesSlider.value)
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
  firmwareFile.disabled = true;
  firmwareFile.style.display = 'none';
  updateButton.disabled = true;

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
    firmwareFile.disabled = false;
    firmwareFile.style.display = '';
    updateProgress.style.display = 'none';
    updateButton.disabled = false;
  };

  xhr.onerror = () => {
    alert('An error occurred during the update.');
    updateProgress.style.display = 'none';
    updateButton.disabled = false;
  };

  const formData = new FormData();
  formData.append('update', file);
  xhr.send(formData);
});


// Function to fetch and display battery status
function fetchBatteryStatus() {
  const batteryLevelDisplay = document.getElementById('batteryLevelDisplay');
  const batteryChargingDisplay = document.getElementById('batteryChargingDisplay');

  fetch('/battery')
    .then(response => response.json())
    .then(data => {
      if (data) {
        if (data.voltage !== undefined) {
          batteryVoltageDisplay.textContent = data.voltage.toFixed(2);
        }
        if (data.level !== undefined) {
          let batteryIcon = '🟥';
          batteryIcon += data.level > 20 ? '🟧' : '⬛';
          batteryIcon += data.level > 60 ? '🟨' : '⬛';
          batteryIcon += data.level > 80 ? '🟩' : '⬛';
          batteryLevelDisplay.textContent = `${batteryIcon} ${data.level}%`;
        }
        if (data.charging !== undefined) {
          batteryChargingDisplay.textContent = data.charging ? '⚡' : '';
        }
        document.querySelector('.lowBatt').style.display = data.low ? '' : 'none';
      }
    })
    .catch(error => console.error('Error fetching battery status:', error));
}

// WebSocket connection
function connectWebSocket(host) {
  if (!host) {
    host = window.location.host;
  }
  try {
    ws = new WebSocket(`ws://${host}/ws`);
    ws.onopen = () => {
      console.log("WebSocket connection established");
      startButton.disabled = false;
      startScreenButton.disabled = false;
    };
    ws.onmessage = (event) => {
      if (event.data === 'ready' && activeStreamType) {
        const state = streamState[activeStreamType];
        state.videoEl.requestVideoFrameCallback(() => sendFrame(activeStreamType));
      }
    };
    ws.onclose = () => {
      console.log("WebSocket connection closed, retrying...");
      startButton.disabled = true;
      stopButton.click();
      stopScreenButton.click();
      setTimeout(connectWebSocket, 1000);
    };
    ws.onerror = (error) => console.error("WebSocket error:", error);
  } catch (e) {
    console.warn("WebSocket connection failed.", e);
    startButton.disabled = true;
  }
}

videoFile.addEventListener('change', () => {
  const file = videoFile.files[0];
  stopButton.click();
  if (file) {
    video.src = URL.createObjectURL(file);
    setActiveVideo('file');
  }
});

document.querySelectorAll('input[name="streamSource"]').forEach(radio => {
  radio.addEventListener('change', (event) => {
    const source = event.target.value;
    document.getElementById('fileStreamControls').style.display = source === 'file' ? '' : 'none';
    document.getElementById('screenStreamControls').style.display = source === 'screen' ? '' : 'none';
    setActiveVideo(source);
  });
});

function setActiveVideo(streamType) {
  for (const type in streamState) {
    streamState[type].videoEl.style.display = 'none';
  }
  streamState[streamType].videoEl.style.display = 'block';
}

function sendFrame(streamType) {
  const state = streamState[streamType];
  if (state.videoEl.paused || state.videoEl.ended) {
    return;
  }

  const jpegQuality = parseFloat(jpegQualitySlider.value);
  const scalingMode = scalingModeSelect.value;
  const canvas = document.createElement('canvas');
  canvas.width = 288;
  canvas.height = 240;
  const context = canvas.getContext('2d');

  const videoAspectRatio = state.videoEl.videoWidth / state.videoEl.videoHeight;
  const canvasAspectRatio = canvas.width / canvas.height;
  let sx = 0, sy = 0, sWidth = state.videoEl.videoWidth, sHeight = state.videoEl.videoHeight;
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
    context.drawImage(state.videoEl, sx, sy, sWidth, sHeight, dx, dy, dWidth, dHeight);
  } else if (scalingMode === 'crop') {
    if (videoAspectRatio > canvasAspectRatio) {
      sWidth = state.videoEl.videoHeight * canvasAspectRatio;
      sx = (state.videoEl.videoWidth - sWidth) / 2;
    } else {
      sHeight = state.videoEl.videoWidth / canvasAspectRatio;
      sy = (state.videoEl.videoHeight - sHeight) / 2;
    }
    context.drawImage(state.videoEl, sx, sy, sWidth, sHeight, 0, 0, canvas.width, canvas.height);
  } else { // stretch
    context.drawImage(state.videoEl, 0, 0, canvas.width, canvas.height);
  }


  canvas.toBlob(blob => {
    if (blob) {
      const now = performance.now();
      if (state.lastFrameTime) {
        const frameTime = now - state.lastFrameTime;
        if (frameTime > 0 && frameTime < 1000) {
          state.frameTimeBuffer.push(frameTime);
        }
      }
      state.lastFrameTime = now;
      state.frameSizeDisplayEl.textContent = blob.size;
      const imageUrl = URL.createObjectURL(blob);
      state.previewImageEl.src = imageUrl;
      state.previewImageEl.onload = () => URL.revokeObjectURL(imageUrl);
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
  startStreaming('file');
};

stopButton.onclick = () => stopStreaming('file');

function startStreaming(streamType) {
  const state = streamState[streamType];
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    alert("WebSocket is not connected. Please wait.");
    return;
  }
  activeStreamType = streamType;
  for (const type in streamState) {
    streamState[type].startButtonEl.disabled = true;
  }
  state.lastFrameTime = performance.now();
  state.frameTimeBuffer = [];
  state.fpsInterval = setInterval(() => {
    if (state.frameTimeBuffer.length > 0) {
      const avgInterval = state.frameTimeBuffer.reduce((a, b) => a + b) / state.frameTimeBuffer.length;
      const currentFps = 1000 / avgInterval;
      state.fpsDisplayEl.textContent = currentFps.toFixed(1);
      state.frameTimeBuffer = [];
    } else {
      state.fpsDisplayEl.textContent = '0.0';
    }
  }, 1000);
  state.videoEl.play();
  ws.send("START");
  state.stopButtonEl.style.display = '';
  state.startButtonEl.style.display = 'none';
}

function stopStreaming(streamType) {
  const state = streamState[streamType];
  activeStreamType = null;
  for (const type in streamState) {
    streamState[type].startButtonEl.disabled = false;
  }
  state.videoEl.pause();
  if (streamType === 'screen' && state.videoEl.srcObject) {
    state.videoEl.srcObject.getTracks().forEach(track => track.stop());
    state.videoEl.srcObject = null;
  }
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send("STOP");
  }
  clearInterval(state.fpsInterval);
  state.fpsDisplayEl.textContent = '-';
  state.frameSizeDisplayEl.textContent = '-';
  state.stopButtonEl.style.display = 'none';
  state.startButtonEl.style.display = '';
}

startScreenButton.onclick = async () => {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    alert("WebSocket is not connected. Please wait.");
    return;
  }
  try {
    screenVideo.srcObject = await navigator.mediaDevices.getDisplayMedia({
      video: {
        width: 288,
        height: 240,
        frameRate: 15
      },
      audio: false
    });
    screenVideo.onloadedmetadata = () => {
      setActiveVideo('screen');
      startStreaming('screen');
    };
  } catch (err) {
    console.error("Error: " + err);
  }
};

stopScreenButton.onclick = () => stopStreaming('screen');

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

function updateTimerDisplay(minutes) {
  if (minutes == 0) {
    timerMinutesDisplay.textContent = 'Off';
  } else {
    timerMinutesDisplay.textContent = `${minutes} minutes`;
  }
}

timerMinutesSlider.addEventListener('input', (event) => {
  updateTimerDisplay(event.target.value);
});

// Initial setup
window.onload = async () => {
  const isOnDevice = !!document.getElementById('tab-streaming');

  if (isOnDevice) {
    // Full UI initialization
    const success = await fetchSettings();
    if (success) {
      fetchBatteryStatus();
      setInterval(fetchBatteryStatus, 10000);
    }
    if (!success || apMode) {
      streamingTabLabel.style.display = 'none';
      settingsTabRadio.checked = true;
    } else {
      connectWebSocket();
      setActiveVideo('file');
    }

    if (!navigator.mediaDevices || !navigator.mediaDevices.getDisplayMedia) {
      document.getElementById('screenShareLabel').style.display = 'none';
      document.getElementById('screenShareWarning').style.display = 'block';
    }
    document.querySelector('.tabs').style.display = 'flex';
    splashscreen.style.display = 'none';
  } else {
    // Screen sharing page initialization
    const connectButton = document.getElementById('connectButton');
    const deviceIpInput = document.getElementById('deviceIp');
    const streamingControls = document.getElementById('streamingControls');

    connectButton.onclick = () => {
      const ip = deviceIpInput.value;
      if (ip) {
        connectWebSocket(ip);
        streamingControls.style.display = 'block';
        connectButton.style.display = 'none';
        deviceIpInput.style.display = 'none';
        setActiveVideo('screen');
      } else {
        alert('Please enter a device IP address.');
      }
    };
  }
};
