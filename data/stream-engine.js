/**
 * BitStream Stream Engine — 共享推流引擎
 * 
 * 提供帧编码 (FrameEncoder) 与流控协议 (StreamProtocol)，
 * 供 index.html（在线控制台）和 offline.html（离线配网控制台）共用。
 * 
 * @version 2.1.0
 */

'use strict';

/* ========================================================================== */
/*  FrameEncoder — Canvas 抓帧 + 灰度二值化 + XBM 编码                         */
/* ========================================================================== */
class FrameEncoder {
    /**
     * @param {number} width  - 输出宽度 (默认 128)
     * @param {number} height - 输出高度 (默认 64)
     */
    constructor(width = 128, height = 64) {
        this.W = width;
        this.H = height;
        this.FRAME_BYTES = (width * height) / 8;  // 1024 for 128x64
    }

    /**
     * 从 <video> 抓取一帧并编码为 XBM Uint8Array
     * @param {HTMLVideoElement} video
     * @param {CanvasRenderingContext2D} ctx     - 隐藏 canvas 的 2D 上下文
     * @param {Object}             [opts]
     * @param {number} [opts.threshold=128]      - 灰度二值化阈值 (0-255)
     * @param {boolean}[opts.greenScreen=false]  - 绿幕过滤
     * @param {boolean}[opts.invert=false]       - 反色
     * @returns {Uint8Array|null} 1024 字节 XBM，失败返回 null
     */
    encode(video, ctx, opts = {}) {
        if (!video || video.readyState < 2) return null;

        const th   = opts.threshold ?? 128;
        const gs   = opts.greenScreen ?? false;
        const inv  = opts.invert ?? false;

        // Step 1: Draw video to canvas
        ctx.drawImage(video, 0, 0, this.W, this.H);

        // Step 2: Read pixels
        const imgData = ctx.getImageData(0, 0, this.W, this.H);
        const px = imgData.data;
        const xbm = new Uint8Array(this.FRAME_BYTES);

        // Step 3: Integer grayscale + threshold
        for (let y = 0; y < this.H; y++) {
            const ro = y * this.W * 4;
            for (let bx = 0; bx < (this.W / 8); bx++) {
                let bv = 0;
                for (let bit = 0; bit < 8; bit++) {
                    const i = ro + (bx * 8 + bit) * 4;
                    const r = px[i], g = px[i + 1], b = px[i + 2];

                    let bright;
                    if (gs && g > r * 1.3 && g > b * 1.3) {
                        bright = 0;
                    } else {
                        bright = ((r * 299 + g * 587 + b * 114) / 1000) > th ? 1 : 0;
                    }
                    if (inv) bright ^= 1;
                    if (bright) bv |= (1 << bit);
                }
                xbm[y * (this.W / 8) + bx] = bv;
            }
        }

        return xbm;
    }

    /**
     * 将 XBM 渲染到预览 canvas（用于 OLED 模拟显示）
     * @param {Uint8Array} xbm
     * @param {CanvasRenderingContext2D} previewCtx
     */
    renderPreview(xbm, previewCtx) {
        const pd = previewCtx.createImageData(this.W, this.H);
        const pp = pd.data;
        for (let y = 0; y < this.H; y++) {
            for (let bx = 0; bx < (this.W / 8); bx++) {
                const bv = xbm[y * (this.W / 8) + bx];
                for (let bit = 0; bit < 8; bit++) {
                    const idx = (y * this.W + bx * 8 + bit) * 4;
                    const v = (bv >> bit) & 1 ? 255 : 0;
                    pp[idx] = pp[idx + 1] = pp[idx + 2] = v;
                    pp[idx + 3] = 255;
                }
            }
        }
        previewCtx.putImageData(pd, 0, 0);
    }
}

/* ========================================================================== */
/*  StreamProtocol — 序号 + 滑动窗口背压 + ACK 解析                             */
/* ========================================================================== */
class StreamProtocol {
    /**
     * @param {number} [windowSize=3]  - 滑动窗口大小（未确认帧数上限）
     * @param {number} [maxFps=20]     - 发送频率上限
     */
    constructor(windowSize = 3, maxFps = 20) {
        this.WINDOW    = windowSize;
        this.MIN_INTERVAL = 1000 / maxFps;  // ms between sends

        this.reset();
    }

    /** 重置所有状态（开始/停止推流时调用） */
    reset() {
        this.nextSeq        = 0;
        this.inflight       = 0;
        this.lastAcked      = -1;
        this.droppedFrames  = 0;
        this.throttledFrames = 0;
        this.lastSendTime   = 0;
        this.lastAckTime    = 0;
        this.rttEstimate    = 0;
        this.totalSent      = 0;
        this._ackHistory    = [];  // 用于 RTT 滑动平均
    }

