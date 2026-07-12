/**
 * 传输适配器工厂
 * 根据运行环境自动选择合适的 TransportAdapter 实现
 */

import { TransportAdapter } from './TransportAdapter';
import { PostMessageTransport } from './PostMessageTransport';
import { WorkerTransport } from './WorkerTransport';
import { SharedWorkerTransport } from './SharedWorkerTransport';

export type TransportType = 'postmessage' | 'worker' | 'sharedworker';

export interface TransportFactoryOptions {
  type: TransportType;
  /** iframe postMessage 模式所需参数 */
  iframeWindow?: Window;
  targetOrigin?: string;
  /** Worker 模式所需参数 */
  worker?: Worker;
  /** SharedWorker 模式所需参数 */
  sharedWorker?: SharedWorker;
}

/**
 * 创建 TransportAdapter 实例
 * 根据传入的 type 自动选择实现
 */
export function createTransport(options: TransportFactoryOptions): TransportAdapter {
  switch (options.type) {
    case 'postmessage': {
      if (!options.iframeWindow) {
        throw new Error('[TransportFactory] postmessage 模式需要 iframeWindow 参数');
      }
      return new PostMessageTransport({
        targetWindow: options.iframeWindow,
        targetOrigin: options.targetOrigin ?? '*',
      });
    }

    case 'worker': {
      if (!options.worker) {
        throw new Error('[TransportFactory] worker 模式需要 worker 参数');
      }
      return new WorkerTransport({ worker: options.worker });
    }

    case 'sharedworker': {
      if (!options.sharedWorker) {
        throw new Error('[TransportFactory] sharedworker 模式需要 sharedWorker 参数');
      }
      return new SharedWorkerTransport({ sharedWorker: options.sharedWorker });
    }

    default:
      throw new Error(`[TransportFactory] 未知的传输类型: ${options.type}`);
  }
}

/**
 * 自动检测适合的传输类型
 * 优先使用 SharedWorker（多页面共享），回退到 Worker，最后使用 iframe postMessage
 */
export function detectTransportType(): TransportType {
  if (typeof SharedWorker !== 'undefined') {
    return 'sharedworker';
  }
  if (typeof Worker !== 'undefined') {
    return 'worker';
  }
  return 'postmessage';
}
