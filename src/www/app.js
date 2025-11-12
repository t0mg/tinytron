const videoFile = document.getElementById('videoFile');
const video = document.getElementById('video');
const startButton = document.getElementById('startButton');
const stopButton = document.getElementById('stopButton');
const previewImage = document.getElementById('previewImage');
const brightnessSlider = document.getElementById('brightness');
const batteryVoltageDisplay = document.getElementById('batteryVoltageDisplay');

// fetch the initial brightness and set the slider
fetch('/brightness')
  .then(response => response.text())
  .then(brightness => {
    brightnessSlider.value = brightness;
  });

// send the brightness to the server when the slider is changed
brightnessSlider.addEventListener('change', () => {
  fetch('/brightness', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded'
    },
    body: `brightness=${brightnessSlider.value}`
  });
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

// Fetch battery voltage on page load
fetchBatteryVoltage();

// Refresh battery voltage every minute
setInterval(fetchBatteryVoltage, 60000);


// New UI elements
const scaleFactorSelect = document.getElementById('scaleFactor');
const jpegQualitySlider = document.getElementById('jpegQuality');
const scalingModeSelect = document.getElementById('scalingMode');
const fpsDisplay = document.getElementById('fpsDisplay');
const frameSizeDisplay = document.getElementById('frameSizeDisplay');

let ws;
let videoFrameId;

// Frame rate and stats
let fpsInterval;
let lastFrameTime;
let frameTimeBuffer;

function connectWebSocket() {
    ws = new WebSocket(`ws://${window.location.host}/ws`);
    ws.onopen = () => console.log("WebSocket connection established");
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
}

videoFile.addEventListener('change', () => {
    const file = videoFile.files[0];
    if (file) {
        video.src = URL.createObjectURL(file);
    }
});

function sendFrame() {
    if (video.paused || video.ended) {
        return; // Stop sending when video is paused or finished
    }

    const scale = parseFloat(scaleFactorSelect.value);
    const jpegQuality = parseFloat(jpegQualitySlider.value);
    const scalingMode = scalingModeSelect.value;

    // Create a canvas to draw the frame on
    const canvas = document.createElement('canvas');
    let baseWidth = 288;
    let baseHeight = 240;
    canvas.width = baseWidth * scale;
    canvas.height = baseHeight * scale;
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

    // Get the JPEG data as a Blob
    canvas.toBlob(blob => {
        if (blob) {
            const now = performance.now();
            if (lastFrameTime) {
                const frameTime = now - lastFrameTime;
                console.log("Frame time:", frameTime.toFixed(2), "ms"); // Debug
                
                // Ne garder que les intervalles raisonnables (éviter les valeurs aberrantes)
                if (frameTime > 0 && frameTime < 1000) { // Ignorer les intervalles > 1s
                    frameTimeBuffer.push(frameTime);
                    console.log("FPS Buffer size:", frameTimeBuffer.length); // Debug
                }
            }
            lastFrameTime = now;
            
            frameSizeDisplay.textContent = blob.size;

            // Display the blob on the client side for verification
            const imageUrl = URL.createObjectURL(blob);
            previewImage.src = imageUrl;
            previewImage.onload = () => {
                URL.revokeObjectURL(imageUrl); // Clean up the object URL
            };

            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(blob); // Send the binary data
            }
        } else {
            console.error("Failed to create JPEG blob.");
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
    console.log("Starting stream...");

    // Reset and start FPS counter
    lastFrameTime = performance.now();
    frameTimeBuffer = [];
    
    // Debug log pour vérifier l'initialisation
    console.log("Starting FPS counter, initial time:", lastFrameTime);
    
    fpsInterval = setInterval(() => {
        if (frameTimeBuffer && frameTimeBuffer.length > 0) {
            console.log("Calculating FPS from", frameTimeBuffer.length, "frames"); // Debug
            
            // Calculer la moyenne des intervalles entre les trames
            const avgInterval = frameTimeBuffer.reduce((a, b) => a + b) / frameTimeBuffer.length;
            const currentFps = 1000 / avgInterval; // Convertir l'intervalle en FPS
            
            console.log("Average interval:", avgInterval.toFixed(2), "ms, FPS:", currentFps.toFixed(1)); // Debug
            
            fpsDisplay.textContent = currentFps.toFixed(1);
            
            // Réinitialiser le buffer pour la prochaine période
            frameTimeBuffer = [];
        } else {
            console.log("No frames in buffer"); // Debug
            fpsDisplay.textContent = '0.0';
        }
    }, 1000);

    video.play();
    ws.send("START");
};

stopButton.onclick = () => {
    console.log("Stopping stream...");
    if (videoFrameId) {
        video.cancelVideoFrameCallback(videoFrameId); // Cancel the request(videoFrameId); // Stop the animation loop
        videoFrameId = null;
    }
    video.pause(); // Pause the video
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send("STOP"); // Send a message to ESP32 to stop video player
    }

    // Clear FPS counter
    clearInterval(fpsInterval);
    fpsBuffer = [];
    fpsDisplay.textContent = '-';
    frameSizeDisplay.textContent = '-';
};

connectWebSocket();
