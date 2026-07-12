/** 组件唯一标识 */
export type ComponentId = string;

/** 组件在画布上的实例 */
export interface CanvasComponent {
  id: ComponentId;
  tagName: string;
  name: string;
  x: number;
  y: number;
  width: number;
  height: number;
  rotation: number;
  zIndex: number;
  visible: boolean;
  locked: boolean;
  props: Record<string, unknown>;
}
