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
const timerMinutesSlider = document.getElementById('timerMinutes');
const boardRevisionSelect = document.getElementById('boardRevision');
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
const videoSourceSelect = document.getElementById('videoSource');
const screenStreamOptions = document.getElementById('screenStreamOptions');
const fileStreamOptions = document.getElementById('fileStreamOptions');
const selectScreenButton = document.getElementById('selectScreenButton');

let lastSsid = '';
let apMode = false;
let streamer;
let batteryInterval = null;

function showSplashScreen(innerHTML) {
  splashscreen.innerHTML = innerHTML;
  splashscreen.style.display = 'flex';
}

// Fetch all settings from the server
async function fetchSettings() {
  let success = false;
  await fetch('/settings')
    .then(response => response.json())
    .then(settings => {
      ssidInput.value = lastSsid = settings.ssid;
      brightnessSlider.value = settings.brightness;
      osdLevelSelect.value = settings.osdLevel;
      if (settings.boardRevision !== undefined) {
        boardRevisionSelect.value = settings.boardRevision;
      } else {
        boardRevisionSelect.value = 2; // Default to V2
      }
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
    boardRevision: parseInt(boardRevisionSelect.value),
    timerMinutes: parseInt(timerMinutesSlider.value)
  };

  const networkUpdated = (settings.ssid !== lastSsid || settings.pass.length > 0);
  if (networkUpdated) {
    let networkMessage = `<h2>Network settings changed</h2>
    <p>The device will restart after saving the settings.</p>`;
    if (apMode) {
      clearInterval(batteryInterval);
      networkMessage += `
      <p>If successful, the "Tinytron" wifi network will disappear and the device
      will show "WiFi Connected" alongside a new IP address.</p>
      <p>You can then reconnect via your home network (${settings.ssid}) and access the 
      device via its new IP.</p>`;
    }
    networkMessage += `<p>Please wait about 20 seconds for the device to restart.
    If the connection fails, the device will revert to Access Point mode so you
    can connect to the "Tinytron" wifi network and try again.</p>`;
    showSplashScreen(networkMessage);
  }
  fetch('/settings', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(settings)
  }).then(() => {
    alert('Settings saved!');
  }).catch(error => {
    if (!networkUpdated) {
      // Only show error if network settings were not updated to avoid conusion
      console.error('Error saving settings:', error);
      alert('Failed to save settings.');
    }
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

function clearVideoSource() {
  if (video.srcObject) {
    video.srcObject.getTracks().forEach(track => track.stop());
    video.srcObject = null;
  } else if (video.src) {
    video.src = '';
  }
}

let source = 'screen';
videoSourceSelect.addEventListener('change', (e) => {
  source = e.target.value;
  clearVideoSource();
  if (source === 'screen') {
    screenStreamOptions.style.display = '';
    fileStreamOptions.style.display = 'none';
  } else if (source === 'file') {
    screenStreamOptions.style.display = 'none';
    fileStreamOptions.style.display = '';
  }
});

jpegQualitySlider.addEventListener('input', (e) => {
  if (streamer) {
    const quality = e.target.value;
    streamer.jpegQuality = quality;
  }
});

scalingModeSelect.addEventListener('input', (e) => {
  if (streamer) {
    const mode = e.target.value;
    streamer.scalingMode = mode;
  }
});

videoFile.addEventListener('change', () => {
  const file = videoFile.files[0];
  if (streamer) {
    streamer.stop();
  }
  if (file) {
    video.src = URL.createObjectURL(file);
  }
});

selectScreenButton.onclick = async () => {
  try {
  const stream = await navigator.mediaDevices.getDisplayMedia({ video: true });
  video.srcObject = stream;
  video.play();
  } catch (error) {
    console.error('Error accessing screen:', error);
    alert('Failed to access screen. Please read the documentation about enabling insecure connections.');
  } 
};

startButton.onclick = () => {
  if (!video.src && !video.srcObject) {
    alert("Please select a video source first.");
    return;
  }
  streamer.start();
  stopButton.style.display = '';
  startButton.style.display = 'none';
  videoSourceSelect.disabled = true;
};

stopButton.onclick = () => {
  if (streamer) {
    streamer.stop();
  }
  stopButton.style.display = 'none';
  startButton.style.display = '';
  videoSourceSelect.disabled = false;
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
  const success = await fetchSettings();
  if (success) {
    fetchBatteryStatus();
    batteryInterval = setInterval(fetchBatteryStatus, 10000);
  }
  if (!success || apMode) {
    streamingTabLabel.style.display = 'none';
    settingsTabRadio.checked = true;
  } else {
    const onFpsUpdate = (fps) => { fpsDisplay.textContent = fps === null ? '-' : `${fps}`; };
    const onFrameSizeUpdate = (frameSize) => {
      frameSizeDisplay.textContent = frameSize === null ? '-' : `${(frameSize/1000).toFixed(1)} kB`;
    };
    streamer = new Streamer(video, previewImage, onFpsUpdate, onFrameSizeUpdate);
    streamer.connectWebSocket(null, () => {
      startButton.disabled = false;
    }, (error) => {
      startButton.disabled = true;
      videoSourceSelect.disabled = false;
    });
  }
  document.querySelector('.tabs').style.display = 'flex';
  splashscreen.style.display = 'none';
}