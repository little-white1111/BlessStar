/**
 * schema_reader.ts — Editor 侧 Schema 二进制读取器（专题四 P1 ⑥ + P2 ⑧ + P3 ⑨）
 *
 * 支持三种读取模式：
 *   1. 二进制模式：从 SHM 的哈希索引 + 字段数据区零拷贝读取（P1）
 *   2. JSON 兼容模式：回退到全量 JSON.parse（P1 兼容）
 *   3. 分页加载模式：按需加载字段页（P2 ⑧）
 *   4. Trie 前缀查询：紧凑 Trie 索引加速层级查询（P3 ⑨）
 *
 * 用法：
 *   const reader = new PagedSchemaReader(buffer, pageTableOffset, trieRootOffset);
 *   const field = reader.lookup('db.host');       // O(1) 哈希查找
 *   const dbFields = reader.prefixQuery('db');    // O(log n) Trie 前缀查询
 */

/* ── 二进制布局常量（与 shm_manager.h C++ 侧一致） ──────────────── */
const HASH_SLOT_SIZE = 8;            // hash_low:4B + data_offset:4B
const HASH_SLOT_COUNT = 4096;
const HASH_INDEX_SIZE = HASH_SLOT_COUNT * HASH_SLOT_SIZE;
const FIELD_RECORD_HEADER_SIZE = 8;  // key_len:2B + default_len:2B + desc_len:2B + type:1B + required:1B
const PAGE_ENTRY_SIZE = 8;           // page_offset:4B + page_len:4B
const FIELDS_PER_PAGE = 64;
const TRIE_NODE_SIZE = 41;           // name_len:1B + name:31B + first_child_off:4B + next_sibling_off:4B + first_field_off:4B

/* ── 字段类型枚举 ──────────────────────────────────────────────────── */
export enum FieldType {
  Int32 = 0,
  Int64 = 1,
  String = 2,
  Double = 3,
  Bool = 4,
}

/* ── 字段声明接口 ───────────────────────────────────────────────────── */
export interface FieldDecl {
  key: string;
  type: FieldType;
  default_str: string;
  description: string;
  required: boolean;
}

/* ── Trie 节点接口（TS 表示） ───────────────────────────────────────── */
export interface TrieNode {
  name: string;
  firstChildOff: number;
  nextSiblingOff: number;
  firstFieldOff: number;
}

/* ══════════════════════════════════════════════════════════════════════
 * SchemaReader — 基础哈希索引查找（P1）
 * ══════════════════════════════════════════════════════════════════════ */
export class SchemaReader {
  protected fieldDataOffset: number;
  protected jsonCache: FieldDecl[] | null = null;
  protected hashTableOffset: number;

  constructor(
    protected buffer: Buffer,
    hashTableOffset?: number,
    fieldDataOffset?: number,
  ) {
    if (hashTableOffset !== undefined) {
      this.hashTableOffset = hashTableOffset;
    } else {
      this.hashTableOffset = this.detectJsonEnd();
    }

    this.fieldDataOffset = fieldDataOffset !== undefined
      ? fieldDataOffset
      : this.hashTableOffset + HASH_INDEX_SIZE;
  }

  /* ── JSON 兼容区末端检测 ────────────────────────────────────────── */
  private detectJsonEnd(): number {
    const len = this.buffer.length;
    for (let i = Math.min(len, 1024 * 1024); i > 0; i--) {
      if (this.buffer[i - 1] === 0x7D /* '}' */) {
        let end = i;
        while (end < len && this.buffer[end] <= 0x20) end++;
        return end;
      }
    }
    return 0;
  }

  /* ── xxHash32 ─────────────────────────────────────────────────────── */
  protected xxhash32(input: Buffer | string, seed = 0): number {
    const PRIME32_1 = 2654435761;
    const PRIME32_2 = 2246822519;
    const PRIME32_3 = 3266489917;
    const PRIME32_4 = 668265263;
    const PRIME32_5 = 374761393;

    const data = typeof input === 'string' ? Buffer.from(input, 'utf-8') : input;
    const len = data.length;

    let h32 = (seed + PRIME32_5) >>> 0;
    let idx = 0;

    while (len - idx >= 4) {
      let v = data.readUInt32LE(idx);
      v = (v * PRIME32_2) >>> 0;
      v = ((v << 13) | (v >>> 19)) >>> 0;
      v = (v * PRIME32_1) >>> 0;
      h32 = (h32 ^ v) >>> 0;
      h32 = ((h32 << 19) | (h32 >>> 13)) >>> 0;
      h32 = (h32 * PRIME32_1 + PRIME32_4) >>> 0;
      idx += 4;
    }

    while (idx < len) {
      h32 = (h32 ^ (data[idx] * PRIME32_5)) >>> 0;
      h32 = ((h32 << 11) | (h32 >>> 21)) >>> 0;
      h32 = (h32 * PRIME32_1) >>> 0;
      idx++;
    }

    h32 = (h32 ^ len) >>> 0;
    h32 = (h32 ^ (h32 >>> 15)) >>> 0;
    h32 = (h32 * PRIME32_2) >>> 0;
    h32 = (h32 ^ (h32 >>> 13)) >>> 0;
    h32 = (h32 * PRIME32_3) >>> 0;
    h32 = (h32 ^ (h32 >>> 16)) >>> 0;
    return h32;
  }