    /**
     * 检查是否可以发送新帧（窗口未满 + 未超过频率上限）
     * @param {number} [now] - performance.now() 值（可选，不传则内部获取）
     * @returns {{ok: boolean, reason: string}}
     */
    canSend(now) {
        now = now || performance.now();

        if (this.inflight >= this.WINDOW) {
            this.droppedFrames++;
            return { ok: false, reason: 'window_full' };
        }
        if (now - this.lastSendTime < this.MIN_INTERVAL) {
            this.throttledFrames++;
            return { ok: false, reason: 'throttled' };
        }
        return { ok: true, reason: '' };
    }

    /**
     * 打包 1028 字节帧：[4B seq (小端序)] + [1024B XBM]
     * @param {Uint8Array} xbm - 1024 字节位图
     * @returns {ArrayBuffer} 1028 字节
     */
    packFrame(xbm) {
        const buf = new ArrayBuffer(1028);
        const view = new DataView(buf);
        view.setUint32(0, this.nextSeq, true);  // 小端序
        new Uint8Array(buf, 4, 1024).set(xbm);
        return buf;
    }

    /** 帧已发送后调用，更新计数器 */
    onSent(now) {
        now = now || performance.now();
        this.lastSendTime = now;
        this.inflight++;
        this.nextSeq++;
        this.totalSent++;
    }

    /**
     * 解析 ESP 返回的 ACK 消息
     * @param {string} data - WebSocket 文本消息
     * @returns {{handled: boolean, seq: number}}
     */
    onMessage(data) {
        if (typeof data !== 'string' || !data.startsWith('ACK:')) {
            return { handled: false, seq: -1 };
        }

        const now = performance.now();
        const seq = parseInt(data.substring(4), 10);

        this.lastAckTime = now;

        if (seq > this.lastAcked) {
            const delta = seq - this.lastAcked;
            this.inflight -= delta;
            if (this.inflight < 0) this.inflight = 0;
            this.lastAcked = seq;

            // 滑动平均 RTT 估算
            this._ackHistory.push(now);
            if (this._ackHistory.length > 8) this._ackHistory.shift();
        }

        return { handled: true, seq };
    }

    /** 获取实时指标 */
    getMetrics() {
        return {
            nextSeq:    this.nextSeq,
            inflight:   this.inflight,
            lastAcked:  this.lastAcked,
            dropped:    this.droppedFrames,
            throttled:  this.throttledFrames,
            totalSent:  this.totalSent,
            windowUse:  ((this.inflight / this.WINDOW) * 100).toFixed(0) + '%',
            rttMs:      this.rttEstimate > 0 ? this.rttEstimate.toFixed(1) : '--'
        };
    }
}
'

/* ========================================================================== */
/*  [P2-1] VideoFrameSync — requestVideoFrameCallback 精确帧同步              */
/* ========================================================================== */
class VideoFrameSync {
    /**
     * 在支持 rVFC 的浏览器中使用 requestVideoFrameCallback，
     * 否则回退到 requestAnimationFrame。
     *
     * @param {HTMLVideoElement} video
     * @param {function(number, object):void} callback - (now, metadata)
     */
    static startLoop(video, callback) {
        if (video.requestVideoFrameCallback) {
            const self = {};
            self.running = true;
            const rvfc = (now, md) => {
                if (!self.running) return;
                callback(now, md);
                video.requestVideoFrameCallback(rvfc);
            };
            video.requestVideoFrameCallback(rvfc);
            return self;
        }

        // fallback to rAF
        const self = {};
        self.running = true;
        const raf = (ts) => {
            if (!self.running) return;
            callback(ts, null);
            self.rafId = requestAnimationFrame(raf);
        };
        self.rafId = requestAnimationFrame(raf);
        return self;
    }

    static stopLoop(handle) {
        if (!handle) return;
        handle.running = false;
        if (handle.rafId) cancelAnimationFrame(handle.rafId);
    }
}

/* ========================================================================== */
/*  [P1-2] StreamMetrics — 从 StreamProtocol 提取可展示指标                    */
/* ========================================================================== */
class StreamMetrics {
    /**
     * @param {StreamProtocol} protocol
     */
    constructor(protocol) {
        this._proto = protocol;
    }

    /** @returns {Object} */
    snapshot() {
        const p = this._proto;
        return {
            inflight:    p.inflight,
            windowSize:  p.WINDOW,
            dropped:     p.droppedFrames,
            throttled:   p.throttledFrames,
            totalSent:   p.totalSent,
            lastAcked:   p.lastAcked,
            rttMs:       p.rttEstimate > 0 ? p.rttEstimate.toFixed(1) : '--'
        };
    }
}
