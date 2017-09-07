// 消除点击延迟 => 不打开会影响tab点击事件...
const FastClick = require('fastclick')
FastClick.attach(document.body)

// 安装图标字库...
// cnpm install font-awesome

// 加载核心vue模块...
import Vue from 'vue'
import App from './App'

// cnpm install vue-lazyload --save-dev
import VueLazyload from 'vue-lazyload'
Vue.use(VueLazyload, {
  throttleWait: 500,
  observer: true,
  attempt: 1
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
  el: '#app', // 关联替换 index.html 里的内容...
  template: '<App/>', // 关联到 App.vue
  components: { App } // 关联到 App.vue
})
