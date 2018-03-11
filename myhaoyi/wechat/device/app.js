//app.js
App({
  onLaunch: function (options) {
    // 打印引导参数...
    console.log(options)
    // 获取系统信息同步接口...
    this.globalData.m_sysInfo = wx.getSystemInfoSync()
    // 展示本地存储能力
    /*var logs = wx.getStorageSync('logs') || []
    logs.unshift(Date.now())
    wx.setStorageSync('logs', logs)*/
  },
  doShowLoading: function() {
    wx.showLoading({ title: '加载中' })
  },
  globalData: {
    m_urlPrev: 'https://myhaoyi.com/wxapi.php/',
    m_nBindGatherID: -1,
    m_bLoadGather: false,
    m_userInfo: null,
    m_sysInfo: null,
    m_nUserID: 0
  }
})