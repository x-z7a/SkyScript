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
      { text: 'Guide', link: '/guide/installation' },
      { text: 'Developer', link: '/developer/getting-started' },
    ],

    sidebar: {
      '/guide/': [
        {
          text: 'User Guide',
          items: [
            { text: 'Installation', link: '/guide/installation' },
            { text: 'Installing Apps', link: '/guide/installing-apps' },
          ],
        },
      ],
      '/developer/': [
        {
          text: 'App Development',
          items: [
            { text: 'Getting Started', link: '/developer/getting-started' },
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
