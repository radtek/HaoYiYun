//app.js
App({
  onLaunch: function (options) {
    // 打印引导参数...
    console.log(options)
    // 展示本地存储能力
    /*var logs = wx.getStorageSync('logs') || []
    logs.unshift(Date.now())
    wx.setStorageSync('logs', logs)*/
  },
  globalData: {
    m_urlPrev: 'https://myhaoyi.com/wxapi.php/',
    m_userInfo: null,
    m_nUserID: 0
  }
})