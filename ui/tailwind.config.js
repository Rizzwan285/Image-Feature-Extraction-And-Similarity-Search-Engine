/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx}'],
  theme: {
    extend: {
      fontFamily: {
        display: ['"Space Grotesk"', 'system-ui', 'sans-serif'],
        body: ['"Inter"', 'system-ui', 'sans-serif'],
        mono: ['"JetBrains Mono"', 'ui-monospace', 'monospace'],
      },
      colors: {
        ink: {
          950: '#070611',
          900: '#0c0a1d',
          800: '#15122b',
          700: '#1f1b3d',
          600: '#2a2553',
        },
        accent: {
          50:  '#eefcff',
          100: '#d6f7ff',
          200: '#b0eeff',
          300: '#79e1ff',
          400: '#3acaff',
          500: '#13aef0',
          600: '#0789c8',
          700: '#0a6ea0',
          800: '#0e5b82',
          900: '#114c6c',
        },
        magenta: {
          400: '#e879f9',
          500: '#d946ef',
          600: '#c026d3',
        },
      },
      backgroundImage: {
        'grid-faint': 'radial-gradient(circle at 1px 1px, rgba(255,255,255,0.06) 1px, transparent 0)',
      },
      keyframes: {
        'shimmer': {
          '0%': { transform: 'translateX(-100%)' },
          '100%': { transform: 'translateX(100%)' },
        },
        'pulse-slow': {
          '0%, 100%': { opacity: '0.6' },
          '50%': { opacity: '1' },
        },
      },
      animation: {
        'shimmer': 'shimmer 2.5s linear infinite',
        'pulse-slow': 'pulse-slow 3s ease-in-out infinite',
      },
    },
  },
  plugins: [],
}
