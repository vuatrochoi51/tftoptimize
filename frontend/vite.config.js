import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite' // 1. Import công cụ mới

export default defineConfig({
  plugins: [
    react(),
    tailwindcss(), // 2. Bật công cụ lên
  ],
})