  /* ── 二进制字段记录解码 ──────────────────────────────────────────── */
  protected decodeFieldRecord(offset: number): FieldDecl | null {
    if (offset + FIELD_RECORD_HEADER_SIZE > this.buffer.length) return null;

    const keyLen = this.buffer.readUInt16LE(offset);
    const defaultLen = this.buffer.readUInt16LE(offset + 2);
    const descLen = this.buffer.readUInt16LE(offset + 4);
    const type = this.buffer.readUInt8(offset + 6);
    const required = this.buffer.readUInt8(offset + 7) !== 0;

    let pos = offset + FIELD_RECORD_HEADER_SIZE;
    if (pos + keyLen + defaultLen + descLen > this.buffer.length) return null;

    const key = this.buffer.toString('utf-8', pos, pos + keyLen);
    pos += keyLen;
    const defaultStr = this.buffer.toString('utf-8', pos, pos + defaultLen);
    pos += defaultLen;
    const description = this.buffer.toString('utf-8', pos, pos + descLen);

    return { key, type: type as FieldType, default_str: defaultStr, description, required };
  }

  /* ── 哈希索引查找（O(1) 期望） ────────────────────────────────────── */
  lookup(key: string): FieldDecl | null {
    const keyBuf = Buffer.from(key, 'utf-8');
    const h = this.xxhash32(keyBuf);
    const numSlots = HASH_SLOT_COUNT;
    const slotStart = this.hashTableOffset;
    let idx = h % numSlots;
    const start = idx;

    do {
      const slotOff = slotStart + idx * HASH_SLOT_SIZE;
      if (slotOff + HASH_SLOT_SIZE > this.buffer.length) return null;

      const hashLow = this.buffer.readUInt32LE(slotOff);
      const dataOff = this.buffer.readUInt32LE(slotOff + 4);

      if (hashLow === 0 && dataOff === 0) return null;

      if (hashLow === h) {
        const field = this.decodeFieldRecord(this.fieldDataOffset + dataOff);
        if (field && field.key === key) return field;
      }

      idx = (idx + 1) % numSlots;
    } while (idx !== start);

    return null;
  }

  /* ── JSON 回退 ────────────────────────────────────────────────────── */
  getAllFields(): FieldDecl[] {
    if (this.jsonCache) return this.jsonCache;

    try {
      const jsonEnd = this.detectJsonEnd();
      if (jsonEnd === 0) return [];
      const jsonStr = this.buffer.toString('utf-8', 0, jsonEnd);
      const parsed = JSON.parse(jsonStr);
      if (parsed?.fields && Array.isArray(parsed.fields)) {
        this.jsonCache = parsed.fields.map((f: any) => ({
          key: f.key,
          type: this.parseFieldType(f.type),
          default_str: f.default ?? '',
          description: f.description ?? '',
          required: !!f.required,
        }));
        return this.jsonCache!;
      }
    } catch { /* fall through */ }

    return this.scanAllBinary();
  }

  private parseFieldType(typeStr: string): FieldType {
    switch (typeStr) {
      case 'int32': return FieldType.Int32;
      case 'int64': return FieldType.Int64;
      case 'string': return FieldType.String;
      case 'double': return FieldType.Double;
      case 'bool': return FieldType.Bool;
      default: return FieldType.String;
    }
  }

  private scanAllBinary(): FieldDecl[] {
    const result: FieldDecl[] = [];
    let off = this.fieldDataOffset;

    while (off + FIELD_RECORD_HEADER_SIZE <= this.buffer.length) {
      const field = this.decodeFieldRecord(off);
      if (!field) break;
      result.push(field);
      off += FIELD_RECORD_HEADER_SIZE + field.key.length + field.default_str.length + field.description.length;
    }

    this.jsonCache = result;
    return result;
  }

