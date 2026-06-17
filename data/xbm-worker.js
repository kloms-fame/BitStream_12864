/**
 * BitStream XBM Encoder — Web Worker
 * 
 * 将 ImageData 编码为 XBM 位图（1024 字节），
 * 移出主线程，避免编码阻塞 UI。
 * 
 * @version 2.1.0
 */

'use strict';

const W = 128, H = 64, FRAME_BYTES = 1024;

self.onmessage = function(e) {
    const { pixels, threshold, greenScreen, invert } = e.data;
    const th  = threshold ?? 128;
    const gs  = greenScreen ?? false;
    const inv = invert  ?? false;

    const xbm = new Uint8Array(FRAME_BYTES);

    for (let y = 0; y < H; y++) {
        const ro = y * W * 4;
        for (let bx = 0; bx < 16; bx++) {
            let bv = 0;
            for (let bit = 0; bit < 8; bit++) {
                const i = ro + (bx * 8 + bit) * 4;
                const r = pixels[i], g = pixels[i + 1], b = pixels[i + 2];

                let bright;
                if (gs && g > r * 1.3 && g > b * 1.3) {
                    bright = 0;
                } else {
                    bright = ((r * 299 + g * 587 + b * 114) / 1000) > th ? 1 : 0;
                }
                if (inv) bright ^= 1;
                if (bright) bv |= (1 << bit);
            }
            xbm[y * 16 + bx] = bv;
        }
    }

    // Transfer the result buffer back (zero-copy)
    self.postMessage({ xbm: xbm.buffer }, [xbm.buffer]);
};
