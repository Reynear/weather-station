/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{vue,js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        dark: '#0f1115',
        card: 'rgba(28, 30, 38, 0.7)',
        accent: '#4facfe',
      }
    },
  },
  plugins: [],
}

