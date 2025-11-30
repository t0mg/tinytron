class Streamer {
  constructor(videoElement, previewImage, fpsUpdateCallback, frameSizeUpdateCallback) {
    this.video = videoElement;
    this.previewImage = previewImage;

    this.fpsUpdateCallback = fpsUpdateCallback || function() {};
    this.frameSizeUpdateCallback = frameSizeUpdateCallback || function() {};

    this.scalingMode = 'letterbox';
    this.jpegQuality = 0.5;

    this.ws = null;
    this.videoFrameId = null;
    this.fpsInterval = null;
    this.lastFrameTime = null;
    this.frameTimeBuffer = [];

    this.sendFrame = this.sendFrame.bind(this);
  }

  connectWebSocket(host, onOpen, onError) {
    if (!host) {
      host = window.location.host;
    }
    try {
      this.ws = new WebSocket(`ws://${host}/ws`);
      this.ws.onopen = () => {
        console.log("WebSocket connection established");
        if (onOpen) {
          onOpen();
        }
      };
      this.ws.onmessage = (event) => {
        if (event.data === "ready") {
          this.video.requestVideoFrameCallback(this.sendFrame);
        }
      };
      this.ws.onclose = () => {
        console.log("WebSocket connection closed, retrying...");
        setTimeout(() => this.connectWebSocket(host, onOpen), 1000);
      };
      this.ws.onerror = (error) => {
        console.error("WebSocket error:", error);
        onError && onError(error);
      }
    } catch (e) {
      console.warn("WebSocket connection failed.", e);
    }
  }

  sendFrame() {
    if (this.video.paused || this.video.ended) {
      return;
    }
    const canvas = document.createElement('canvas');
    canvas.width = 288;
    canvas.height = 240;
    const context = canvas.getContext('2d');
    const videoAspectRatio = this.video.videoWidth / this.video.videoHeight;
    const canvasAspectRatio = canvas.width / canvas.height;
    let sx = 0, sy = 0, sWidth = this.video.videoWidth, sHeight = this.video.videoHeight;
    let dx = 0, dy = 0, dWidth = canvas.width, dHeight = canvas.height;

    switch (this.scalingMode) {
      case 'letterbox':
        if (videoAspectRatio > canvasAspectRatio) {
          dHeight = canvas.width / videoAspectRatio;
          dy = (canvas.height - dHeight) / 2;
        } else {
          dWidth = canvas.height * videoAspectRatio;
          dx = (canvas.width - dWidth) / 2;
        }
        context.fillStyle = 'black';
        context.fillRect(0, 0, canvas.width, canvas.height);
        context.drawImage(this.video, sx, sy, sWidth, sHeight, dx, dy, dWidth, dHeight);
        break;
      case 'crop':
        if (videoAspectRatio > canvasAspectRatio) {
          sWidth = this.video.videoHeight * canvasAspectRatio;
          sx = (this.video.videoWidth - sWidth) / 2;
        } else {
          sHeight = this.video.videoWidth / canvasAspectRatio;
          sy = (this.video.videoHeight - sHeight) / 2;
        }
        context.drawImage(this.video, sx, sy, sWidth, sHeight, 0, 0, canvas.width, canvas.height);
        break;  
      case 'stretch':
        context.drawImage(this.video, 0, 0, canvas.width, canvas.height);
        break;
      default:
        console.warn(`Unknown scaling mode: ${this.scalingMode}, defaulting to 'letterbox'`);
        this.scalingMode = 'letterbox';
    }

    canvas.toBlob(blob => {
      if (blob) {
        const now = performance.now();
        if (this.lastFrameTime) {
          const frameTime = now - this.lastFrameTime;
          if (frameTime > 0 && frameTime < 1000) {
            this.frameTimeBuffer.push(frameTime);
          }
        }
        this.lastFrameTime = now;
        this.frameSizeUpdateCallback(blob.size);
        const imageUrl = URL.createObjectURL(blob);
        this.previewImage.src = imageUrl;
        this.previewImage.onload = () => URL.revokeObjectURL(imageUrl);
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
          this.ws.send(blob);
        }
      }
    }, 'image/jpeg', this.jpegQuality);
  }

  start() {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      alert("WebSocket is not connected. Please wait.");
      return;
    }
    this.lastFrameTime = performance.now();
    this.frameTimeBuffer = [];
    this.fpsInterval = setInterval(() => {
      if (this.frameTimeBuffer.length > 0) {
        const avgInterval = this.frameTimeBuffer.reduce((a, b) => a + b) / this.frameTimeBuffer.length;
        const currentFps = 1000 / avgInterval;
        this.fpsUpdateCallback(currentFps.toFixed(1));
        this.frameTimeBuffer = [];
      } else {
        this.fpsUpdateCallback(0);
      }
    }, 1000);
    this.video.play();
    this.ws.send("START");
  }

  stop() {
    if (this.videoFrameId) {
      this.video.cancelVideoFrameCallback(this.videoFrameId);
      this.videoFrameId = null;
    }
    this.video.pause();
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send("STOP");
    }
    clearInterval(this.fpsInterval);
    this.fpsUpdateCallback(null);
    this.frameSizeUpdateCallback(null);
  }
}
