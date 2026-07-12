/** 图层项 */
export interface LayerItem {
  id: string;
  name: string;
  zIndex: number;
  visible: boolean;
  locked: boolean;
  type: 'component' | 'group';
  children?: LayerItem[];
}

/** 图层状态 */
export interface LayerState {
  layers: LayerItem[];
  activeLayerId: string | null;
}

/** 图层操作 */
export type LayerAction =
  | { type: 'ADD_LAYER'; payload: LayerItem }
  | { type: 'REMOVE_LAYER'; payload: { id: string } }
  | { type: 'REORDER'; payload: { fromIndex: number; toIndex: number } }
  | { type: 'TOGGLE_VISIBILITY'; payload: { id: string } }
  | { type: 'TOGGLE_LOCK'; payload: { id: string } }
  | { type: 'SELECT_LAYER'; payload: { id: string | null } }
  | { type: 'SET_LAYERS'; payload: LayerItem[] };
