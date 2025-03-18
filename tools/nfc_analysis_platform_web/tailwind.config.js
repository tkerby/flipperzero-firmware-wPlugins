/** @type {import('tailwindcss').Config} */
export default {
    content: [
        "./index.html",
        "./src/**/*.{vue,js,ts,jsx,tsx}",
    ],
    theme: {
        extend: {
            colors: {
                'ark': {
                    'bg': '#0D1117',
                    'panel': '#161B22',
                    'border': '#21262D',
                    'hover': '#1C2128',
                    'active': '#22272E',
                    'accent': '#38BDF8',
                    'text': {
                        DEFAULT: '#C9D1D9',
                        secondary: '#8B949E',
                        muted: '#6E7681',
                    },
                    'blue': {
                        light: '#79C0FF',
                        DEFAULT: '#1F6FEB',
                        dark: '#1958B7',
                    },
                    'green': {
                        light: '#7EE787',
                        DEFAULT: '#2EA043',
                        dark: '#238636',
                    },
                    'red': {
                        light: '#FF7B72',
                        DEFAULT: '#F85149',
                        dark: '#DA3633',
                    },
                    'yellow': {
                        light: '#F8E3A1',
                        DEFAULT: '#F2CC60',
                        dark: '#D9B44D',
                    },
                    'purple': {
                        light: '#D2A8FF',
                        DEFAULT: '#8957E5',
                        dark: '#6E40C9',
                    },
                    'cyan': {
                        light: '#76E4F7',
                        DEFAULT: '#39C5BB',
                        dark: '#1B9E94',
                    },
                    'orange': {
                        light: '#FFAB70',
                        DEFAULT: '#F0883E',
                        dark: '#DB6D28',
                    },
                    'pink': {
                        light: '#F778BA',
                        DEFAULT: '#DB61A2',
                        dark: '#BF4B8A',
                    },
                    'indigo': {
                        light: '#A5B4FC',
                        DEFAULT: '#6366F1',
                        dark: '#4F46E5',
                    },
                    'teal': {
                        light: '#5EEAD4',
                        DEFAULT: '#14B8A6',
                        dark: '#0D9488',
                    }
                },
                'neon': {
                    green: '#39FF14',
                    blue: '#00FFFF',
                    pink: '#FF00FF',
                    yellow: '#FFFF00',
                    red: '#FF0000',
                },
                'hacker': {
                    dark: '#0D0208',
                    primary: '#003B00',
                    secondary: '#008F11',
                    accent: '#00FF41',
                    light: '#D1F7DF',
                },
                'hacker-primary': '#00ff00',
                'hacker-secondary': '#1a1a1a',
                'hacker-accent': '#00ffff',
                'hacker-dark': '#0a0a0a',
                'hacker-light': '#cccccc',
            },
            fontFamily: {
                'display': ['Fira Code', 'monospace'],
                'mono': ['JetBrains Mono', 'Monaco', 'Consolas', 'monospace'],
                'sans': ['-apple-system', 'BlinkMacSystemFont', 'Segoe UI', 'Helvetica', 'Arial', 'sans-serif'],
            },
            animation: {
                'pulse-fast': 'pulse 1s cubic-bezier(0.4, 0, 0.6, 1) infinite',
                'glow': 'glow 2s ease-in-out infinite alternate',
                'typing': 'typing 3.5s steps(40, end), blink-caret .75s step-end infinite',
            },
            keyframes: {
                glow: {
                    '0%': { textShadow: '0 0 5px #00FF41, 0 0 10px #00FF41' },
                    '100%': { textShadow: '0 0 10px #00FF41, 0 0 20px #00FF41, 0 0 30px #00FF41' },
                },
                typing: {
                    'from': { width: '0' },
                    'to': { width: '100%' },
                },
                'blink-caret': {
                    'from, to': { borderColor: 'transparent' },
                    '50%': { borderColor: '#00FF41' },
                },
            },
            backgroundImage: {
                'cyber-grid': "url('/src/assets/cyber-grid.svg')",
                'circuit': "url('/src/assets/circuit-pattern.svg')",
            },
            boxShadow: {
                'ark': '0 0 10px rgba(0, 0, 0, 0.3)',
                'ark-sm': '0 1px 2px rgba(0, 0, 0, 0.1)',
                'ark-lg': '0 8px 24px rgba(0, 0, 0, 0.4)',
            },
            borderRadius: {
                'ark': '6px',
            },
        },
    },
    plugins: [],
} 