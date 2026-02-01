import { defineConfig } from 'vitepress'

export default defineConfig({
  title: "SkyScript",
  description: "Build X-Plane plugins with React & TypeScript",
  base: '/',
  
  head: [
    ['link', { rel: 'icon', href: '/favicon.ico' }]
  ],

  themeConfig: {
    logo: '/logo.svg',
    
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Guide', link: '/guide/quick-start' },
      { text: 'API', link: '/api/' },
      { 
        text: 'GitHub', 
        link: 'https://github.com/your-username/SkyScript' 
      }
    ],

    sidebar: {
      '/guide/': [
        {
          text: 'Getting Started',
          items: [
            { text: 'Introduction', link: '/guide/' },
            { text: 'Quick Start', link: '/guide/quick-start' },
            { text: 'Project Structure', link: '/guide/project-structure' }
          ]
        },
        {
          text: 'Building Apps',
          items: [
            { text: 'Creating Your First App', link: '/guide/first-app' },
            { text: 'Using TypeScript', link: '/guide/typescript' },
            { text: 'Styling', link: '/guide/styling' }
          ]
        }
      ],
      '/api/': [
        {
          text: 'API Reference',
          items: [
            { text: 'Overview', link: '/api/' },
            { text: 'DataRef API', link: '/api/DataRefAPI' },
            { text: 'Scenery API', link: '/api/SceneryAPI' },
            { text: 'Instance API', link: '/api/InstanceAPI' },
            { text: 'Graphics API', link: '/api/GraphicsAPI' }
          ]
        }
      ]
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/your-username/SkyScript' }
    ],

    footer: {
      message: 'Released under the MIT License.',
      copyright: 'Copyright © 2024-present'
    },

    search: {
      provider: 'local'
    }
  }
})
