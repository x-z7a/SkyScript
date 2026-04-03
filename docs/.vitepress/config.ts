import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'SkyScript',
  description: 'Browser plugin for X-Plane 12',
  base: '/',

  head: [
    ['link', { rel: 'icon', type: 'image/svg+xml', href: '/logo.svg' }],
  ],

  themeConfig: {
    nav: [
      { text: 'Developer', link: '/developer/getting-started' },
    ],

    sidebar: {
      '/developer/': [
        {
          text: 'Developer Guide',
          items: [
            { text: 'Getting Started', link: '/developer/getting-started' },
            { text: 'C++ Library API', link: '/developer/cpp-api' },
            { text: 'App Manifest', link: '/developer/manifest' },
            { text: 'JavaScript API', link: '/developer/api' },
          ],
        },
      ],
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/x-z7a/skyscript' },
    ],

    footer: {
      message: 'Released under the GPL-3.0 License.',
    },
  },
})
