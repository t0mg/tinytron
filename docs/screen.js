// UI elements from screen.html
const deviceIpInput = document.getElementById('deviceIp');
const video = document.getElementById('video');
const startButton = document.getElementById('startButton');
const stopButton = document.getElementById('stopButton');
const previewImage = document.getElementById('previewImage');
const fpsDisplay = document.getElementById('fpsDisplay');
const frameSizeDisplay = document.getElementById('frameSizeDisplay');

let streamer;

startButton.onclick = async () => {
  const deviceIp = deviceIpInput.value.trim();
  if (!deviceIp) {
    alert("Please enter the device IP address.");
    return;
  }

  try {
    // 1. Get screen capture stream
    const stream = await navigator.mediaDevices.getDisplayMedia({ video: true });
    video.srcObject = stream;
    video.play();

    // Disable the input while streaming
    deviceIpInput.disabled = true;

    // 2. Instantiate and connect the streamer
    streamer = new Streamer(
      video,
      previewImage,
      (fps) => {
        fpsDisplay.textContent = fps === null ? '-' : `${fps}`;
      },
      (frameSize) => {
        frameSizeDisplay.textContent = frameSize === null ? '-' : `${frameSize}`;
      }
    );

    // Set scaling mode and JPEG quality
    streamer.scalingMode = 'crop';
    streamer.jpegQuality = 0.5;

    streamer.connectWebSocket(deviceIp, () => {
      streamer.start();
    }, (error) => {
      alert("WebSocket connection failed. Please verify you have enabled insecure connection in the browser flags and that the device IP is correct.");
    });

    stopButton.style.display = '';
    startButton.style.display = 'none';

  } catch (error) {
    console.error("Error starting screen capture:", error);
    alert("Could not start screen capture. Please ensure you grant permission.");

    stopButton.style.display = 'none';
    startButton.style.display = '';
    deviceIpInput.disabled = false;
  }
};

stopButton.onclick = () => {
  if (streamer) {
    streamer.stop();
  }

  // Stop the screen capture tracks
  if (video.srcObject) {
    video.srcObject.getTracks().forEach(track => track.stop());
    video.srcObject = null;
  }

  stopButton.style.display = 'none';
  startButton.style.display = '';
  deviceIpInput.disabled = false;
};