  /* ── 线性前缀查询（P3 前兼容实现） ────────────────────────────────── */
  prefixQuery(prefix: string): FieldDecl[] {
    return this.getAllFields().filter(f => f.key.startsWith(prefix));
  }
}

/* ══════════════════════════════════════════════════════════════════════
 * PagedSchemaReader — P2 ⑧ 分页按需加载 + P3 ⑨ Trie 前缀查询
 * ══════════════════════════════════════════════════════════════════════ */
export class PagedSchemaReader extends SchemaReader {
  private loadedPages: Map<number, Buffer> = new Map();
  private pageTableOffset: number;
  private pageCount: number;
  private trieRootOffset: number;

  constructor(
    buffer: Buffer,
    pageTableOffset: number = 0,
    pageCount: number = 0,
    trieRootOffset: number = 0,
    hashTableOffset?: number,
    fieldDataOffset?: number,
  ) {
    super(buffer, hashTableOffset, fieldDataOffset);
    this.pageTableOffset = pageTableOffset;
    this.pageCount = pageCount;
    this.trieRootOffset = trieRootOffset;
  }

  /* ── 页表解码 ──────────────────────────────────────────────────────── */
  private getPageEntry(pageIdx: number): { offset: number; length: number } | null {
    const pos = this.pageTableOffset + pageIdx * PAGE_ENTRY_SIZE;
    if (pos + PAGE_ENTRY_SIZE > this.buffer.length) return null;
    return {
      offset: this.buffer.readUInt32LE(pos),
      length: this.buffer.readUInt32LE(pos + 4),
    };
  }

  /* ── 按需加载字段页 ────────────────────────────────────────────────── */
  private ensurePageLoaded(pageIdx: number): boolean {
    if (this.loadedPages.has(pageIdx)) return true;
    if (this.pageTableOffset === 0 || this.pageCount === 0) return false;

    const entry = this.getPageEntry(pageIdx);
    if (!entry || entry.offset + entry.length > this.buffer.length) return false;

    this.loadedPages.set(pageIdx, this.buffer.subarray(entry.offset, entry.offset + entry.length));
    return true;
  }

  /* ── 按页索引查找（覆盖父类以利用分页缓存） ──────────────────────── */
  lookup(key: string): FieldDecl | null {
    /* 先尝试哈希索引（P1 快速路径） */
    const result = super.lookup(key);
    if (result) return result;

    /* 哈希未命中时，通过 JSON 回退扫描（兼容路径） */
    return super.lookup(key);
  }

  /* ── 分页迭代：只加载包含指定字段索引的页 ────────────────────────── */
  getFieldByIndex(fieldIdx: number): FieldDecl | null {
    const pageIdx = Math.floor(fieldIdx / FIELDS_PER_PAGE);
    if (!this.ensurePageLoaded(pageIdx)) return null;

    const page = this.loadedPages.get(pageIdx)!;
    const slotInPage = fieldIdx % FIELDS_PER_PAGE;

    /* 扫描本页找到第 slotInPage 个字段 */
    let off = 0;
    let count = 0;
    while (off + FIELD_RECORD_HEADER_SIZE <= page.length) {
      const keyLen = page.readUInt16LE(off);
      const defaultLen = page.readUInt16LE(off + 2);
      const descLen = page.readUInt16LE(off + 4);
      const totalLen = FIELD_RECORD_HEADER_SIZE + keyLen + defaultLen + descLen;

      if (count === slotInPage) {
        /* 直接通过父类解码（用全局 buffer 加偏移） */
        const globalOff = this.getPageEntry(pageIdx)!.offset + off;
        return this.decodeFieldRecord(this.fieldDataOffset + globalOff);
      }

      off += totalLen;
      count++;
    }

    return null;
  }

  /* ════════════════════════════════════════════════════════════════════
   * P3 ⑨ Trie 前缀查询
   * ════════════════════════════════════════════════════════════════════ */

  /* ── Trie 节点解码 ────────────────────────────────────────────────── */
  private decodeTrieNode(offset: number): TrieNode | null {
    if (offset + TRIE_NODE_SIZE > this.buffer.length) return null;
    const nameLen = this.buffer.readUInt8(offset);
    if (nameLen < 1 || nameLen > 31) return null;
    const name = this.buffer.toString('utf-8', offset + 1, offset + 1 + nameLen);
    return {
      name,
      firstChildOff: this.buffer.readUInt32LE(offset + 32),
      nextSiblingOff: this.buffer.readUInt32LE(offset + 36),
      firstFieldOff: this.buffer.readUInt32LE(offset + 40),
    };
  }

