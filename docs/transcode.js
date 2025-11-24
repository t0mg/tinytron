// Using ES modules for ffmpeg.wasm
import { FFmpeg } from './ffmpeg/index.js';
import { toBlobURL } from './ffmpeg-util/index.js';

document.addEventListener('DOMContentLoaded', () => {
    const transcodeVideoFile = document.getElementById('transcodeVideoFile');
    const transcodeButton = document.getElementById('transcodeButton');
    const transcodeProgress = document.getElementById('transcodeProgress');
    const transcodeStatus = document.getElementById('transcodeStatus');
    const downloadLink = document.getElementById('downloadLink');

    let ffmpeg;

    const loadFFmpeg = async () => {
        transcodeStatus.textContent = 'Loading ffmpeg-core.js';
        ffmpeg = new FFmpeg();
        ffmpeg.on('log', ({ message }) => {
            console.log(message);
        });
        ffmpeg.on('progress', ({ progress, time }) => {
            transcodeProgress.value = progress * 100;
        });

        const coreURL = await toBlobURL('https://unpkg.com/@ffmpeg/core@0.12.6/dist/esm/ffmpeg-core.js', 'text/javascript');
        const wasmURL = await toBlobURL('https://unpkg.com/@ffmpeg/core@0.12.6/dist/esm/ffmpeg-core.wasm', 'application/wasm');

        await ffmpeg.load({
            coreURL,
            wasmURL
        });
        transcodeStatus.textContent = 'FFmpeg loaded.';
        transcodeButton.disabled = false;
    };

    transcodeButton.addEventListener('click', async () => {
        if (!transcodeVideoFile.files || transcodeVideoFile.files.length === 0) {
            alert('Please select a video file first.');
            return;
        }

        const file = transcodeVideoFile.files[0];
        transcodeButton.disabled = true;
        transcodeProgress.style.display = 'block';
        transcodeProgress.value = 0;
        downloadLink.style.display = 'none';
        transcodeStatus.textContent = 'Transcoding...';

        await ffmpeg.writeFile('input.mp4', new Uint8Array(await file.arrayBuffer()));

        const command = [
            '-y',
            '-i',
            'input.mp4',
            '-an',
            '-c:v',
            'mjpeg',
            '-q:v',
            '10',
            '-vf',
            'scale=-1:240:flags=lanczos,crop=288:240:(in_w-288)/2:0,fps=min(25, original_fps)',
            'out.avi'
        ];

        await ffmpeg.exec(command);

        const data = await ffmpeg.readFile('out.avi');
        const blob = new Blob([data.buffer], { type: 'video/avi' });
        const url = URL.createObjectURL(blob);

        downloadLink.href = url;
        downloadLink.download = 'out.avi';
        downloadLink.style.display = 'block';
        transcodeStatus.textContent = 'Transcoding complete!';
        transcodeButton.disabled = false;
        transcodeProgress.style.display = 'none';
    });

    loadFFmpeg();
});