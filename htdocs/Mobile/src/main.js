
// 消除点击延迟 => 不打开会影响tab点击事件...
// 必须采用定向绑定，否则会造成Videojs点击两次...
// 需要在743|744加入 else { return false }，否则android无法支持...
const FastClick = require('fastclick')

// 安装图标字库...
// cnpm install font-awesome

// 加载video样式、主题、语言包...
require('video.js/dist/lang/zh-CN.js')
require('video.js/dist/video-js.css')
require('vue-video-player/src/custom-theme.css')
// 加载video-hls直播模块...
require('videojs-contrib-hls/dist/videojs-contrib-hls')

// 加载核心vue模块...
import Vue from 'vue'
import Vuex from 'vuex'
import router from './router'
import VueVideoPlayer from 'vue-video-player'

// 加载vue-video-player...
Vue.use(VueVideoPlayer)

// 加载存储对象...
Vue.use(Vuex)

// 创建一个存储对象...
let store = new Vuex.Store()

// 设置需要全局保存的变量...
store.registerModule('vux', {
  state: {
    isLoading: false,
    direction: 'forward',
    fastClick: FastClick,
    // build 发行版本需要的信息...
    ajaxUrlPath: '/wxapi.php/',
    ajaxImgPath: '/wxapi/public/images/'
    // dev 本地调试版本需要的信息...
    // ajaxUrlPath: 'http://192.168.1.70/wxapi.php/',
    // ajaxImgPath: 'http://192.168.1.70/wxapi/public/images/'
  },
  mutations: {
    updateLoadingStatus (state, payload) {
      state.isLoading = payload.isLoading
    },
    updateDirection (state, payload) {
      state.direction = payload.direction
    }
  }
})

// 加载App对象...
import App from './App'

// 处理历史记录...
const history = window.sessionStorage
history.clear()
let historyCount = history.getItem('count') * 1 || 0
history.setItem('/', 0)

// 路由跳转前的事件处理...
router.beforeEach(function (to, from, next) {
  // 这里做了全局等待框处理，只要进行页面路由就会出现等待框...
  store.commit('updateLoadingStatus', {isLoading: true})
  const toIndex = history.getItem(to.path)
  const fromIndex = history.getItem(from.path)
  // 这里进行页面切换的方向判断...
  if (toIndex) {
    if (!fromIndex || parseInt(toIndex, 10) > parseInt(fromIndex, 10) || (toIndex === '0' && fromIndex === '0')) {
      store.commit('updateDirection', {direction: 'forward'})
    } else {
      store.commit('updateDirection', {direction: 'reverse'})
    }
  } else {
    ++historyCount
    history.setItem('count', historyCount)
    to.path !== '/' && history.setItem(to.path, historyCount)
    store.commit('updateDirection', {direction: 'forward'})
  }
  // 执行跳转页面...
  next()
})

// 路由跳转后的事件处理 => 关闭等待框交给了各个模块自己去处理，从而实现全局等待框...
/* router.afterEach(function (to) {
  store.commit('updateLoadingStatus', {isLoading: false})
}) */

// 懒加载组件 => 功能强大的易用组件...
// webpack.base.conf.js => symlinks: true
// cnpm install vue-lazyload --save-dev
import VueLazyload from 'vue-lazyload'
Vue.use(VueLazyload, {
  throttleWait: 300,
  observer: false,
  attempt: 2
})

// cnpm install vue-resource --save-dev
// 使用 vue-resource => 官方已经不支持了...
// import VueResource from 'vue-resource'
// Vue.use(VueResource)
// Vue.http.options.emulateJSON = true

// cnpm install axios vue-axios --save-dev
// 官方用法 => axios + vue-axios...
import axios from 'axios'
import VueAxios from 'vue-axios'
Vue.use(VueAxios, axios)

// 简洁用法 => axios...
// import axios from 'axios'
// Vue.prototype.$http = axios

// 阻止在生产环境启动时产生提示...
Vue.config.productionTip = false
// 允许开启调试工具...
Vue.config.devtools = true

// 这行注释必须加，否则报错...
/* eslint-disable no-new */
new Vue({
  store,  // 全局变量存储对象...
  router, // 路由对象，会自动加载根路径内容...
  el: '#app', // 关联替换 index.html 里的内容...
  template: '<App/>', // 关联到 App.vue
  components: { App } // 关联到 App.vue
})