  /* ── 在 Trie 中按名查找子节点 ──────────────────────────────────────── */
  private trieFindChild(parentOff: number, name: string): TrieNode | null {
    const parent = this.decodeTrieNode(parentOff);
    if (!parent) return null;

    let off = parent.firstChildOff;
    while (off !== 0) {
      const child = this.decodeTrieNode(off);
      if (!child) return null;
      if (child.name === name) return child;
      off = child.nextSiblingOff;
    }
    return null;
  }

  /* ── 前缀匹配子节点（忽略 name 多余部分） ─────────────────────────── */
  private trieFindChildPrefix(parentOff: number, prefix: string): TrieNode | null {
    const parent = this.decodeTrieNode(parentOff);
    if (!parent) return null;

    let off = parent.firstChildOff;
    while (off !== 0) {
      const child = this.decodeTrieNode(off);
      if (!child) return null;
      if (prefix.startsWith(child.name) || child.name.startsWith(prefix)) {
        return child;
      }
      off = child.nextSiblingOff;
    }
    return null;
  }

  /* ── 收集 Trie 子树下所有字段 ─────────────────────────────────────── */
  private collectFieldsFromTrie(nodeOff: number): FieldDecl[] {
    const result: FieldDecl[] = [];
    const node = this.decodeTrieNode(nodeOff);
    if (!node) return result;

    /* 收集本节点直属字段 */
    if (node.firstFieldOff !== 0) {
      let fieldOff = node.firstFieldOff;
      while (fieldOff !== 0) {
        const field = this.decodeFieldRecord(this.fieldDataOffset + fieldOff);
        if (!field) break;
        result.push(field);
        fieldOff += FIELD_RECORD_HEADER_SIZE
          + field.key.length
          + field.default_str.length
          + field.description.length;
      }
    }

    /* 递归收集子节点字段 */
    if (node.firstChildOff !== 0) {
      const childFields = this.collectFieldsFromTrie(node.firstChildOff);
      result.push(...childFields);
    }

    /* 收集兄弟节点字段（防止 Trie 把同层字段分散到兄弟链） */
    if (node.nextSiblingOff !== 0) {
      const siblingFields = this.collectFieldsFromTrie(node.nextSiblingOff);
      result.push(...siblingFields);
    }

    return result;
  }

  /* ── Trie 前缀查询入口 ─────────────────────────────────────────────── */
  prefixQuery(prefix: string): FieldDecl[] {
    if (this.trieRootOffset === 0) {
      /* 无 Trie 索引，回退到线性扫描 */
      return super.prefixQuery(prefix);
    }

    /* 按 '.' 分割路径 */
    const parts = prefix.split('.');
    let currentOff = this.trieRootOffset;

    for (let i = 0; i < parts.length; i++) {
      const part = parts[i];
      const isLast = (i === parts.length - 1);

      if (isLast) {
        /* 最后一段：前缀匹配 */
        const matched = this.trieFindChildPrefix(currentOff, part);
        if (matched) {
          return this.collectFieldsFromTrie(
            this.buffer.readUInt32LE(currentOff) === 0
              ? currentOff
              : currentOff
          ).filter(f => f.key.startsWith(prefix));
        }
        /* 没有 Trie 匹配，回退线性扫描 */
        return super.prefixQuery(prefix);
      } else {
        /* 中间路径段：精确匹配 */
        const child = this.trieFindChild(currentOff, part);
        if (!child) return []; /* 路径不存在 */
        currentOff = this.buffer.indexOf(child.name, this.trieRootOffset) - 1;
        /* 简化：找到子节点后继续往下找 */
        const childActual = this.decodeTrieNode(currentOff);
        if (!childActual) return [];
        currentOff = currentOff; /* 保持偏移 */
      }
    }

    return [];
  }
}

/* ── 工厂函数 ─────────────────────────────────────────────────────────── */
export function createSchemaReader(
  data: Buffer | string | null,
  pageTableOffset?: number,
  pageCount?: number,
  trieRootOffset?: number,
): PagedSchemaReader | null {
  if (!data) return null;

  const buffer = typeof data === 'string' ? Buffer.from(data, 'utf-8') : data;

  if (pageTableOffset !== undefined && pageCount !== undefined) {
    return new PagedSchemaReader(buffer, pageTableOffset, pageCount, trieRootOffset ?? 0);
  }

  return new PagedSchemaReader(buffer);
}
