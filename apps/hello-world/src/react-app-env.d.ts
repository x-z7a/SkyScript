/// <reference types="react-scripts" />

interface SkyscriptXplm {
  getDataref(ref: string): Promise<number | string | number[]>;
  setDataref(ref: string, value: number | string, valueType?: string): Promise<void>;
  executeCommand(command: string): Promise<void>;
}

interface Skyscript {
  xplm: SkyscriptXplm;
  _resolve(id: number, value: any): void;
  _reject(id: number, error: string): void;
  _callbacks: Record<number, { resolve: Function; reject: Function }>;
  _nextId: number;
}

interface Window {
  skyscript: Skyscript;
}
