import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      'livestyle-engine': path.resolve(__dirname, '../livestyle-engine/src'),
    },
  },
  server: {
    port: 5173,
  },
  test: {
    environment: 'jsdom',
  },
});
