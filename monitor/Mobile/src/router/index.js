
import Vue from 'vue'
import VueRouter from 'vue-router'

import Index from '@/pages/Index'
import Home from '@/pages/Home'
import Vod from '@/pages/Vod'
import Live from '@/pages/Live'

Vue.use(VueRouter)

export default new VueRouter({
  routes: [
    {
      path: '/',
      name: 'Index',
      component: Index
    },
    {
      path: '/home',
      name: 'Home',
      component: Home
    },
    {
      path: '/vod',
      name: 'Vod',
      component: Vod
    },
    {
      path: '/live',
      name: 'Live',
      component: Live
    }
  ]
})
